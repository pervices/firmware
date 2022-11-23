#include "array-utils.h"
#include "comm_manager.h"
#include "uart.h"

#include <unistd.h>

#undef NDEBUG
#include <assert.h>

int main(void)
{
    int fd = -1;

    init_uart_comm(&fd, "/dev/ttyMAX0", 0);


    for(int i = 0; 1; i++)
    {
        char message[512] = { 0 };
        snprintf(message, 512, "The quick brown fox jumps over the lazy dog and fell into a puddle of rubble, next to home sweet home: %d\r\n", i);

        send_uart(fd, (uint8_t*) message, strlen(message));

        char in[512] = {0};
        uint16_t got = 0;
        const int fail = recv_uart(fd, in, &got, sizeof(in));

        if(fail)
        {
            puts("Timeout ERROR....");
            exit(1);
        }
        else printf("%d: %d: [%d]: %s", i, fail, got, in); // Newline included.
    }
}
