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
	uint8_t pan_id[8] = {0x00, 0x00, 0x00, 0x00, 0xFE, 0xFE, 0xFE, 0x00};
	XBee_config config("/dev/ttyUSB0", false, pan_id);
	

	XBee interface(config);
	interface.xbee_init();
	for (int i=0; i <= 20; i++) {
		sleep(1);
		interface.xbee_status();
	}
	return 0;
}
