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

#define SERIAL_PORT "/dev/ttyAMA0"
#define BUF_SIZE 255

/* Definition of global variables, used to store the current configuration
 * of the serial port, to be able to restore it */
static int global_fd = -1;
static struct termios old_settings;

/* restores the terminal settings to the state before the program
 * made any changes and terminates the program */
static void xbee_exit(bool reset)
{
	if (reset) {
		tcflush(global_fd, TCIFLUSH);
		tcsetattr(global_fd, TCSANOW, &old_settings);
	}
	close(global_fd);
	exit(-1);
}

/* try to open the serial port for communication with the xbee device
 * @device: NULL terminated string, defining the tty device to use
 * @ret_val:	0 on success, -1 on error */
int open_serial(const char* device) {
	int fd = -1; 
	/* O_RDWR - read write mode
	 * O_NOCTTY - this program isn't the controlling terminal
	 * O_NDELAY - ignore status of DCD signal line */ 
	fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);
	if (fd < 0)
		fprintf(stderr, "Unable to open %s", device);
	return fd;
}

/* place the terminal 'fd' into Xbee compatible mode
 * @fd:		file descriptor for a terminal device
 * @ret_val:	0 on success, -1 on error */
int setup_serial(int fd) {
	struct termios settings;

	/* Get the current settings for the port */
	if (tcgetattr(fd, &old_settings)) {
		perror("tcgetattr: ");
		return -1;
	}
	settings = old_settings;
	
	// setup baudrate
	settings.c_cflag |= B9600;
	// Ignore modem status lines & allow input to be received
	settings.c_cflag |= (CLOCAL | CREAD);
	// data size set to 8bits
	settings.c_cflag |= CS8;
	// disable flow control (Raspberry UART doesn't offer control lines)
	settings.c_cflag &= ~CRTSCTS;
	
	// ignore chars with parity errors & ignore BREAK condition
	settings.c_iflag = IGNPAR | IGNBRK;

	// output & local flags are irrelevant
	settings.c_oflag = 0;
	settings.c_lflag = 0;

	// set terminal I/O mode:  start timer when read() is called,
	// read() returns if at least 1 byte is read, or timer runs out
	settings.c_cc[VMIN] = 0;
	settings.c_cc[VTIME] = 1; // 0.1s

	/* Set the new settings */
	/* TSCNOW -> change occurs immediately */
	tcflush(fd, TCIOFLUSH);
	if (tcsetattr(fd, TCSANOW, &settings) < 0) {
		perror("tcsetattr: ");
		return -1;
	}
	return fd;
}

/* retrieve all bytes available in the serial buffer */
int xbee_receive(int fd, char *buf) {
	int bytes_available;

	sleep(2);
	memset(buf, 0, BUF_SIZE * sizeof(char));
	ioctl(fd, FIONREAD, &bytes_available);
	//printf("IOCTL: %i bytes available in the buffer\n", bytes_available);
	bytes_available = read(fd, buf, BUF_SIZE);
	buf[bytes_available] = '\0';
	//printf("read fct: %i bytes available in the buffer\n", bytes_available);
	return bytes_available;
}

/* a signal handler for the ctrl+c interrupt, in order to end the program
 * gracefully (restoring terminal settings and closing fd) */
static void signal_handler_interrupt(int signum)
{
	fprintf(stderr, "Interrupt received: Terminating program\n");
	xbee_exit(true);
}

/* main control loop of the program */
int main(int argc, char **argv) {
	int fd = -1;
	char buf[BUF_SIZE];
	int byte_cnt;

	/* register signal handler for interrupt signal, to exit gracefully */
	signal(SIGINT, signal_handler_interrupt);

	/* try to open the serial port and configure it to be compatible
	 * with the xbee port */
	fd = open_serial(SERIAL_PORT);
	global_fd = fd;
	if (fd < 0)
		xbee_exit(false);
	if (setup_serial(fd) < 0)
		xbee_exit(true);

	/* set the xbee into command mode */
	strcpy(&buf[0], "+++");
	byte_cnt = write(fd, &buf, 3);
	printf("setting xbee into command mode, wrote %i bytes\n", byte_cnt); 
	byte_cnt = xbee_receive(fd, buf);

	/* read Baud rate setting */
	strcpy(&buf[0], "ATBD\r");
	write(fd, &buf, 5);
	
	byte_cnt = xbee_receive(fd, buf);
	printf("\nBAUD: ");
	for (int i=0; i<byte_cnt; i++) {
		printf("%c", buf[i]);
	}
	
	/* read PAN ID */
	strcpy(&buf[0], "ATID\r");
	write(fd, &buf, 5);
	byte_cnt = xbee_receive(fd, buf);

	printf("\nPAN ID: ");
	for (int i=0; i<byte_cnt; i++) {
		printf("%c", buf[i]);
	}
	printf("\n");

	/* close the serial port and exit the program */
	xbee_exit(true);
	return 1;
}
