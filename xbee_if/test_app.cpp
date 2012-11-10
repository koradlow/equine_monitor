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

int main(int argc, char **argv) {
	uint8_t pan_id[2] = {0xAB, 0xDD};
	uint8_t unique_id = 2;
	XBee_config config("/dev/ttyUSB0", false, unique_id, pan_id, 750);
	/* transmission test data */
	uint8_t data[2] = {0xAB, 0xCD};
	XBee_measurement measurement1(TEMP, data, 2);
	uint8_t data2[3] = {0x01, 0x23, 0x45};
	XBee_measurement measurement2(HEART, data2, 3);

	XBee interface(config);
	interface.xbee_init();

	interface.xbee_status();
	printf("\n");
	for (int i=0; i < 1; i++) {
		sleep(2);
		printf("Sending measurement type: %02x, values: %02x, %02x, length: %02x\n",
		measurement1.type, measurement1.data[0], measurement1.data[1], measurement1.length+1);
		interface.xbee_send_measurement(measurement1);
		sleep(2);
		printf("Sending measurement type: %02x, value: %02x, %02x, %02x, length: %02x\n",
		measurement2.type, measurement2.data[0], measurement2.data[1], measurement2.data[2], measurement2.length+1);
		interface.xbee_send_measurement(measurement2);
	}

	printf("\n");
	interface.xbee_print_at_value("MY"); 	// own address
	interface.xbee_print_at_value("ID");	// extended PAN ID
	interface.xbee_print_at_value("OP");	// operating extended PAN ID
	interface.xbee_print_at_value("OI");	// operating 16bit PAN ID
	interface.xbee_print_at_value("DH");	// destination Address high
	interface.xbee_print_at_value("DL");	// destination Address low
	interface.xbee_print_at_value("SM");	// sleep mode
	interface.xbee_print_at_value("MP");	// 16bit Parent Address
	
	return 0;
}
