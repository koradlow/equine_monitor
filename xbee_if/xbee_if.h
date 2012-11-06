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

enum sensor_type {
	TEMP,
	ACC,
	GPS,
	HEART
};

class XBee_config {
public:
	XBee_config(std::string port, bool mode, const uint8_t unique_id,
	const uint8_t pan[2]);

	std::string serial_port;
	bool coordinator_mode;
	uint8_t pan_id[2];
	uint8_t unique_id;
	uint32_t baud_rate;
};

class XBee_measurement {
public:
	XBee_measurement(enum sensor_type, const uint8_t* data, uint16_t length );
	~XBee_measurement();
	enum sensor_type type;
	uint8_t* data;
	uint16_t length;

};

class XBee {
public:
	XBee(XBee_config& config);
	virtual ~XBee();
	void xbee_init();
	void xbee_status();
	void xbee_print_at_value(std::string at);
	void xbee_send_measurement(XBee_measurement& measurement);
	void xbee_receive_measurement();
	int xbee_bytes_available();
private: 
	void xbee_start_network();
	GBeeFrameData& xbee_receive_and_print(uint32_t timeout, uint16_t *length);
	uint8_t* xbee_at_cmd(const std::string at_cmd_str);
	XBee_config config;
	GBee *gbee_handle;
};



#endif
