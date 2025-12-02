#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

// For flock
#include <sys/file.h>

// Crimson + Calamine Paths
char PATH_TNG_RX[50] = "/dev/ttycrimson-rx";
char PATH_TNG_TX[50] = "/dev/ttycrimson-tx";
char PATH_TNG_TIME[50] = "/dev/ttycrimson-time";
char PATH_TNG_GPIO[50] = "/dev/ttycrimson-gpio";

// Crimson + Calamine Hexfile Names
char HEX_TNG_RX[50] = "vaunt-rx.hex";
char HEX_TNG_TX[50] = "vaunt-tx.hex";
char HEX_TNG_TIME[50] = "vaunt-synth.hex";
char HEX_TNG_AVERY_RX[50] = "avery-rx.hex";
char HEX_TNG_AVERY_CTRL[50] = "avery-ctrl.hex";
char HEX_TNG_FULLTX[50] = "vaunt-fulltx.hex";

// Tate Paths
char PATH_TATE_RX[50] = "/dev/ttycyan-rfe-";
char PATH_TATE_TX[50] = "/dev/ttycyan-rfe-";
char PATH_TATE_TIME[50] = "/dev/ttycyan-time";

// Tate Hexfile Names
char HEX_TATE_RX[50] = "tate-rx.hex";
char HEX_TATE_RX3[50] = "tate-rx3.hex";
char HEX_TATE_BBRX[50] = "tate-bbrx.hex";
char HEX_TATE_TX[50] = "tate-tx.hex";
char HEX_TATE_BBTX[50] = "tate-bbtx.hex";
char HEX_TATE_TX3[50] = "tate-tx3.hex";
char HEX_TATE_TIME[50] = "tate-synth.hex";
char HEX_TATE_TIME3[50] = "tate-synth3.hex";
char HEX_TATE_TIME1ON3[50] = "tate-synth1on3.hex";

// Lily Paths
char PATH_LILY_RX[50] = "/dev/ttychestnut-rfe-";
char PATH_LILY_TX[50] = "/dev/ttychestnut-rfe-";
char PATH_LILY_TIME[50] = "/dev/ttychestnut-time";

// Lily Hexfile Names
char HEX_LILY_RX[50] = "lily-rx.hex";
char HEX_LILY_TX[50] = "lily-tx.hex";
char HEX_LILY_TIME[50] = "lily-synth.hex";

// Flash Utility Parameters
// Includes transmission baud rate (-b ?).
char FLASHBIN[50] = "/usr/bin/avrdude";
char FLASH_ARGS[50] = "-c avr109 -p x256a3u -B 8 -b 115200 "; // Standard programming args

char FLASH_ARGPATH[50] = "-P"; // Path specification argument
char FLASH_WRITE[50] = "-e -U flash:w:";
char FLASH_VERIFY[50] = "-U flash:v:";

// MCU Reset Command
char MCU_CMD_BOOTEXIT[50] = "E\r";
char MCU_CMD_APPREBOOT[50] = "board -r\r";

// Bootloader Parameters
char BOOT_WAIT_CHAR = '.';
char BOOT_ENTRY_SEQUENCE[30] = "abcdefghijklmnopqrstuvwxyz";

int help_summary(char *this)
{
    printf("Usage: %s [ w(rite) | v(erify) ] [ rx | rx3 | bbrx | tx | tx3 | bbtx| time | time3 | time1on3 | avery-rx | avery-ctrl | fulltx | "
           "all ] "
           "[crimson | tate | lily | avery] [0..15]\n",
           this);
    printf("Example: \n");
    printf("\tWrite MCU to all crimson boards: \n \t%s w all crimson\n", this);
    printf("\tVerify time board tate mcu code: \n \t%s v time tate\n", this);
    printf("\tWrite tate tx MCU to rfe-7: \n \t%s w tx tate 7\n", this);
    printf("\tWrite lily rx MCU to rfe-2: \n \t%s w rx lily 2\n", this);
    printf("\tWrite avery rx (rtm1): \n \t%s w avery-rx avery\n", this);
    printf("\tWrite avery ctrl (rtm2+): \n \t%s w avery-ctrl avery\n", this);
    return 0;
}

int server_switch(int on)
{
    if (on == 1)
    {
        system("systemctl start crimson-website.service");
        system("systemctl start crimson-server.service");
        printf("Starting Crimson server\n");
    }
    else if (on == 0)
    {
        system("systemctl stop crimson-website.service");
        system("systemctl stop crimson-server.service");
        printf("Stopping Crimson server \n");
    }
    else
    {
        return 0;
    }
    return 1;
}

int program(char *PATH_DEV, char *BOARD_HEX, char *MODE);
int main(int argc, char *argv[])
{

    // argv[1] needs to equal v or w
    // argv[2] needs to equal rx tx time or all

    int server_status = 1;
    if (argc <= 3)
    {
        printf("ERROR: At least three arguments required.\n");
        help_summary(argv[0]);
        return 0;
    }

    if (!(strcmp(argv[1], "v") == 0 || strcmp(argv[1], "w") == 0))
    {
        printf("ERROR: First argument should be \"v\" or \"w\".\n");
        help_summary(argv[0]);
        return 0;
    }

    if ((strcmp(argv[2], "time") == 0) || (strcmp(argv[2], "time3") == 0) || (strcmp(argv[2], "time1on3") == 0) ||
        (strcmp(argv[2], "rx") == 0) || (strcmp(argv[2], "rx3") == 0) || (strcmp(argv[2], "bbrx") == 0) ||
        (strcmp(argv[2], "tx") == 0) || (strcmp(argv[2], "tx3") == 0) || (strcmp(argv[2], "bbtx") == 0) ||
        (strcmp(argv[2], "avery-rx") == 0) || (strcmp(argv[2], "avery-ctrl") == 0) ||
        (strcmp(argv[2], "fulltx") == 0) || (strcmp(argv[2], "all") == 0))
    {
        fflush(stdout);
    }
    else
    {
        printf("ERROR: Invalid second argument: %s: ", argv[2]);
        help_summary(argv[0]);
        return 1;
    }

    if ((strcmp(argv[3], "crimson") == 0) || (strcmp(argv[3], "tate") == 0) || (strcmp(argv[3], "lily") == 0) || (strcmp(argv[3], "avery") == 0))
    {
        fflush(stdout);
        if ((strcmp(argv[2], "all") == 0) && (strcmp(argv[3], "tate") == 0))
        {
            printf("ERROR: flash all currently unsupported for tate.\n");
            return 1;
        }
        if ((strcmp(argv[2], "all") == 0) && (strcmp(argv[3], "lily") == 0))
        {
            printf("ERROR: flash all currently unsupported for lily.\n");
            return 1;
        }
        if ((strcmp(argv[2], "rx3") == 0) && ((strcmp(argv[3], "crimson") == 0 || strcmp(argv[3], "lily") == 0)))
        {
            printf("ERROR: rx3 currently unsupported for crimson, chestnut.\n");
            return 1;
        }
        if ((strcmp(argv[2], "bbrx") == 0) && ((strcmp(argv[3], "crimson") == 0 || strcmp(argv[3], "lily") == 0)))
        {
            printf("ERROR: bbrx currently unsupported for crimson, chestnut.\n");
            return 1;
        }
        if ((strcmp(argv[2], "tx3") == 0) && ((strcmp(argv[3], "crimson") == 0 || strcmp(argv[3], "lily") == 0)))
        {
            printf("ERROR: tx3 currently unsupported for crimson, chestnut.\n");
            return 1;
        }
        if ((strcmp(argv[2], "bbtx") == 0) && ((strcmp(argv[3], "crimson") == 0 || strcmp(argv[3], "lily") == 0)))
        {
            printf("ERROR: bbtx currently unsupported for crimson, chestnut.\n");
            return 1;
        }
        if ((strcmp(argv[2], "time3") == 0) && ((strcmp(argv[3], "crimson") == 0 || strcmp(argv[3], "lily") == 0)))
        {
            printf("ERROR: time3 currently unsupported for crimson, chestnut.\n");
            return 1;
        }
        if ((strcmp(argv[2], "time1on3") == 0) && ((strcmp(argv[3], "crimson") == 0 || strcmp(argv[3], "lily") == 0)))
        {
            printf("ERROR: time3 currently unsupported for crimson, chestnut.\n");
            return 1;
        }
        if ((strcmp(argv[2], "avery-rx") == 0) && ((strcmp(argv[3], "tate") == 0 || strcmp(argv[3], "lily") == 0)))
        {
            printf("ERROR: avery-rx currently unsupported for tate, chestnut.\n");
            return 1;
        }
        if ((strcmp(argv[2], "avery-ctrl") == 0) && ((strcmp(argv[3], "tate") == 0 || strcmp(argv[3], "lily") == 0)))
        {
            printf("ERROR: avery-ctrl currently unsupported for tate, chestnut.\n");
            return 1;
        }
        if ((strcmp(argv[2], "fulltx") == 0) && ((strcmp(argv[3], "tate") == 0 || strcmp(argv[3], "lily") == 0)))
        {
            printf("ERROR: fulltx currently unsupported for tate, chestnut.\n");
            return 1;
        }
    }
    else
    {
        printf("ERROR: Invalid third argument: %s: ", argv[2]);
        help_summary(argv[0]);
        return 1;
    }

    if (strcmp(argv[3], "tate") == 0)
    {
        // arg 4 will not exist when the user attempts to flash the time board
        bool is_time =
            (strcmp(argv[2], "time") == 0) || (strcmp(argv[2], "time3") == 0) || (strcmp(argv[2], "time1on3") == 0);
        bool is_rx_or_tx = (strcmp(argv[2], "rx") == 0) || (strcmp(argv[2], "rx3") == 0) ||
                           (strcmp(argv[2], "bbrx") == 0) || (strcmp(argv[2], "tx") == 0) ||
                           (strcmp(argv[2], "tx3") == 0) || (strcmp(argv[2], "bbtx") == 0);
        bool is_valid_board;
        if (is_rx_or_tx)
        {
            is_valid_board =
                ((strcmp(argv[4], "0") == 0) || (strcmp(argv[4], "1") == 0) || (strcmp(argv[4], "2") == 0) ||
                 (strcmp(argv[4], "3") == 0) || (strcmp(argv[4], "4") == 0) || (strcmp(argv[4], "5") == 0) ||
                 (strcmp(argv[4], "6") == 0) || (strcmp(argv[4], "7") == 0) || (strcmp(argv[4], "8") == 0) ||
                 (strcmp(argv[4], "9") == 0) || (strcmp(argv[4], "10") == 0) || (strcmp(argv[4], "11") == 0) ||
                 (strcmp(argv[4], "12") == 0) || (strcmp(argv[4], "13") == 0) || (strcmp(argv[4], "14") == 0) ||
                 (strcmp(argv[4], "15") == 0));
        }
        else
            is_valid_board = true;
        fflush(stdout);
        if (is_time || (is_rx_or_tx && is_valid_board))
        {
            if (is_rx_or_tx)
            {
                strcat(PATH_TATE_TX, argv[4]);
                strcat(PATH_TATE_RX, argv[4]);
            }
            printf("Time Path is: %s \n", PATH_TATE_TIME);
            printf("Tx Path is: %s \n", PATH_TATE_TX);
            printf("Rx Path is: %s \n", PATH_TATE_RX);
            printf("\n");
        }
        else
        {
            printf("ERROR: invalid board");
            return 1;
        }
    }
    else if (strcmp(argv[3], "lily") == 0)
    {
        // arg 4 will not exist when the user attempts to flash the time board
        bool is_time = (strcmp(argv[2], "time") == 0);
        bool is_rx_or_tx = (strcmp(argv[2], "rx") == 0) || (strcmp(argv[2], "tx") == 0);
        bool is_valid_board;
        if (is_rx_or_tx)
        {
            is_valid_board =
                ((strcmp(argv[4], "2") == 0) || (strcmp(argv[4], "3") == 0) || (strcmp(argv[4], "6") == 0) ||
                 (strcmp(argv[4], "7") == 0) || (strcmp(argv[4], "10") == 0) || (strcmp(argv[4], "11") == 0) ||
                 (strcmp(argv[4], "14") == 0) || (strcmp(argv[4], "15") == 0));
            if (!is_valid_board)
            {
                printf("ERROR: You've selected a board number which is not compatible with the lily product. "
                       "Compatible board numbers are 2,3,6,7,10,11,14,15. \n");
                return 1;
            }
        }
        else
            is_valid_board = true;
        fflush(stdout);
        if (is_time || (is_rx_or_tx && is_valid_board))
        {
            if (is_rx_or_tx)
            {
                strcat(PATH_LILY_TX, argv[4]);
                strcat(PATH_LILY_RX, argv[4]);
            }
            printf("Time Path is: %s \n", PATH_LILY_TIME);
            printf("Tx Path is: %s \n", PATH_LILY_TX);
            printf("Rx Path is: %s \n", PATH_LILY_RX);
            printf("\n");
        }
        else
        {
            printf("ERROR: invalid board");
            return 1;
        }
    }
    else
    {
        strcat(PATH_TATE_TX, "0");
        strcat(PATH_TATE_RX, "0");
        printf("Flash Args: %s \n", FLASH_ARGS);
        printf("Time Path is: %s \n", PATH_TNG_TIME);
        printf("Tx Path is: %s \n", PATH_TNG_TX);
        printf("Rx Path is: %s \n", PATH_TNG_RX);
        printf("\n");
    }

    if (strcmp(argv[3], "tate") == 0)
    {
        if (!system("systemctl is-active --quiet cyan-server.service"))
        {
            printf("Cyan server is active, stopping cyan server\n");
            if (system("systemctl stop cyan-website.service") == -1)
            {
                return 1;
            }
            if (system("systemctl stop cyan-server.service") == -1)
            {
                return 1;
            }
        }
        else
        {
            printf("Cyan server is not active\n");
            server_status = 0;
        }
    }
    else if (strcmp(argv[3], "lily") == 0)
    {
        if (!system("systemctl is-active --quiet chestnut-server.service"))
        {
            printf("Chestnut server is active, stopping chestnut server\n");
            if (system("systemctl stop chestnut-server.service") == -1)
            {
                return 1;
            }
        }
        else
        {
            printf("Chestnut server is not active\n");
            server_status = 0;
        }
    }
    else if (strcmp(argv[3], "crimson") == 0)
    {
        if (!system("systemctl is-active --quiet crimson-server.service"))
        {
            printf("Crimson server is active, stopping crimson server\n");
            if (system("systemctl stop crimson-website.service") == -1)
            {
                return 1;
            }
            if (system("systemctl stop crimson-server.service") == -1)
            {
                return 1;
            }
        }
    }
    // As of December 2, 2025 Avery is keeping the same systemd service names as Crimson
    else if (strcmp(argv[3], "avery") == 0)
    {
        if (!system("systemctl is-active --quiet crimson-server.service"))
        {
            printf("Avery server is active, stopping avery server\n");
            if (system("systemctl stop crimson-website.service") == -1)
            {
                return 1;
            }
            if (system("systemctl stop crimson-server.service") == -1)
            {
                return 1;
            }
        }else{
            printf("Calamine server is not active\n");
            server_status = 0;
        }
    }
    // Closes all minicom instances, since they will interfere with flashing
    system("killall minicom");

    if ((strcmp(argv[3], "crimson") == 0))
    {
        if ((strcmp(argv[2], "time") == 0) || (strcmp(argv[2], "all") == 0))
        {
            printf("Flashing TIME\n");
            program(PATH_TNG_TIME, HEX_TNG_TIME, argv[1]);
        }
        if ((strcmp(argv[2], "rx") == 0) || (strcmp(argv[2], "all") == 0))
        {
            printf("Flashing RX\n");
            program(PATH_TNG_RX, HEX_TNG_RX, argv[1]);
        }
        if ((strcmp(argv[2], "tx") == 0) || (strcmp(argv[2], "all") == 0))
        {
            printf("Flashing TX\n");
            program(PATH_TNG_TX, HEX_TNG_TX, argv[1]);
        }
        if (strcmp(argv[2], "fulltx") == 0)
        {
            printf("Flashing FULLTX\n");
            program(PATH_TNG_TX, HEX_TNG_FULLTX, argv[1]);
        }
    } else if ((strcmp(argv[3], "avery") == 0))
    {
        if ((strcmp(argv[2], "time") == 0) || (strcmp(argv[2], "all") == 0))
        {
            printf("Flashing TIME\n");
            program(PATH_TNG_TIME, HEX_TNG_TIME, argv[1]);
        }
        if ((strcmp(argv[2], "rx") == 0) || (strcmp(argv[2], "all") == 0))
        {
            printf("Flashing RX\n");
            program(PATH_TNG_RX, HEX_TNG_RX, argv[1]);
        }
        if ((strcmp(argv[2], "avery-rx") == 0) || (strcmp(argv[2], "all") == 0))
        {
            printf("Flashing AVERY-RX\n");
            program(PATH_TNG_TX, HEX_TNG_AVERY_RX, argv[1]);
        }
        if ((strcmp(argv[2], "avery-ctrl") == 0) || (strcmp(argv[2], "all") == 0))
        {
            printf("Flashing AVERY-CTRL\n");
            program(PATH_TNG_GPIO, HEX_TNG_AVERY_CTRL, argv[1]);
        }
    }
    else if ((strcmp(argv[3], "tate") == 0))
    {
        if (strcmp(argv[2], "time") == 0)
        {
            printf("Flashing TIME\n");
            program(PATH_TATE_TIME, HEX_TATE_TIME, argv[1]);
        }
        if (strcmp(argv[2], "time3") == 0)
        {
            printf("Flashing TIME3\n");
            program(PATH_TATE_TIME, HEX_TATE_TIME3, argv[1]);
        }
        if (strcmp(argv[2], "time1on3") == 0)
        {
            printf("Flashing TIME1ON3\n");
            program(PATH_TATE_TIME, HEX_TATE_TIME1ON3, argv[1]);
        }
        if (strcmp(argv[2], "rx") == 0)
        {
            printf("Flashing RX\n");
            program(PATH_TATE_RX, HEX_TATE_RX, argv[1]);
        }
        if (strcmp(argv[2], "rx3") == 0)
        {
            printf("Flashing RX3\n");
            program(PATH_TATE_RX, HEX_TATE_RX3, argv[1]);
        }
        if (strcmp(argv[2], "bbrx") == 0)
        {
            printf("Flashing BBRX\n");
            program(PATH_TATE_RX, HEX_TATE_BBRX, argv[1]);
        }
        if (strcmp(argv[2], "tx") == 0)
        {
            printf("Flashing TX\n");
            program(PATH_TATE_TX, HEX_TATE_TX, argv[1]);
        }
        if (strcmp(argv[2], "bbtx") == 0)
        {
            printf("Flashing BBTX\n");
            program(PATH_TATE_TX, HEX_TATE_BBTX, argv[1]);
        }
        if (strcmp(argv[2], "tx3") == 0)
        {
            printf("Flashing TX3\n");
            program(PATH_TATE_TX, HEX_TATE_TX3, argv[1]);
        }
    }
    else if ((strcmp(argv[3], "lily") == 0))
    {
        if (strcmp(argv[2], "time") == 0)
        {
            printf("Flashing TIME\n");
            program(PATH_LILY_TIME, HEX_LILY_TIME, argv[1]);
        }
        if (strcmp(argv[2], "rx") == 0)
        {
            printf("Flashing RX\n");
            program(PATH_LILY_RX, HEX_LILY_RX, argv[1]);
        }
        if (strcmp(argv[2], "tx") == 0)
        {
            printf("Flashing TX\n");
            program(PATH_LILY_TX, HEX_LILY_TX, argv[1]);
        }
    }

    if (server_status)
    {
        printf("INFO: Flashing complete. Reboot the server if you flashed an RF board. "
               "Reboot the unit if you flashed the time board\n");
    };

    return 0;
}

int program(char *PATH_DEV, char *BOARD_HEX, char *MODE){

    // open device path
    int fd = -1;
    fd = open(PATH_DEV, O_RDWR);
    char byte = 0;

    if (-1 == fd)
    {
        printf("ERROR: Aborting because we are unable to open device: %s.\n", PATH_DEV);
        return 1;
    }

    int lock_fail = flock(fd, LOCK_EX | LOCK_NB);
    if (lock_fail)
    {
        if (errno == EWOULDBLOCK)
        {
            printf("ERROR: Aborting. Device already in use\n");
            return EBUSY;
        }
        else
        {
            printf("ERROR: Aborting. Get lock failed with error: %s\n", strerror(errno));
            return errno;
        }
    }

    // Exit boot loader in case we are in there
    write(fd, MCU_CMD_BOOTEXIT, 2);

    // CLEAR BUFF
    sleep(1);
    tcflush(fd, TCIOFLUSH);

    // RESTART BOARD
    write(fd, MCU_CMD_APPREBOOT, 9);

    size_t rfFD;
    int i;
    int checkBoot = 0;

    // check for board response
    printf("Looking for boot sequence...\n");

    struct timeval timeout;
    fd_set set;
    FD_ZERO(&set);       /* clear the set */
    FD_SET(fd, &set);    /* add our file descriptor to the set */
    timeout.tv_sec = 36; // Watchdog time is 6s, plus around 12s boot = 18s per loop. 36s ensures we should have seen a
                         // boot twice (to account for when a second reboot needs to be issues)
    timeout.tv_usec = 0;
    int rv;
    rv = select(fd + 1, &set, NULL, NULL, &timeout);
    if (-1 == rv)
    {
        printf("ERROR: Aborting due to invalid file handler: %s\n", PATH_DEV);
        return 1;
    }
    else if (rv == 0)
        printf("ERROR: Timeout: No data received for: %s\n", PATH_DEV); /* Filehandler timeout occured */
    else
    {
        rfFD = read(fd, &byte, 1);
        for (i = 0; i < 500; i++)
        {
            rfFD = read(fd, &byte, 1);
            if (-1 == rfFD)
            {
                printf("ERROR: Aborting due to invalid file handler for: %s\n", PATH_DEV);
                return 1;
            }
            switch (checkBoot)
            {
            case 0:
                if (byte == BOOT_WAIT_CHAR)
                {
                    checkBoot++;
                }
                else
                {
                    checkBoot = 0;
                    printf("%c", byte);
                }
                break;
            case 1:
                if (byte == BOOT_WAIT_CHAR)
                {
                    checkBoot++;
                }
                else
                {
                    checkBoot = 0;
                }
                break;

            case 2:
                if (byte == BOOT_WAIT_CHAR)
                {
                    checkBoot++;
                }
                else
                {
                    checkBoot = 0;
                }
                break;

            case 3:
                // Verifies BOOT_WAIT_CHAR was printed a 4th time. Since 3 times could be an elipsis
                if (byte != BOOT_WAIT_CHAR)
                {
                    checkBoot = 0;
                    // Re-issue reboot command
                    // If 3 . are detected then the board was likely in an unclean state and the help message was
                    // printed instead
                    write(fd, MCU_CMD_APPREBOOT, 9);
                    break;
                }
                // Send token
                printf("Sending Entry Token...\n");
                write(fd, BOOT_ENTRY_SEQUENCE, 26);
                sleep(1);
                // sending commands to check version or write MCU code
                printf("Attempting to program...\n");
                // create command...
                char command[1024] = {0};
                if (strcmp(MODE, "w") == 0)
                {
                    snprintf(command, sizeof(command), "%s %s %s %s %s%s", FLASHBIN, FLASH_ARGS, FLASH_ARGPATH,
                             PATH_DEV, FLASH_WRITE, BOARD_HEX);
                    printf("Executing: %s\n", command);
                }
                else if (strcmp(MODE, "v") == 0)
                {
                    snprintf(command, sizeof(command), "%s %s %s %s %s%s", FLASHBIN, FLASH_ARGS, FLASH_ARGPATH,
                             PATH_DEV, FLASH_VERIFY, BOARD_HEX);
                    printf("Executing: %s\n", command);
                }
                else
                {
                    printf("ERROR: Exiting program, Invalid Argument: %s\n", MODE);
                    return 0;
                }
                system(command);

                return 0;
            }
        }
    }
    printf("ERROR: Timeout: Boot Entry Sequence not observed for: %s\n", PATH_DEV);
    return 1;
}
