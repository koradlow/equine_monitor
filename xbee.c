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
#include <termios.h>	/* POSIX terminal control definitions */

#define SERIAL_PORT "/dev/ttyAMA0"

/* try to open the serial port for communication with the xbee device
 * @device: NULL terminated string, defining the tty device to use
 * @ret_val:	0 on success, -1 on error */
int open_serial(const char* device) {
	int fd = -1; 
	fd = open(device, O_RDWR);
	if (fd < 0)
		fprintf(stderr, "Unable to open %s", device);
	return fd;
}

/* place the terminal 'fd' into Xbee compatible mode
 * @fd:		file descriptor for a terminal device
 * @ret_val:	0 on success, -1 on error */
int setup_serial(int fd) {
	struct termios new_settings, old_settings;

	/* Get the current settings for the port */
	if (tcgetattr(fd, &old_settings)) {
		perror("tcgetattr: ");
		return -1;
	}
	new_settings = old_settings;

	/* set to B9600-8N1, allow input to be received */
	new_settings.c_cflag = B9600 | CS8 | CREAD;
	/* disable hardware flow control */
	new_settings.c_cflag &= ~CRTSCTS; 
	
	/* Set the new settings */
	/* TSCNOW -> change occurs immediately */
	tcflush(fd, TCIFLUSH);
	if (tcsetattr(fd, TCSANOW, &new_settings) < 0) {
		perror("tcsetattr: ");
		return -1;
	}
	return fd;
}

/* main control loop of the program */
int main(int argc, char **argv) {
	char buf[64];
	int byte_cnt;

	/* try to open the serial port and configure it to be compatible
	 * with the xbee port */
	int fd = open_serial(SERIAL_PORT);
	if (fd < 0)
		exit(-1);
	if (setup_serial(fd) < 0)
		exit(-1);

	/* set the xbee into command mode */
	strcpy("+++", &buf[0]);
	write(fd, &buf, 3);
	printf("setting xbee into command mode\n"); 
	byte_cnt = read(fd, &buf, 2);
	printf("read %d bytes from serial: %s\n", byte_cnt, buf);

	return 1;
}
