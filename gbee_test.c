/* This file is part of Equine Monitor
 *
 * Equine Monitor is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Equine Monitor is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Equine Monitor.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Konke Radlow <koradlow@gmail.com>
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <termios.h>	/* POSIX terminal control definitions */
#include <signal.h>
#include <sys/ioctl.h>
#include "gbee.h"
#include <gbee-util.h>

#define SERIAL_PORT "/dev/ttyAMA0"
#define BUF_SIZE 255

/* Definition of global variables, used to store the current configuration
 * of the serial port, to be able to restore it */
static GBee *gbee_handle;

/* restores the terminal settings to the state before the program
 * made any changes and terminates the program */
static void gbee_exit(bool reset)
{
	if (reset) {}
	gbeeDestroy(gbee_handle);
	exit(-1);
}

/* a signal handler for the ctrl+c interrupt, in order to end the program
 * gracefully (restoring terminal settings and closing fd) */
static void signal_handler_interrupt(int signum)
{
	fprintf(stderr, "Interrupt received: Terminating program\n");
	gbee_exit(false);
}

/* prints the fields contained in an AT command response frame */
static void print_frame_at_response(GBeeFrameData *frame, uint16_t length) {
	int i = 0;
	
	GBeeAtCommandResponse *at_frame = (GBeeAtCommandResponse*) frame;
	printf("Frame ID: %02d, status:%d\n", at_frame->frameId, at_frame->status);
	printf("AT Command: %c%c \n", at_frame->atCommand[0], at_frame->atCommand[1]);
	printf("Register value: ");
	while(at_frame->value[i] != 0x0d && i < length)
		printf("%x", at_frame->value[i++]);
	printf("\n");
}

/* main control loop of the program */
int main(int argc, char **argv) {
	GBeeError error_code;

	/* register signal handler for interrupt signal, to exit gracefully */
	signal(SIGINT, signal_handler_interrupt);
	
	/* initialize the xbee module */
	gbee_handle = gbeeCreate(SERIAL_PORT);
	if (!gbee_handle) {
		perror("Error creating XBee handle");
		gbee_exit(false);
	}
	
	/* set XBee device into API Mode */
	gbeeSetMode(gbee_handle, GBEE_MODE_API);
	
	/* detect mode of XBee device */
	GBeeMode mode;
	error_code = gbeeGetMode(gbee_handle, &mode);
	if (error_code != GBEE_NO_ERROR) {
		perror("Error detecting XBee mode");
		gbee_exit(false);
	}
	printf("XBee mode: %s\n", (mode == GBEE_MODE_API)? "API" : "Transparent");

	/* Figure out Firmware version */
	uint8_t frame_id = 1;
	uint8_t at_cmd[2] = {0x56, 0x52}; /* VR: V=0x56 R=053 */
	uint8_t value = 0;
	uint8_t length = 0;
	uint16_t length_l = 0;
	uint32_t timeout = 100;
	GBeeFrameData data;
	
	error_code = gbeeSendAtCommand(gbee_handle, frame_id, &at_cmd[0], &value , length);
	if (error_code != GBEE_NO_ERROR) {
		perror("Error sending XBee AT Command");
		gbee_exit(false);
	}
	error_code = gbeeReceive(gbee_handle, &data, &length_l, &timeout);
	if (error_code != GBEE_NO_ERROR) {
		perror("Error receiving XBee AT Response");
		gbee_exit(false);
	}
	printf("received Frame type %i wth length %i\n", data.ident, length_l);
	if (data.ident == 0x88)
		print_frame_at_response(&data, length_l);
	
	/* close the serial port and exit the program */
	gbee_exit(false);
	return 1;
}
