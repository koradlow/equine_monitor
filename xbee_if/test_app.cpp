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
	XBee_config config("/dev/ttyUSB0", false, unique_id, pan_id);
	/* transmission test data */
	uint8_t data[2] = {123, 211};
	XBee_measurement measurement(TEMP, data, 2);

	XBee interface(config);
	interface.xbee_init();

	for (int i=0; i <= 10; i++) {
		sleep(2);
		interface.xbee_send_measurement(measurement);
		interface.xbee_status();
	}

	interface.xbee_print_at_value("MY"); 	// own address
	interface.xbee_print_at_value("ID");	// extended PAN ID
	interface.xbee_print_at_value("OP");	// operating extended PAN ID
	interface.xbee_print_at_value("OI");	// operating 16bit PAN ID
	interface.xbee_print_at_value("DH");	// destination Address high
	interface.xbee_print_at_value("DL");	// destination Address low
	interface.xbee_print_at_value("SM");	// sleep mode
	interface.xbee_print_at_value("MP");	// 16bit Parent Address
	interface.xbee_print_at_value("ND");	// Node discovery

	return 0;
}
