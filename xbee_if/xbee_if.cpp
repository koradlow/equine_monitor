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
#include <unistd.h>
#include <sys/ioctl.h>


/** XBee_Address Class implementation */
/* default constructor of XBee_Address, creating an empty object */
XBee_Address::XBee_Address() :
	node(""),
	addr16(0),
	addr64h(0),
	addr64l(0)
{}

/* constructor that decodes the data returned as an reply to the AT "DI"
 * command by an XBee device */
 // TODO: decode payload data
XBee_Address::XBee_Address(const std::string &node, const uint8_t *payload) :
	node(node)
{
}

/** XBee_Config Class implementation */
/* constructor of the XBee_config class, which is used to provide access
 * to configuration options. It is a raw data container at the moment */
 /* TODO: find a good way to set baud rate for xbees */ 
XBee_Config::XBee_Config(const std::string &port, const std::string &node, bool mode, 
			uint8_t unique_id, const uint8_t *pan, uint32_t timeout):
		serial_port(port),
		node(node),
		coordinator_mode(mode),
		unique_id(unique_id),
		baud_rate(9600),
		timeout(timeout)
{
	memcpy(pan_id, pan, 8);
}

/** XBee_At_Command Class implementation */
/* constructs a XBee_At_Command object, by copying the given values into
 * an internal memory space */
XBee_At_Command::XBee_At_Command(const std::string &command, const uint8_t *cmd_data, uint8_t cmd_length) :
		at_command(command),
		length(cmd_length)
{
	data = new uint8_t[cmd_length];
	memcpy(data, cmd_data, cmd_length);
}

/* constructs a XBee_At_Command object, by translating the command string into
 * a byte array and copying it an internal memory space */
XBee_At_Command::XBee_At_Command(const std::string &command, const std::string &cmd_data) :
		at_command(command),
		length(cmd_data.length())
{
	data = new uint8_t[length];
	memcpy(data, cmd_data.c_str(), length);
}

/* constructs a XBee_At_Command object that is empty except for the actual
 * AT command, and can be used to request values */
XBee_At_Command::XBee_At_Command(const std::string &command) :
		at_command(command),
		data(NULL),
		length(0)
{}

/* copy constructor, performs a deep copy */
XBee_At_Command::XBee_At_Command(const XBee_At_Command &cmd) :
		at_command(cmd.at_command),
		length(cmd.length)
{
	data = new uint8_t[length];
	memcpy(data, cmd.data, length);
}

/* assignment operator, performs deep copy for pointer members */
XBee_At_Command& XBee_At_Command::operator=(const XBee_At_Command &cmd) {
	at_command = cmd.at_command;
	length = cmd.length;

	/* free locally allocated memory, and copy memory content from cmd.data
	 * address into new allocated memory space */
	if (data)
		delete[] data;
	data = new uint8_t[length];
	memcpy(data, cmd.data, length);

	return *this;
}

XBee_At_Command::~XBee_At_Command() {
	if (data)
		delete[] data;
}

/* frees the memory space allocated for the data, and copies the given 
 * data into the object by allocating new memory space */
void XBee_At_Command::set_data(const uint8_t *cmd_data, uint8_t cmd_length) {
	if (data)
		delete[] data;
	length = cmd_length;
	data = new uint8_t[cmd_length];
	memcpy(data, cmd_data, cmd_length); 
}

/* appends the data chunk to the existing memory space in the object.
 * This is used for AT commands with a multi frame reply */
void XBee_At_Command::append_data(const uint8_t *new_data, uint8_t length) {
	uint8_t *old_data = data;
	
	/* allocate new memory, big enough to contain the existing data and
	 * the additional new data, and copy the old data to the new memory space */
	data = new uint8_t[this->length + length];
	memcpy(data, old_data, this->length);
	
	/* append the new_data to the existing data, and update the length
	 * of the data field */
	memcpy(&data[this->length], new_data, length);
	this->length += length;
	
	/* free the old memory space */
	if (old_data)
		delete[] old_data;
}

/** XBee_Message Class implementation */
/* constructor for a XBee message - used to create messages for transmission */
XBee_Message::XBee_Message(enum message_type type, const uint8_t *payload, uint16_t length):
		type(type),
		payload_len(length),
		message_part(1),	/* message part numbers start with 1 */
		message_complete(true)	/* messages created by this constructor
					 * are complete at construction time */
{
	/* calculate the number of parts required to transmit this message */
	message_part_cnt = payload_len / (XBEE_MSG_LENGTH - MSG_HEADER_LENGTH) + 1;

	/* allocate memory to copy the payload into the object */
	this->payload = new uint8_t[payload_len];
	memcpy(this->payload, payload, payload_len);
	/* allocate memory for the message buffer */
	message_buffer = allocate_msg_buffer(payload_len);
}

/* constructor for XBee_messages - used to deserialize objects after reception */
XBee_Message::XBee_Message(const uint8_t *message):
		message_buffer(NULL),	/* this message type will not use the buffer */
		type(static_cast<message_type>(message[MSG_TYPE])),
		payload_len(message[MSG_PAYLOAD_LENGTH]),
		message_part(message[MSG_PART]),
		message_part_cnt(message[MSG_PART_CNT])
{
	/* allocate memory to copy the payload into the object */
	this->payload = new uint8_t[payload_len];
	memcpy(this->payload, &message[MSG_HEADER_LENGTH], payload_len);

	/* determine if the message is complete, or just a part of a longer
	 * message */
	if (message_part_cnt == 1)
		message_complete = true;
	else 
		message_complete = false;
}

/* constructor for a empty XBee message - used to create messages for appending data */
XBee_Message::XBee_Message():
	message_buffer(NULL),
	payload(NULL),
	payload_len(0),
	message_part(0),
	message_part_cnt(0),
	message_complete(false)
{}

/* copy constructor, performs a deep copy */
XBee_Message::XBee_Message(const XBee_Message& msg) :
	type(msg.type),
	payload_len(msg.payload_len),
	message_part(msg.message_part),
	message_part_cnt(msg.message_part_cnt),
	message_complete(msg.message_complete)
{
	/* allocate memory space for the payload and copy the data from msg */
	payload = new uint8_t[payload_len];
	memcpy(payload, msg.payload, payload_len);
	/* allocate memory for the message buffer */
	message_buffer = allocate_msg_buffer(payload_len);
}

/* assignment operator, performs deep copy for pointer members */
XBee_Message& XBee_Message::operator=(const XBee_Message& msg) {
	payload_len = msg.payload_len;
	message_part = msg.message_part;
	message_part_cnt = msg.message_part_cnt;
	message_complete = msg.message_complete;

	/* take care of pointer members */
	/* if memory was allocated in the object, free the memory */
	if (payload)
		delete[] payload;
	if (message_buffer)
		delete[] message_buffer;

	/* allocate memory space for the payload and copy the data from msg */
	payload = new uint8_t[payload_len];
	memcpy(payload, msg.payload, payload_len);
	/* allocate memory for the message buffer */
	message_buffer = allocate_msg_buffer(payload_len);

	return *this;
}

XBee_Message::~XBee_Message() {
	if (payload)
		delete[] payload;
	if (message_buffer)
		delete[] message_buffer;
}

uint8_t* XBee_Message::get_payload(uint16_t *length) {
	*length = payload_len;
	return payload;
}

enum message_type XBee_Message::get_type() {
	return type;
}

bool XBee_Message::is_complete() {
	return message_complete;
}

/* reconstructs messages that consist of multiple parts, returns true if
 * the message was successfully appended and false, if the operation failed
 * due to failed validity check */
bool XBee_Message::append_msg(const XBee_Message &msg) {
	uint8_t *new_payload;
	uint16_t new_payload_len;
	
	/* check if it's possible to append the given message */
	if (msg.message_part != message_part+1)
		return false;
	
	/* given message passed validity check -> allocate memory */
	new_payload_len = payload_len + msg.payload_len;
	new_payload = new uint8_t[new_payload_len];
	
	/* copy the existing payload into the new memory space */
	memcpy(new_payload, payload, payload_len);
	/* append the new payload to the memory */
	memcpy(&new_payload[payload_len], msg.payload, msg.payload_len);
	
	/* update internal variables to match new data */
	if (payload)
		delete[] payload;
	payload = new_payload;
	payload_len += msg.payload_len;
	message_part += 1;
	
	/* determine if the message is complete */
	if (message_part == message_part_cnt)
		message_complete = true;

	return true;
}

bool XBee_Message::append_msg(const uint8_t *data) {
	XBee_Message tmp_msg(data);
	return append_msg(tmp_msg);
}

/* returns a pointer to a message buffer that includes a header and a payload.
 * The message_buffer is constructed on the fly into a preallocated and fixed
 * memory space.
 * The content of the memory space is overwritten each time this function is called */
uint8_t* XBee_Message::get_msg(uint8_t part = 1) {
	uint16_t length = payload_len;
	uint16_t overhead_len;
	uint16_t offset = 0;

	/* check if memory was allocated for the message_buffer. Because
	 * we're working on a machine with limited memory, there is a case
	 * when no memory is allocated for the buffer during object instantiation
	 * (de-serializing object from byte stream) because these objects are only used
	 * to put received message back together */
	if (!message_buffer)
		return NULL;
	
	if (message_part_cnt > 1) {
		/* calculate the length of the payload in last message part */
		overhead_len = length - (message_part_cnt - 1) * (XBEE_MSG_LENGTH - MSG_HEADER_LENGTH);
		/* payload length depends on the part number of the message -> 
		 * last message part is an exception */
		length = (part == message_part_cnt)? overhead_len : (XBEE_MSG_LENGTH - MSG_HEADER_LENGTH);
		/* offset in the payload data based on message part */
		offset = (part - 1) * (XBEE_MSG_LENGTH - MSG_HEADER_LENGTH);
	}
	/* create the header of the message */
	message_buffer[MSG_TYPE] = static_cast<uint8_t>(type);
	message_buffer[MSG_PART] = part;
	message_buffer[MSG_PART_CNT] = message_part_cnt;
	message_buffer[MSG_PAYLOAD_LENGTH] = length;
	/* copy payload into message body */
	memcpy(&message_buffer[MSG_HEADER_LENGTH], &payload[offset], length);

	return message_buffer;
}

/* returns the length of the requested part of the message (including header) */
uint16_t XBee_Message::get_msg_len(uint8_t part = 1) {
	/* check if memory was allocated for the message_buffer, and see
	 * get_msg function for explanation */
	if (!message_buffer)
		return 0;

	/* message consists of one part? */
	if (message_part_cnt == 1)
		return (MSG_HEADER_LENGTH + payload_len);

	/* message consists of multiple parts, part in the middle requested.
	 * Parts in the middle always have the maximal possible message length
	 * to make best use of bandwidth */
	if (message_part_cnt != part)
		return XBEE_MSG_LENGTH;

	/* message consists of multiple parts, last part requested */
	uint16_t transmitted_len = (message_part_cnt - 1) * (XBEE_MSG_LENGTH - MSG_HEADER_LENGTH);
	return MSG_HEADER_LENGTH + payload_len - transmitted_len;
}

/* allocates memory in for the message buffer in a XBee_Message object.
 * The size of the memory depends on the the fact if the payload will fit
 * into one transmission or has to be split up */
uint8_t* XBee_Message::allocate_msg_buffer(uint8_t payload_len) {
	uint8_t *message_buffer;
	uint8_t msg_part_cnt;
	
	/* calculate the number of parts required to transmit this message */
	msg_part_cnt = payload_len / (XBEE_MSG_LENGTH - MSG_HEADER_LENGTH) + 1;
	
	/* allocate memory for the message buffer */
	if (msg_part_cnt > 1) {
		/* message has to be split into multiple parts, but each
		 * single part will not be larger thatn the maximal msg lengh */
		message_buffer = new uint8_t[XBEE_MSG_LENGTH];
	} else {
		/* message fits into one transmission */
		message_buffer = new uint8_t[payload_len + MSG_HEADER_LENGTH];
	}

	return message_buffer;
}
 
/** XBee Class implementation */
XBee::XBee(XBee_Config& config) :
	config(config),
	address_cache_size(0)
{}

XBee::~XBee() {
	gbeeDestroy(gbee_handle);
	for (int i = 0; i < address_cache_size; i++)
		delete address_cache[i];
}

/* the init function initializes the internally used libgbee library by creating
 * a handle for the xbee device */
uint8_t XBee::xbee_init() {
	gbee_handle = gbeeCreate(config.serial_port.c_str());
	if (!gbee_handle) {
		printf("Error creating handle for XBee device\n");
		exit(-1);
	}

	/* TODO: check if device is operating in API mode: the functions provided by
	 * libgbee (gbeeGetMode, gbeeSetMode) cannot be used, because they rely
	 * on the AT mode of the devices which is not working with the current
	 * Firmware version */
	return xbee_configure_device();
}

/* the configure device function sets the basic parameters for the XBee modules,
 * according to the values found in the XBee_Config object.
 * It will read the register values from the device and compare them with the
 * desired values, updating them if a mismatch is detected. If a register was
 * updated the changes are written into the nonvolatile memory of the device */
 // TODO: Configure Sleep Mode for Coordinator / End Devices
uint8_t XBee::xbee_configure_device() {
	uint8_t error_code;
	bool register_updated = false;

	printf("Validating device configuration \n"); 
	
	/* check the 64bit PAN ID */
	XBee_At_Command cmd("ID");
	error_code = xbee_at_command(cmd);
	if (error_code != GBEE_NO_ERROR)
		return error_code;
	if (memcmp(cmd.data, config.pan_id, 8)) {
		printf("Setting PAN ID\n");
		XBee_At_Command cmd_pan("ID", config.pan_id, 8);
		xbee_at_command(cmd_pan);
		register_updated = true;
	}
	
	/* check the Node Identifier */
	cmd = XBee_At_Command("NI");
	error_code = xbee_at_command(cmd);
	if (error_code != GBEE_NO_ERROR)
		return error_code;
	if (memcmp(cmd.data, config.node.c_str(), config.node.length())) {
		printf("Setting Node Identifier\n");
		XBee_At_Command cmd_ni("NI", config.node);
		xbee_at_command(cmd_ni);
		register_updated = true;
	}

	if (register_updated) {
		/* write the changes to the internal memory of the xbee module */
		cmd = XBee_At_Command("WR");
		error_code = xbee_at_command(cmd);
		if (error_code != GBEE_NO_ERROR)
			return error_code;

		/* apply the queued changes */
		cmd = XBee_At_Command("AC");
		error_code = xbee_at_command(cmd);
		if (error_code != GBEE_NO_ERROR)
			return error_code;
	}
	return GBEE_NO_ERROR;
}

/* xbee_status requests, decodes and prints the current status of the XBee module */
uint8_t XBee::xbee_status() {
	GBeeFrameData frame;
	GBeeError error_code;
	uint16_t length;
	uint8_t frame_id = 3;	/* used to identify response frame */
	uint8_t status = 0xFE;	/* Unknown Status */
	
	/* query the current network status and print the response in cleartext */
	error_code = gbeeSendAtCommand(gbee_handle, frame_id, at_cmd_str("AI"), NULL, 0);
	if (error_code != GBEE_NO_ERROR) {
		printf("Error requesting XBee status: %s\n", gbeeUtilCodeToString(error_code));
		return status;
	}
	/* wait for the response to the command */
	frame = xbee_receive_and_print(&length);
	if (frame.ident == GBEE_AT_COMMAND_RESPONSE) {
		GBeeAtCommandResponse *at_frame = (GBeeAtCommandResponse*) &frame;
		status = at_frame->value[0];
		printf("Status: %s\n", gbeeUtilStatusCodeToString(status));
	}

	return status;
}

/* sends out the requested AT command, receives & stores the register value 
 * in the XBee_At_Command object */
uint8_t XBee::xbee_at_command(XBee_At_Command& cmd){
	GBeeFrameData frame;
	GBeeError error_code;
	uint8_t response_cnt = 0;
	uint16_t length = 0;
	uint32_t timeout = config.timeout;
	static uint8_t frame_id;
	
	memset(&frame, 0, sizeof(frame));

	/* send the AT command & data to the device */
	frame_id = (frame_id % 255) + 1;	/* give each frame a unique ID */
	error_code = gbeeSendAtCommand(gbee_handle, frame_id, at_cmd_str(cmd.at_command), cmd.data, cmd.length);
	if (error_code != GBEE_NO_ERROR) {
		printf("Error sending XBee AT (%s) command : %s\n", cmd.at_command.c_str(),
		gbeeUtilCodeToString(error_code));
		return error_code;
	}
	/* try to receive the response and copy it into the XBee_At_Command object*/
	do {
		error_code = gbeeReceive(gbee_handle, &frame, &length, &timeout);
		if (error_code != GBEE_NO_ERROR)
			return error_code;
		
		/* check if the received frame is a AT Command Response frame */
		if (frame.ident == GBEE_AT_COMMAND_RESPONSE) {
			GBeeAtCommandResponse *at_frame = (GBeeAtCommandResponse*) &frame;
			if (at_frame->frameId != frame_id) {
				printf("Problem: Frame IDs not matching\n (%i : %i)\n",
				frame_id, at_frame->frameId);
				error_code = GBEE_RESPONSE_ERROR;
				/* if the frameId is larger than expected nothing can be done */
				if (frame_id < at_frame->frameId)
					break;
				/* if it's smaller, wait for a while and try again */
				sleep(1);
				continue;
			}
			/* copy the response payload into the XBee_At_Command object.
			 * This frame type has an overhead of 5 bytes that are counted
			 * as part of the length. */
			if (response_cnt++ < 1)
				cmd.set_data(at_frame->value, length - 5);
			else
				cmd.append_data(at_frame->value, length - 5);
		/* Modem Status frames can be transmitted at arbitrary times,
		 * as long as there's no message queue, we have to handle them here */
		} else if (frame.ident == GBEE_MODEM_STATUS) {
			GBeeModemStatus *status_frame = (GBeeModemStatus*) &frame;
			printf("Received Modem status: %02x\n", status_frame->status);
			sleep(1);
		}
	} while (xbee_bytes_available() > 0);
	/* in case there was an unexpected error, the function returned early
	 * the only way we get this far, is if everything */
	return GBEE_NO_ERROR;
}

/* sends the data in the message object to the coordinator */
uint8_t XBee::xbee_send_to_coordinator(XBee_Message& msg) {
	/* coordinator can be addressed by setting the 64bit destination
	 * address to all zeros and the 16bit address to 0xFFFE */
	uint16_t addr16 = 0xFFFE;
	return xbee_send(msg, addr16, 0,0);
}

/* sends the data in the message object to a Network Node */
uint8_t XBee::xbee_send_to_node(XBee_Message& msg, const std::string &node) {
	const XBee_Address *addr = xbee_get_address(node);
	if (!addr)
		return GBEE_TIMEOUT_ERROR;	/* node couldn't be found in network */
	return xbee_send(msg, addr->addr16, addr->addr64h, addr->addr64l);
}

/* checks the buffer for (parts of) messages, puts together a complete message
 * from the parts */
XBee_Message* XBee::xbee_receive_message() {
	GBeeFrameData frame;
	GBeeError error_code;
	XBee_Message *msg = NULL;
	uint16_t length = 0;
	uint32_t timeout = config.timeout;
	memset(&frame, 0, sizeof(frame));

	/* try to receive a message, it might consist of several parts */
	do {
		error_code = gbeeReceive(gbee_handle, &frame, &length, &timeout);
		if (error_code != GBEE_NO_ERROR) {
			printf("Error receiving message: length=%d, error= %s\n",
			length, gbeeUtilCodeToString(error_code));
			break;
		}
		/* check if the received frame is a RxPacket frame */
		if (frame.ident == GBEE_RX_PACKET) {
			GBeeRxPacket *rx_frame = (GBeeRxPacket*) &frame;
			/* 12 bytes of overhead data */
			printf("Received message from Node %04x\n", rx_frame->srcAddr16);
			printf("SH: %08x, SL: %08x\n", rx_frame->srcAddr64h, rx_frame->srcAddr64l);
			if (!msg)
				msg = new XBee_Message(rx_frame->data);
			else
				msg->append_msg(rx_frame->data);
		}
		else {
			printf("Received unexpected message frame: ident=%02x\n",frame.ident);
			break;
		}
	} while (!msg->is_complete());

	return msg;
}

/* returns a reference to an address object, that contains the current network 
 * address of the node identified by the string */
const XBee_Address* XBee::xbee_get_address(const std::string &node) {
	uint8_t error_code;

	/* check for cached addresses */
	for (int i = 0; i < address_cache_size; i++) {
		if (address_cache[i]->node == node)
			return address_cache[i];
	}
	/* address not cached -> do a destination node lookup */
	XBee_At_Command cmd("DN", config.node); 
	error_code = xbee_at_command(cmd);
	if (error_code != GBEE_NO_ERROR) {
		printf("Node discovery failed, error: %s\n", gbeeUtilCodeToString((gbeeError)error_code));
		return NULL;
	}
	/* decode the returned data and add the address to the cache */
	XBee_Address *new_address = new XBee_Address(node, cmd.data);
	address_cache[address_cache_size++] = new_address;
	
	return new_address;
}

/* checks the buffer of the serial device for available data, and returns the 
 * number of pending bytes */
int XBee::xbee_bytes_available() {
	int bytes_available;
	ioctl(gbee_handle->serialDevice, FIONREAD, &bytes_available);

	return bytes_available;
}

GBeeFrameData& XBee::xbee_receive_and_print(uint16_t *length) {
	static GBeeFrameData frame;
	GBeeError error_code;
	uint32_t timeout = config.timeout;
	
	memset(&frame, 0, sizeof(GBeeFrameData));
	error_code = gbeeReceive(gbee_handle, &frame, length, &timeout);
	if (error_code == GBEE_NO_ERROR) {
		return frame;
	}
	printf("Error receiving package: %s\n", gbeeUtilCodeToString(error_code));

	return frame;
}

uint8_t XBee::xbee_send(XBee_Message& msg, uint16_t addr16, uint32_t addr64h, uint32_t addr64l) {
	GBeeFrameData frame;
	GBeeError error_code;
	uint8_t options = 0x00;	/* 0x01 = Disable ACK, 0x20 - Enable APS
				 * encryption (if EE=1), 0x04 = Send packet
				 * with Broadcast Pan ID.
				 * All other bits must be set to 0. */
	uint8_t tx_status = 0xFF;	/* -> Unknown Tx Status */
	uint8_t bcast_radius = 0;	/* -> max hops for bcast transmission */
	uint16_t length;
	/* coordinator can be addressed by setting the 64bit destination
	 * address to all zeros and the 16bit address to 0xFFFE */
	uint32_t timeout = config.timeout;
	static uint8_t frame_id;;
	memset(&frame, 0, sizeof(frame));

	/* send the message, by splitting it up into parts that have the 
	 * correct length for transmission over ZigBee */
	for (uint8_t i = 1; i <= msg.message_part_cnt; i++) {
		/* send out one part of the message */
		frame_id = (frame_id % 255) + 1;	/* give each frame a unique ID */
		error_code = gbeeSendTxRequest(gbee_handle, frame_id, addr64h, addr64l,
		addr16, bcast_radius, options, msg.get_msg(i), msg.get_msg_len(i));
		if (error_code != GBEE_NO_ERROR) {
			printf("Error sending message part %u of %u: %s\n",
			i, msg.message_part_cnt, gbeeUtilCodeToString(error_code));
			tx_status = 0xFF;	/* -> Unknown Tx Status */
			break;
		}
		
		/* check the transmission status of message part*/
		error_code = gbeeReceive(gbee_handle, &frame, &length, &timeout);
		if (error_code != GBEE_NO_ERROR) {
			printf("Error receiving transmission, status message: error= %s\n",
			gbeeUtilCodeToString(error_code));
			tx_status = 0xFF;	/* -> Unknown Tx Status */
			break;
		/* check if the received frame is a TxStatus frame */
		} else if (frame.ident == GBEE_TX_STATUS_NEW) {
			GBeeTxStatusNew *tx_frame = (GBeeTxStatusNew*) &frame;
			printf("Transmission status: %s\n",
			gbeeUtilTxStatusCodeToString(tx_frame->deliveryStatus));
			tx_status = tx_frame->deliveryStatus; 
		}
	}

	return tx_status;
}

/* converts a std::string into a ASCII coded byte array - the length of
 * the byte array is fixed to a length of an AT command (2 chars) */
uint8_t* XBee::at_cmd_str(const std::string at_cmd_str) {
	static uint8_t at_cmd[2];
	memcpy(at_cmd, at_cmd_str.c_str(), 2);
	return &at_cmd[0];
}


#define DTA_SIZE 400
void XBee::xbee_test_msg() {
	uint8_t test_data[DTA_SIZE];
	uint8_t *payload;
	uint16_t length;

	/* create random test data */
	for (int i = 0; i < DTA_SIZE; i++) {
		test_data[i] = rand() % 255;
		printf("%02x ", test_data[i]);
	}
	printf("\n");
	XBee_Message msg(DATA, test_data, DTA_SIZE);
	XBee_Message msg_des;
	
	for (int i = 1; i <= msg.message_part_cnt; i++) {
		msg_des.append_msg(msg.get_msg(i));
	}
	
	payload = msg_des.get_payload(&length);
	for (int i = 0; i < length; i++)
		printf("%02x ",payload[i]);
	printf("\n");
	
	/* test the util function for reading a register value */
	GBeeError error_code;
	uint8_t value[20];
	uint16_t ln;
	uint16_t max_length = 20;
	error_code = gbeeUtilReadRegister(gbee_handle, "MY", value, &ln, max_length);
	printf("Status: %s\n", gbeeUtilCodeToString(error_code));
	printf("MY: %02x %02x, ln: %u\n", value[0], value[1], ln);
}
