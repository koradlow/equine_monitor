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
#define XBEE_ADDR_CACHE_SIZE 4

#define MSG_HEADER_LENGTH 4
/* define position of values in the header */
#define MSG_TYPE 0x00
#define MSG_PART 0x01
#define MSG_PART_CNT 0x02
#define MSG_PAYLOAD_LENGTH 0x03

enum message_type {
	CONFIG,
	TEST,
	DATA
};

class XBee_Message;

class XBee_Address {
public:
	XBee_Address();
	XBee_Address(const std::string &node, const uint8_t *payload);

	std::string node;
	uint16_t addr16;
	uint32_t addr64h;
	uint32_t addr64l;
};

class XBee_Config {
public:
	XBee_Config(const std::string &port, const std::string &node, bool mode, 
		uint8_t unique_id, const uint8_t *pan, uint8_t pan_length, uint32_t timeout);
	XBee_Config(const XBee_Config &conf);
	XBee_Config& operator=(const XBee_Config &conf);
	~XBee_Config();
	
	std::string serial_port;
	std::string node;
	bool coordinator_mode;
	uint8_t unique_id;
	uint8_t *pan_id;
	uint8_t pan_length;
	uint32_t baud_rate;
	uint32_t timeout;
};

class XBee_At_Command {
public:
	XBee_At_Command(const std::string &command, const uint8_t *cmd_data, uint8_t length);
	XBee_At_Command(const std::string &command, const std::string &cmd_data);
	XBee_At_Command(const std::string &command);
	XBee_At_Command(const XBee_At_Command &cmd);
	XBee_At_Command& operator=(const XBee_At_Command &cmd);
	~XBee_At_Command();

	void set_data(const uint8_t *data, uint8_t length);
	void append_data(const uint8_t *data, uint8_t length);
	
	std::string at_command;
	uint8_t *data;
	uint8_t length;
private:

};

class XBee {
public:
	XBee(XBee_Config& config);
	virtual ~XBee();
	uint8_t xbee_init();
	uint8_t xbee_status();
	uint8_t xbee_at_command(XBee_At_Command& cmd);
	uint8_t xbee_send_to_coordinator(XBee_Message& msg);
	uint8_t xbee_send_to_node(XBee_Message& msg, const std::string &node);
	XBee_Message* xbee_receive_message();
	const XBee_Address* xbee_get_address(const std::string &node);
	int xbee_bytes_available();
	void xbee_test_msg();
private:
	XBee(const XBee&);
	XBee& operator=(const XBee&);
	uint8_t xbee_send(XBee_Message& msg, uint16_t addr16, uint32_t addr64h, uint32_t addr64l);
	uint8_t xbee_configure_device();
	GBeeFrameData& xbee_receive_and_print(uint16_t *length);
	uint8_t* at_cmd_str(const std::string at_cmd_str);
	
	XBee_Config config;
	XBee_Address *address_cache[XBEE_ADDR_CACHE_SIZE];
	uint8_t address_cache_size;
	GBee *gbee_handle;
};

class XBee_Message {
friend XBee;
public:
	XBee_Message(enum message_type type, const uint8_t *payload, uint16_t length);
	XBee_Message(const uint8_t *message);
	XBee_Message();
	XBee_Message(const XBee_Message& msg);
	XBee_Message& operator=(const XBee_Message &msg);
	~XBee_Message();
	uint8_t* get_payload(uint16_t *length);
	enum message_type get_type();
	bool is_complete();

private:
	bool append_msg(const XBee_Message &msg);
	bool append_msg(const uint8_t *data);
	uint8_t* get_msg(uint8_t part);
	uint16_t get_msg_len(uint8_t part);
	uint8_t* allocate_msg_buffer(uint8_t payload_length);

	uint8_t *message_buffer;
	uint8_t *payload;
	enum message_type type;
	uint16_t payload_len;
	uint8_t message_part;
	uint8_t message_part_cnt;
	bool message_complete;
};


#endif
