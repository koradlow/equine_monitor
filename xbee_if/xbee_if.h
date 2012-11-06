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

#ifndef XBEE_IF
#define XBEE_IF

#include <gbee.h>
#include <string>
#include <inttypes.h>

class XBee_config {
public:
	XBee_config(std::string port, bool mode, const uint8_t pan[8]);

	std::string serial_port;
	bool coordinator_mode;
	uint8_t pan_id[8];
	uint32_t baud_rate;
};

class XBee {
public:
	XBee(XBee_config& config);
	virtual ~XBee();
	void xbee_init();
	void xbee_status();
	void xbee_print_at_value(std::string at);
private: 
	void xbee_start_network();
	GBeeFrameData& xbee_receive_and_print(uint32_t timeout, uint16_t *length);
	uint8_t* xbee_at_cmd(const std::string at_cmd_str);
	XBee_config config;
	GBee *gbee_handle;
};



#endif
