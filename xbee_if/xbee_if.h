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

#define XBEE_MSG_LENGTH 84

#define MSG_HEADER_LENGTH 4
/* define position of values in the header */
#define MSG_TYPE 0x00
#define MSG_PART 0x01
#define MSG_PART_CNT 0x02
#define MSG_PAYLOAD_LENGTH 0x03

enum sensor_type {
	TEMP,
	ACC,
	GPS,
	HEART
};

enum message_type {
	CONFIG,
	TEST,
	DATA
};

class XBee_config {
public:
	XBee_config(std::string port, bool mode, const uint8_t unique_id,
	const uint8_t pan[2], uint32_t timeout);

	std::string serial_port;
	bool coordinator_mode;
	uint8_t pan_id[2];
	uint8_t unique_id;
	uint32_t baud_rate;
	uint32_t timeout;
};

class XBee_measurement {
public:
	XBee_measurement(enum sensor_type, const uint8_t* data, uint16_t length);
	enum sensor_type type;
	uint8_t* data;
	uint16_t length;

};

class XBee {
public:
	XBee(XBee_config& config);
	virtual ~XBee();
	void xbee_init();
	uint8_t xbee_status();
	void xbee_print_at_value(std::string at);
	uint8_t xbee_send_measurement(XBee_measurement& measurement);
	void xbee_receive_measurement();
	int xbee_bytes_available();
	void xbee_test_msg();
private: 
	void xbee_start_network();
	GBeeFrameData& xbee_receive_and_print(uint16_t *length);
	uint8_t* xbee_at_cmd(const std::string at_cmd_str);
	XBee_config config;
	GBee *gbee_handle;
};

class XBee_Message {
friend XBee;
public:
	XBee_Message(enum message_type type, const uint8_t *payload, uint16_t length);
	XBee_Message(const uint8_t *message);
	XBee_Message();
	~XBee_Message();
	uint8_t* get_payload(uint16_t *length);
	enum message_type get_type();
	bool is_complete();

private:
	bool append_msg(const XBee_Message &msg);
	uint8_t* get_msg(uint8_t part);
	uint16_t get_msg_len(uint8_t part);

	uint8_t *message_buffer;
	uint8_t *payload;
	enum message_type type;
	uint16_t payload_len;
	uint8_t message_part;
	uint8_t message_part_cnt;
	bool message_complete;
};


#endif
