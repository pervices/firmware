#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>

//Tate Paths
char PATH_TATE_TIME[50]      = "/dev/ttycyan-time";


//MCU Command
char MCU_CMD_BOOTEXIT[50] = "E\r";
char MCU_CMD_APPREBOOT[50] = "board -r\r";
char MCU_CMD_LMK1PLL1[50] = "status -l 11\r";
char MCU_CMD_LMK1PLL2[50] = "status -l 12\r";
char MCU_CMD_LMK2PLL1[50] = "status -l 21\r";
char MCU_CMD_LMK2PLL2[50] = "status -l 22\r";
char MCU_CMD_LMK3PLL1[50] = "status -l 31\r";
char MCU_CMD_LMK3PLL2[50] = "status -l 32\r";

//Bootloader Parameters
char BOOT_WAIT_CHAR = '.';
//char PLL_LOCKED[12] = "PLL12 Locked";
char BOOT_ENTRY_SEQUENCE[30] = "abcdefghijklmnopqrstuvwxyz";

int failcount=0;

int execute(char *CMD){
	//open device path
	int fd = -1;
	fd = open(PATH_TATE_TIME, O_RDWR);
	char byte = 0;

	if( -1 == fd ){
		printf("ERROR: Aborting because we are unable to open device: %s.\n", PATH_TATE_TIME);
		return 1;
	}

	//CLEAR BUFF
	sleep(1);
	tcflush(fd,TCIOFLUSH);

	if (CMD==MCU_CMD_APPREBOOT) {
		write(fd, MCU_CMD_APPREBOOT, 9);
		sleep(10);
		return 0;
	}
	
	//SEND COMMAND
	write(fd, CMD, 13);

	size_t rfFD;
	int i;
	int checkBoot = 0;

	//check for board response
	//printf("Looking for PLL status...\n");

	struct timeval timeout;
	fd_set set;
	FD_ZERO(&set); /* clear the set */
	FD_SET(fd, &set); /* add our file descriptor to the set */
	timeout.tv_sec = 10; /* this shouldn't take more than 10 sec*/
	timeout.tv_usec = 0;
	int rv;
	rv = select(fd + 1, &set, NULL, NULL, &timeout);
	if (rv == -1) {
		printf("ERROR: Aborting due to invalid file handler: %s\n", PATH_TATE_TIME);
		return 1;
	} else if (rv == 0) {
		printf("ERROR: Timeout: No data received for: %s\n", PATH_TATE_TIME); /* Filehandler timeout occured */	
	} else {
		for (i = 0; i < 6; i++) {
			rfFD = read(fd, &byte, 1);
			if (-1 == rfFD) {
				printf("ERROR: Aborting due to invalid file handler for: %s\n", PATH_TATE_TIME);
				return 1;
			}
			switch (checkBoot) {
				case 0:
					if (byte == 'P'){
						checkBoot++;
					}else{
						checkBoot = 0;
					}	
					break;
				case 1:
					if (byte == 'L'){
						checkBoot++;
					}else{
						checkBoot = 0;
					}
					break;
				case 2:
					if (byte == 'L'){
						checkBoot++;
					}else{
						checkBoot = 0;
					}
					break;
				case 3:
					if ((byte == '1')||(byte == '2')){
						checkBoot++;
					}else{
						checkBoot = 0;
					}
					break;
				case 4:
					if (byte == ' '){
						checkBoot++;
					}else{
						checkBoot = 0;
					}
					break;
				case 5:
					if (byte == 'L'){
						return 0;
					}else{
						return 1;
					}
					/*break;
				case 6:
					if (byte == 'o'){
						checkBoot++;
					}else{
						checkBoot = 0;
					}
					break;
				case 7:
					if (byte == 'c'){
						checkBoot++;
					}else{
						checkBoot = 0;
					}
					break;
				case 8:
					if (byte == 'k'){
						checkBoot++;
					}else{
						checkBoot = 0;
					}
					break;
				case 9:
					if (byte == 'e'){
						checkBoot++;
					}else{
						checkBoot = 0;
					}
					break;
				case 10:
					if (byte == 'd'){
						checkBoot++;
					}else{
						checkBoot = 0;
					}
					return 0;*/
				default:
					return 1;
			}
		}
	}
	printf("ERROR: Timeout: PLL status is not observed for: %s\n", PATH_TATE_TIME);
	return 1;
}
int checkpll(void) {
	if ((execute(MCU_CMD_LMK1PLL1)) ||\
	    (execute(MCU_CMD_LMK1PLL2)) ||\
	    (execute(MCU_CMD_LMK2PLL1)) ||\
	    (execute(MCU_CMD_LMK2PLL2)) ||\
	    (execute(MCU_CMD_LMK3PLL1)) ||\
	    (execute(MCU_CMD_LMK3PLL2))) {
		printf("Error: PLL is not locked, rebooting time board\n");
		failcount++;
		execute(MCU_CMD_APPREBOOT);
		sleep (10);
		return 1;
	} else {
		printf("Success: PLLs are locked \n");
		return 0;
	}
	printf("ERROR: Timeout: PLL status is not observed for: %s\n", PATH_TATE_TIME);
	return 1;
}
int main(void) {
	while (failcount<3) {
		//printf("failed count is: %i\n", failcount);
		if (!checkpll()) {
			return 0;
		}
	}
	return 1;
}
