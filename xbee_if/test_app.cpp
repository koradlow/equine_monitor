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
	uint8_t unique_id = 1;
	XBee_config config("/dev/ttyUSB0", true, unique_id, pan_id);


	XBee interface(config);
	interface.xbee_init();

	for (int i=0; i <= 20; i++) {
		sleep(1);
		if (interface.xbee_bytes_available() > 0) {
			interface.xbee_receive_measurement();
		}
		interface.xbee_status();
	}

	interface.xbee_print_at_value("MY");
	interface.xbee_print_at_value("OP");
	interface.xbee_print_at_value("ID");
	interface.xbee_print_at_value("OI");
	interface.xbee_print_at_value("SC");

	return 0;
}
