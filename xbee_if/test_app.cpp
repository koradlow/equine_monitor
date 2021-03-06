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

#include "xbee_if.h"
#include <gbee.h>
#include <gbee-util.h>
#include <array> 
#include <unistd.h>
#include <sys/time.h>

const char* hex_str(uint8_t *data, uint8_t length);
XBee_Message get_message(const std::string &dest, uint16_t size);
void speed_measurement(XBee* interface, const std::string &dest, uint16_t size, uint8_t iterations);

int main(int argc, char **argv) {
	uint8_t pan_id[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0xAB, 0xBC, 0xCD};
	uint8_t error_code;
	XBee_Config config("/dev/ttyUSB0", "denver", false, pan_id, 500, B115200, 1);
	
	XBee interface(config);
	error_code = interface.xbee_init();
	if (error_code != GBEE_NO_ERROR) {
		printf("Error: unable to configure device, code: %02x\n", error_code);
		return 0;
	}
	interface.xbee_status();
	speed_measurement(&interface, "coordinator", 1024, 10);

	XBee_Message *rcv_msg = NULL;
	uint16_t length = 0;
	uint8_t *payload;
	while (true) {
		if (interface.xbee_bytes_available()) {
			rcv_msg = interface.xbee_receive_message();
			if ( rcv_msg->is_complete()) {
				rcv_msg->get_payload(&length);
				printf("%s msg received., length: %u\n",
				rcv_msg->is_complete() ? "complete" : "incomplete", length);
				payload = rcv_msg->get_payload(&length);
				printf("content: %s\n", hex_str(payload, length));
			}
			delete rcv_msg;
		}
		usleep(200);
	}
	return 0;
}

const char* hex_str(uint8_t *data, uint8_t length) {
	static char c_str[80];
	for (int i = 0; i < length; i++)
		sprintf(&c_str[i*3], "%02x ", data[i]);
	return c_str;
}

XBee_Message get_message(XBee* interface, const std::string &dest, uint16_t size) {
	const XBee_Address *addr = interface->xbee_get_address(dest);
	if (!addr) {
		printf("Error getting address for node %s\n", dest.c_str());
		return XBee_Message();
	}
	uint8_t *payload = new uint8_t[size];
	
	for (int i = 0; i < size; i++) {
		payload[i] = (uint8_t)i % 255;
	}
	XBee_Message test_msg(*addr, payload, size);
	delete[] payload;

	return test_msg; 
}

void speed_measurement(XBee* interface, const std::string &dest, uint16_t size, uint8_t iterations) {
	struct timeval start, end;
	long mtime, seconds, useconds;
	uint8_t error_code;  
	XBee_Message test_msg = get_message(interface, dest, size);

	gettimeofday(&start, NULL);
	for (int i = 0; i < iterations; i++) {
		if ((error_code = interface->xbee_send_data(test_msg))!= 0x00) {
			printf("Error transmitting: %u\n", error_code);
			break;
		}
		printf("Successfully transmitted msg %u with \n", i+1);
	}
	gettimeofday(&end, NULL);

	seconds  = end.tv_sec  - start.tv_sec;
	useconds = end.tv_usec - start.tv_usec;

	mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;

	printf("Elapsed time: %ld milliseconds\n", mtime);
	printf("Data throughput: %.1f kB/s\n", (iterations * size) / (float) mtime);
}

