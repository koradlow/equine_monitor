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

const char* hex_str(uint8_t *data, uint8_t length);

int main(int argc, char **argv) {
	uint8_t pan_id[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0xAB, 0xBC, 0xCD};
	uint8_t unique_id = 2;
	uint8_t error_code;
	XBee_Config config("/dev/ttyUSB0", "Denver", false, unique_id, pan_id, 9000);
	
	XBee interface(config);
	error_code = interface.xbee_init();
	if (error_code != GBEE_NO_ERROR) {
		printf("Error: unable to configure device, code: %02x\n", error_code);
		return 0;
	}

	for (int i=0; i < 1; i++) {
		interface.xbee_status();
		sleep(2);
	}

	/* print some information about the current network status */
	XBee_At_Command cmd("MY");
	interface.xbee_at_command(cmd);
	printf("%s: %s\n", cmd.at_command.c_str(), hex_str(cmd.data, cmd.length));

	cmd = XBee_At_Command("ID");
	interface.xbee_at_command(cmd);
	printf("%s: %s\n", cmd.at_command.c_str(), hex_str(cmd.data, cmd.length));

	cmd = XBee_At_Command("NI");
	interface.xbee_at_command(cmd);
	printf("%s: %s\n", cmd.at_command.c_str(), hex_str(cmd.data, cmd.length));

	cmd = XBee_At_Command("NP");
	interface.xbee_at_command(cmd);
	printf("%s: %s\n", cmd.at_command.c_str(), hex_str(cmd.data, cmd.length));

	cmd = XBee_At_Command("SH");
	interface.xbee_at_command(cmd);
	printf("%s: %s\n", cmd.at_command.c_str(), hex_str(cmd.data, cmd.length));
	cmd = XBee_At_Command("SL");
	interface.xbee_at_command(cmd);
	printf("%s: %s\n", cmd.at_command.c_str(), hex_str(cmd.data, cmd.length));

	const XBee_Address *addr = interface.xbee_get_address("coordinator");
	printf("Address of %s: 16bit: %04x, 64bith: %08x, 64bitl: %08x\n", addr->node.c_str(), 
	addr->addr16, addr->addr64h, addr->addr64l);

	return 0;
}

const char* hex_str(uint8_t *data, uint8_t length) {
	static char c_str[80];
	for (int i = 0; i < length; i++)
		sprintf(&c_str[i*3], "%02x ", data[i]);
	return c_str;
}
