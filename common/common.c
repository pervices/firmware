//
// Copyright 2014 Per Vices Corporation
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

// Exposes pthread_condattr_setclock and CLOCK_MONOTONIC
#define _GNU_SOURCE

#include "common.h"

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>

int verbose;

//
// Lazy printing
//
// Each thread that calls PRINT gets its own single-producer/single-consumer
// ring buffer. The thread (the producer) formats its message into its ring;
// one background thread (the logger) reads every ring and writes the messages
// to stdout/stderr. Because each ring has one writer and one reader, messages
// from a thread stay in order and the hot path takes no locks.
//
// ERROR messages also go through the ring, so they can't overtake the thread's
// earlier messages, but the producer then waits until the logger has written
// them.
//
// If a producer waits on the logger for more than LOGGER_TIMEOUT_S (ring full,
// or an ERROR not yet written), lazy printing is switched off for the rest of
// the process. That producer prints whatever is still in its ring itself,
// prefixed with FALLBACK_PREFIX. The logger keeps draining too, so those
// messages may appear twice; that is accepted.
//

// Size of each thread's ring in bytes. Must be a power of 2
#define RING_SIZE (128 * 1024)
#define RING_MASK (RING_SIZE - 1)

// Longest message kept; longer messages are truncated.
// Must stay well under RING_SIZE / 2 so a record still fits after wrap padding.
#define MAX_MSG_LEN (32 * 1024)

// Messages shorter than this are formatted into a stack buffer and copied into
// the ring. Longer ones are formatted a second time, directly into the ring.
#define SMALL_MSG_LEN 256

// How long a producer waits for the logger before lazy printing is disabled
#define LOGGER_TIMEOUT_S 5

// How long an idle logger sleeps before rechecking the rings. Producers wake
// it explicitly, so this is only a safety net.
#define LOGGER_IDLE_NS (100L * 1000 * 1000)

// How often the exit handler checks whether the logger has drained the rings
#define EXIT_POLL_NS (1L * 1000 * 1000)

#define FALLBACK_PREFIX "LOGGING FALLBACK: "

#define NS_PER_S 1000000000L

// Record flags
#define RECORD_STDERR (1u << 0) // write to stderr instead of stdout
#define RECORD_PAD (1u << 1)    // filler to the end of the buffer; the next record starts at offset 0

// Header in front of every record in a ring.
// The message bytes follow it, then a NUL, then padding to a multiple of 8.
struct record_header {
    uint32_t len;   // message length in bytes, not counting the header or NUL
    uint32_t flags; // RECORD_*
};

// Fields written by one thread and polled by another get their own cache line,
// so the producer and logger don't slow each other down
#define CACHE_LINE 64

// Indices are free-running and wrap at 2^32; the byte offset in buf is
// (index & RING_MASK) and the number of unread bytes is (write_idx - read_idx).
struct ring {
    // One past the last published byte. Only the producer writes it
    _Alignas(CACHE_LINE) _Atomic uint32_t write_idx;

    // Next byte the logger will read. Only the logger writes it, and only after
    // it has finished with the bytes before it
    _Alignas(CACHE_LINE) _Atomic uint32_t read_idx;

    // Set while the producer is blocked waiting for read_idx to advance
    _Alignas(CACHE_LINE) atomic_bool producer_waiting;

    // Set when the owning thread exits. The logger frees the ring once it is empty
    atomic_bool dead;

    // Lets the logger wake a waiting producer
    pthread_mutex_t lock;
    pthread_cond_t cond; // uses CLOCK_MONOTONIC

    // Next ring in ring_list. Changed only under rings_lock. Producers only
    // prepend; only the logger unlinks, so the logger can walk the list unlocked
    struct ring *next;

    _Alignas(CACHE_LINE) char buf[RING_SIZE];
};

// Set once lazy printing is off for the rest of the process. Never cleared
static atomic_bool lazy_disabled = false;

static pthread_once_t init_once = PTHREAD_ONCE_INIT;

// Its destructor marks a thread's ring dead when the thread exits
static pthread_key_t ring_key;

// Every live ring, newest first
static _Atomic(struct ring *) ring_list = NULL;
static pthread_mutex_t rings_lock = PTHREAD_MUTEX_INITIALIZER;

// Lets producers wake an idle logger
static atomic_bool logger_sleeping = false;
static pthread_mutex_t logger_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t logger_cond; // initialized in lazy_init to use CLOCK_MONOTONIC

// This thread's ring, or NULL before its first PRINT
static _Thread_local struct ring *tls_ring = NULL;

//
// Helpers
//

// Bytes a record with a message of len bytes takes in the ring
static uint32_t record_size(uint32_t len) {
    return (uint32_t)((sizeof(struct record_header) + len + 1 + 7) & ~(size_t)7);
}

// True once read_idx has reached target. Indices wrap, so compare the signed distance
static bool reached(uint32_t read_idx, uint32_t target) {
    return (int32_t)(read_idx - target) >= 0;
}

static bool ring_is_empty(struct ring *r) {
    return atomic_load(&r->read_idx) == atomic_load(&r->write_idx);
}

// Monotonic time ns nanoseconds from now, for pthread_cond_timedwait
static struct timespec deadline_after_ns(long ns) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ts.tv_sec += ns / NS_PER_S;
    ts.tv_nsec += ns % NS_PER_S;
    if (ts.tv_nsec >= NS_PER_S) {
        ts.tv_sec++;
        ts.tv_nsec -= NS_PER_S;
    }
    return ts;
}

static bool past(const struct timespec *deadline) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec > deadline->tv_sec ||
           (now.tv_sec == deadline->tv_sec && now.tv_nsec >= deadline->tv_nsec);
}

// Condition variables default to CLOCK_REALTIME, which can jump; use CLOCK_MONOTONIC
static int cond_init_monotonic(pthread_cond_t *cond) {
    pthread_condattr_t attr;
    int r = pthread_condattr_init(&attr);
    if (r != 0) {
        return r;
    }
    r = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    if (r == 0) {
        r = pthread_cond_init(cond, &attr);
    }
    pthread_condattr_destroy(&attr);
    return r;
}

// Writes an error in the same format as PRINT(ERROR, ...), without going through a ring
static void print_error_now(const char *func, const char *msg) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    fprintf(stderr, "[%6ld.%03ld] ERROR: %s(): %s", (long)ts.tv_sec,
            ts.tv_nsec / 1000000L, func, msg);
    fflush(stderr);
}

// Wakes the logger if it is idle. Called after publishing a record
static void wake_logger(void) {
    // Producer: store write_idx, then load logger_sleeping.
    // Logger: store logger_sleeping, then load write_idx.
    // Both are seq_cst, so at least one side sees the other's store and no
    // wakeup is lost. Holding logger_lock to signal means the signal can't land
    // between the logger's check and its wait.
    if (atomic_load(&logger_sleeping)) {
        pthread_mutex_lock(&logger_lock);
        pthread_cond_signal(&logger_cond);
        pthread_mutex_unlock(&logger_lock);
    }
}

//
// Ring lifetime
//

static struct ring *ring_create(void) {
    struct ring *r = aligned_alloc(_Alignof(struct ring), sizeof(struct ring));
    if (r == NULL) {
        return NULL;
    }
    atomic_init(&r->write_idx, 0);
    atomic_init(&r->read_idx, 0);
    atomic_init(&r->producer_waiting, false);
    atomic_init(&r->dead, false);
    r->next = NULL;
    if (pthread_mutex_init(&r->lock, NULL) != 0) {
        free(r);
        return NULL;
    }
    if (cond_init_monotonic(&r->cond) != 0) {
        pthread_mutex_destroy(&r->lock);
        free(r);
        return NULL;
    }
    return r;
}

static void ring_destroy(struct ring *r) {
    pthread_cond_destroy(&r->cond);
    pthread_mutex_destroy(&r->lock);
    free(r);
}

static void ring_register(struct ring *r) {
    pthread_mutex_lock(&rings_lock);
    r->next = atomic_load(&ring_list);
    atomic_store(&ring_list, r);
    pthread_mutex_unlock(&rings_lock);
}

// Only the logger calls this
static void ring_unlink(struct ring *r) {
    pthread_mutex_lock(&rings_lock);
    struct ring *head = atomic_load(&ring_list);
    if (head == r) {
        atomic_store(&ring_list, r->next);
    } else {
        struct ring *prev = head;
        while (prev->next != r) {
            prev = prev->next;
        }
        prev->next = r->next;
    }
    pthread_mutex_unlock(&rings_lock);
}

// Runs in the exiting thread. The ring itself is heap memory that outlives the
// thread; the logger frees it once it has written everything left in it.
static void ring_key_destructor(void *value) {
    struct ring *r = value;
    // A PRINT from a later destructor in this thread gets a fresh ring
    tls_ring = NULL;
    atomic_store(&r->dead, true);
    wake_logger();
}

//
// Logger thread
//

// Writes every published record in r. Returns true if there was anything to write.
static bool drain_ring(struct ring *r) {
    // Only this thread writes read_idx, so a relaxed load is enough
    uint32_t rd = atomic_load_explicit(&r->read_idx, memory_order_relaxed);
    // Acquire: the record bytes the producer wrote before publishing are visible
    uint32_t wr = atomic_load_explicit(&r->write_idx, memory_order_acquire);

    if (rd == wr) {
        return false;
    }

    while (rd != wr) {
        uint32_t pos = rd & RING_MASK;
        struct record_header h;
        memcpy(&h, r->buf + pos, sizeof(h));

        if (h.flags & RECORD_PAD) {
            rd += RING_SIZE - pos;
        } else {
            const char *msg = r->buf + pos + sizeof(h);
            if (h.flags & RECORD_STDERR) {
                // Earlier stdout messages from this thread must come out first
                fflush(stdout);
                fwrite(msg, 1, h.len, stderr);
                // The producer may be waiting for this ERROR to be written, so
                // flush before advancing read_idx past it
                fflush(stderr);
            } else {
                fwrite(msg, 1, h.len, stdout);
            }
            rd += record_size(h.len);
        }

        // Seq_cst (which includes release): we are done with these bytes, the
        // producer may reuse them. Pairs with the producer's store of
        // producer_waiting followed by its load of read_idx, so a waiting
        // producer is never missed.
        atomic_store(&r->read_idx, rd);
        if (atomic_load(&r->producer_waiting)) {
            pthread_mutex_lock(&r->lock);
            pthread_cond_broadcast(&r->cond);
            pthread_mutex_unlock(&r->lock);
        }
    }
    return true;
}

// True if any ring has unread records or is waiting to be freed
static bool rings_have_work(void) {
    for (struct ring *r = atomic_load(&ring_list); r != NULL; r = r->next) {
        if (atomic_load(&r->dead) || !ring_is_empty(r)) {
            return true;
        }
    }
    return false;
}

static void logger_sleep(void) {
    struct timespec deadline = deadline_after_ns(LOGGER_IDLE_NS);
    pthread_mutex_lock(&logger_lock);
    // Set the flag before the final check; see wake_logger
    atomic_store(&logger_sleeping, true);
    if (!rings_have_work()) {
        pthread_cond_timedwait(&logger_cond, &logger_lock, &deadline);
    }
    atomic_store(&logger_sleeping, false);
    pthread_mutex_unlock(&logger_lock);
}

static void *logger_main(void *unused) {
    (void)unused;
    for (;;) {
        bool worked = false;

        struct ring *r = atomic_load(&ring_list);
        while (r != NULL) {
            // Read before r might be freed below
            struct ring *next = r->next;

            // Load dead before draining: once its thread has exited, no more
            // records can be published, so an empty ring can be freed safely
            bool dead = atomic_load(&r->dead);

            if (drain_ring(r)) {
                worked = true;
            }
            if (dead && ring_is_empty(r)) {
                ring_unlink(r);
                ring_destroy(r);
                worked = true;
            }
            r = next;
        }

        // stdout may be fully buffered (pipe, journal); push each pass out
        fflush(stdout);

        if (!worked) {
            logger_sleep();
        }
    }
    return NULL;
}

//
// Process start and exit
//

// Runs at exit(): waits up to LOGGER_TIMEOUT_S for the logger to drain every
// ring, then makes later prints synchronous so atexit handlers that run after
// this one aren't lost
static void flush_at_exit(void) {
    struct timespec deadline = deadline_after_ns(LOGGER_TIMEOUT_S * NS_PER_S);
    const struct timespec poll = { .tv_sec = 0, .tv_nsec = EXIT_POLL_NS };

    for (;;) {
        bool empty = true;
        // Hold rings_lock so the logger can't free a ring while we look at it
        pthread_mutex_lock(&rings_lock);
        for (struct ring *r = atomic_load(&ring_list); r != NULL; r = r->next) {
            if (!ring_is_empty(r)) {
                empty = false;
                break;
            }
        }
        pthread_mutex_unlock(&rings_lock);

        if (empty || past(&deadline)) {
            break;
        }
        wake_logger();
        nanosleep(&poll, NULL);
    }

    atomic_store(&lazy_disabled, true);
}

static void lazy_init(void) {
    if (cond_init_monotonic(&logger_cond) != 0 ||
        pthread_key_create(&ring_key, ring_key_destructor) != 0) {
        atomic_store(&lazy_disabled, true);
        print_error_now(__func__, "Failed to initialize the logger, lazy printing disabled\n");
        return;
    }

    // The logger blocks every signal, so signals keep going to the program's
    // own threads as they did before the logger existed. The new thread
    // inherits the mask in effect when it is created.
    sigset_t all, old;
    sigfillset(&all);
    pthread_sigmask(SIG_SETMASK, &all, &old);
    pthread_t thread;
    int r = pthread_create(&thread, NULL, logger_main, NULL);
    pthread_sigmask(SIG_SETMASK, &old, NULL);

    if (r != 0) {
        atomic_store(&lazy_disabled, true);
        print_error_now(__func__, "Failed to start the logger, lazy printing disabled\n");
        return;
    }
    pthread_detach(thread);
    atexit(flush_at_exit);
}

// Returns this thread's ring, creating it on first use.
// Returns NULL if lazy printing is off; the caller then prints synchronously.
static struct ring *get_ring(void) {
    if (tls_ring != NULL) {
        return tls_ring;
    }

    // Checked before pthread_once so programs that call print_lazy_disable()
    // first never start the logger
    if (atomic_load(&lazy_disabled)) {
        return NULL;
    }
    pthread_once(&init_once, lazy_init);
    if (atomic_load(&lazy_disabled)) {
        return NULL;
    }

    struct ring *r = ring_create();
    if (r == NULL) {
        return NULL;
    }
    if (pthread_setspecific(ring_key, r) != 0) {
        ring_destroy(r);
        return NULL;
    }
    ring_register(r);
    tls_ring = r;
    return r;
}

//
// Producer
//

// Waits until the logger's read_idx reaches target, for up to LOGGER_TIMEOUT_S.
// Returns false on timeout.
static bool wait_for_logger(struct ring *r, uint32_t target) {
    if (reached(atomic_load(&r->read_idx), target)) {
        return true;
    }

    struct timespec deadline = deadline_after_ns(LOGGER_TIMEOUT_S * NS_PER_S);
    bool ok;

    pthread_mutex_lock(&r->lock);
    // Set the flag before checking read_idx; see drain_ring
    atomic_store(&r->producer_waiting, true);
    for (;;) {
        if (reached(atomic_load(&r->read_idx), target)) {
            ok = true;
            break;
        }
        // Spurious wakeups and broadcasts that didn't free enough space just loop
        if (pthread_cond_timedwait(&r->cond, &r->lock, &deadline) == ETIMEDOUT) {
            ok = reached(atomic_load(&r->read_idx), target);
            break;
        }
    }
    atomic_store(&r->producer_waiting, false);
    pthread_mutex_unlock(&r->lock);

    return ok;
}

// Prints every record in r the logger hasn't consumed yet, with FALLBACK_PREFIX.
// Safe without locking: this thread is r's only writer and has stopped writing
// to it, the logger never modifies record bytes, and a live thread's ring is
// never freed. The logger may print the same records too.
static void print_ring_fallback(struct ring *r) {
    uint32_t rd = atomic_load(&r->read_idx);
    // Our own index; no other thread writes it
    uint32_t wr = atomic_load_explicit(&r->write_idx, memory_order_relaxed);

    while (rd != wr) {
        uint32_t pos = rd & RING_MASK;
        struct record_header h;
        memcpy(&h, r->buf + pos, sizeof(h));

        if (h.flags & RECORD_PAD) {
            rd += RING_SIZE - pos;
            continue;
        }

        FILE *o = (h.flags & RECORD_STDERR) ? stderr : stdout;
        fputs(FALLBACK_PREFIX, o);
        fwrite(r->buf + pos + sizeof(h), 1, h.len, o);
        fflush(o);
        rd += record_size(h.len);
    }
}

// Called when this thread has waited too long for the logger
static void lazy_timeout(struct ring *r) {
    bool expected = false;
    // Only the first thread to time out prints the warning
    if (atomic_compare_exchange_strong(&lazy_disabled, &expected, true)) {
        print_error_now(__func__, "Logger is taking too long, disabling lazy printing. "
                                  "Some messages may arrive late or out of order.\n");
    }
    print_ring_fallback(r);
}

// Queues one message in r. args has not been used yet.
static void lazy_print(struct ring *r, print_t priority, FILE *o,
                       const char *format, va_list args) {
    // Format into a small stack buffer first; this also gives the full length
    char small[SMALL_MSG_LEN];
    va_list args_copy;
    va_copy(args_copy, args);
    int n = vsnprintf(small, sizeof(small), format, args_copy);
    va_end(args_copy);
    if (n < 0) {
        // Invalid format; the synchronous path would print nothing either
        return;
    }

    uint32_t len = (uint32_t)n > MAX_MSG_LEN ? MAX_MSG_LEN : (uint32_t)n;
    uint32_t need = record_size(len);

    uint32_t wr = atomic_load_explicit(&r->write_idx, memory_order_relaxed);
    uint32_t pos = wr & RING_MASK;

    // A record never wraps around the end of the buffer; if it doesn't fit
    // before the end, pad to the end and start it at offset 0
    uint32_t pad = (RING_SIZE - pos < need) ? RING_SIZE - pos : 0;
    uint32_t total = pad + need;

    // Free space is RING_SIZE - (wr - read_idx), so there is room for total
    // bytes once read_idx reaches wr + total - RING_SIZE
    if (!wait_for_logger(r, wr + total - RING_SIZE)) {
        lazy_timeout(r);
        // This message never reached the ring, so print it without the prefix
        vfprintf(o, format, args);
        fflush(o);
        return;
    }

    if (pad != 0) {
        struct record_header ph = { .len = 0, .flags = RECORD_PAD };
        memcpy(r->buf + pos, &ph, sizeof(ph));
        pos = 0;
    }

    struct record_header h = {
        .len = len,
        .flags = (priority == ERROR) ? RECORD_STDERR : 0,
    };
    memcpy(r->buf + pos, &h, sizeof(h));

    char *msg = r->buf + pos + sizeof(h);
    if ((size_t)n < sizeof(small)) {
        memcpy(msg, small, len + 1);
    } else {
        // Too long for the stack buffer; format again straight into the ring.
        // Truncates to len bytes if the message exceeds MAX_MSG_LEN.
        vsnprintf(msg, len + 1, format, args);
        if ((uint32_t)n > len) {
            // Keep the line ending so the next message starts on its own line
            msg[len - 1] = '\n';
        }
    }

    uint32_t end = wr + total;
    // Seq_cst (which includes release): publishes the record to the logger,
    // and orders this store before wake_logger's load of logger_sleeping
    atomic_store(&r->write_idx, end);
    wake_logger();

    if (priority == ERROR) {
        // Return only once the logger has written this record
        if (!wait_for_logger(r, end)) {
            // Prints the ERROR too, since it is still in the ring
            lazy_timeout(r);
        }
    }
}

//
// Public interface
//

void PRINT_WRAPPER(print_t priority, const char *format, ...) {
    // PRINT already checks this; repeated for direct callers
    if (!print_enabled(priority)) {
        return;
    }

    FILE *o = (priority == ERROR) ? stderr : stdout;

    va_list args;
    va_start(args, format);

    struct ring *r = atomic_load(&lazy_disabled) ? NULL : get_ring();
    if (r != NULL) {
        lazy_print(r, priority, o, format, args);
    } else {
        // Lazy printing is off: the original synchronous behaviour
        vfprintf(o, format, args);
        fflush(o);
    }

    va_end(args);
}

void print_lazy_disable(void) {
    atomic_store(&lazy_disabled, true);
}
