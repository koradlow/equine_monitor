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


/* constructor of the XBee_config class, which is used to provide access
 * to configuration options. It is a raw data container at the moment */
XBee_config::XBee_config(std::string port, bool mode, const uint8_t unique_id,
			const uint8_t pan[2], uint32_t timeout):
	serial_port(port),
	coordinator_mode(mode),
	unique_id(unique_id),
	timeout(timeout) {
		memcpy(pan_id, pan, 2);
}

/* constructor of the XBee_measurement class, which is used to pass measurement
 * data into and out of the libgebee wrapper library */
XBee_measurement::XBee_measurement(enum sensor_type type, const uint8_t* data, 
			uint16_t length ):
	type(type),
	length(length)
	{
		this->data = new uint8_t[length];
		memcpy(this->data, data, length);

}

XBee::XBee(XBee_config& config) :
	config(config) {}

XBee::~XBee() {
	gbeeDestroy(gbee_handle);
}

/* the init function initializes the internally used libgbee library by creating
 * a handle for the xbee device */
void XBee::xbee_init() {
	gbee_handle = gbeeCreate(config.serial_port.c_str());
	if (!gbee_handle) {
		printf("Error creating handle for XBee device\n");
		// TODO: throw exception instead of exiting
		exit(-1);
	}
	sleep(1);

	/* TODO: check if device is operating in API mode: the functions provided by
	 * libgbee (gbeeGetMode, gbeeSetMode) cannot be used, because they rely
	 * on the AT mode of the devices which is not working with the current
	 * Firmware version */
}

/* the start network function sets the basic parameters for the ZigBee network,
 * it only needs to be called the first time working with new XBee modules,
 * because the parameters are stored in the device memory */
void XBee::xbee_start_network() {
	GBeeError error_code;
	uint16_t length;
	uint8_t value[64];

	printf("Setting constant parameters for new Network \n"); 
	
	/* set the 64bit PAN ID */
	error_code = gbeeSendAtCommandQueue(gbee_handle, 0, xbee_at_cmd("ID"),
	&config.pan_id[0], 2);
	printf("Set 64bit PAN ID: %s\n", gbeeUtilCodeToString(error_code));
	xbee_receive_and_print(&length);
	
	/* enable sleep mode for end devices, disable for controllers */
	if (config.coordinator_mode) {
		value[0] = 0x00;
		error_code = gbeeSendAtCommandQueue(gbee_handle, 0,
		xbee_at_cmd("SL"), &value[0], 1); 
	} else {
		value[0] = 0x04; /* cyclic sleep */
		error_code = gbeeSendAtCommandQueue(gbee_handle, 0,
		xbee_at_cmd("SL"), &value[0], 1); 
		
	}
	printf("%s sleep mode: %s\n", (config.coordinator_mode)?
	"Disabled" : "Enabled", gbeeUtilCodeToString(error_code));
	xbee_receive_and_print(&length);

	/* if configuring end device, set the destination address registers
	 * to the default address of the coordinator */
	if (!config.coordinator_mode) {
		value[0] = 0x00; 
		error_code = gbeeSendAtCommandQueue(gbee_handle, 0, xbee_at_cmd("DH"), &value[0], 1);
		printf("Set DH Address: %s\n", gbeeUtilCodeToString(error_code)); 
		xbee_receive_and_print(&length);
		error_code = gbeeSendAtCommandQueue(gbee_handle, 0, xbee_at_cmd("DL"), &value[0], 1); 
		printf("Set DL Address: %s\n", gbeeUtilCodeToString(error_code)); 
		xbee_receive_and_print(&length);
	}

	/* write the changes to the internal memory of the xbee module */
	error_code = gbeeSendAtCommandQueue(gbee_handle, 0, xbee_at_cmd("WR"), &value[0], 0);
	printf("Write settings to memory: %s\n", gbeeUtilCodeToString(error_code)); 
	// wait for 'OK'
	xbee_receive_and_print(&length);

	/* apply the queued changes */
	error_code = gbeeSendAtCommandQueue(gbee_handle, 0, xbee_at_cmd("AC"), &value[0], 0);
	printf("Apply queued changes: %s\n", gbeeUtilCodeToString(error_code)); 
	// wait for 'Associated'
	xbee_receive_and_print(&length);
}

/* xbee_status requests, decodes and prints the current status of the XBee module */
uint8_t XBee::xbee_status() {
	GBeeFrameData frame;
	GBeeError error_code;
	uint16_t length;
	uint8_t frame_id = 3;	/* used to identify response frame */
	uint8_t status = 0xFE;	/* Unknown Status */
	
	/* query the current network status and print the response in cleartext */
	error_code = gbeeSendAtCommand(gbee_handle, frame_id, xbee_at_cmd("AI"), NULL, 0);
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

/* sends out the requested AT command, receives for the answer and prints the value */
void XBee::xbee_print_at_value(std::string at){
	GBeeFrameData frame;
	GBeeError error_code;
	uint16_t length;
	uint16_t payload;
	uint32_t timeout = config.timeout;
	static uint8_t frame_id = 1;
	
	memset(&frame, 0, sizeof(frame));
	length = 0;
	/* query the current network status and print the response in cleartext */
	frame_id = (frame_id % 255) + 1;	/* give each frame a unique ID */
	error_code = gbeeSendAtCommand(gbee_handle, frame_id, xbee_at_cmd(at.c_str()), NULL, 0);
	if (error_code != GBEE_NO_ERROR)
		printf("Error sending XBee AT (%s) command : %s\n", at.c_str(),
		gbeeUtilCodeToString(error_code));

	/* receive the response and print the value */
	do {
		error_code = gbeeReceive(gbee_handle, &frame, &length, &timeout);
		if (error_code != GBEE_NO_ERROR)
			break;
		/* check if the received frame is a AT Command Response frame */
		if (frame.ident == GBEE_AT_COMMAND_RESPONSE) {
			GBeeAtCommandResponse *at_frame = (GBeeAtCommandResponse*) &frame;
			payload = length - 5;	/* 5 bytes overhead */
			if (at_frame->frameId != frame_id) {
				printf("Error: Frame IDs not matching\n (%i : %i)\n",
				frame_id, at_frame->frameId);
			}
			printf("Received AT (%c%c) response with length: %d, status: %d and value: ", 
			at_frame->atCommand[0], at_frame->atCommand[1],length, at_frame->status);
			for (int i = payload-1; i >= 0; i--)
				printf("%02x",at_frame->value[i]);
			printf("\n");
		}
	} while (error_code == GBEE_NO_ERROR);
}

/* sends the data in the measurement object to the coordinator */
uint8_t XBee::xbee_send_measurement(XBee_measurement& measurement) {
	GBeeFrameData frame;
	GBeeError error_code;
	uint8_t options = 0x00;	/* 0x01 = Disable ACK, 0x20 - Enable APS
				 * encryption (if EE=1), 0x04 = Send packet
				 * with Broadcast Pan ID.
				 * All other bits must be set to 0. */
	uint8_t *buffer = new uint8_t[measurement.length+1];
	uint8_t frame_id = 0x02;
	uint16_t length;
	uint32_t timeout = config.timeout;
	memset(&frame, 0, sizeof(frame));
	
	/* copy the msg type into the first byte of the buffer, and append
	 * the measurement data */
	buffer[0] = measurement.type;
	memcpy(&buffer[1], measurement.data, measurement.length);

	/* coordinator can be addresses by setting the 64bit destination 
	 * address to all zeros and the 16bit address to 0xFFFE 
	 * address 0xFFFF is the broadcast address */
	error_code = gbeeSendTxRequest(gbee_handle, frame_id, 0, 0,
		0xFFFE, 0, options, buffer, measurement.length+1);
	delete[] buffer;
	if (error_code != GBEE_NO_ERROR)
		printf("Error sending measurement data: %s\n",
		gbeeUtilCodeToString(error_code));

	/* check the transmission status */
	error_code = gbeeReceive(gbee_handle, &frame, &length, &timeout);
	if (error_code != GBEE_NO_ERROR) {
		printf("Error receiving transmission status message: error= %s\n",
		gbeeUtilCodeToString(error_code));
	/* check if the received frame is a TxStatus frame */
	} else if (frame.ident == GBEE_TX_STATUS_NEW) {
		GBeeTxStatusNew *tx_frame = (GBeeTxStatusNew*) &frame;
		printf("Transmission status: %s\n",
		gbeeUtilTxStatusCodeToString(tx_frame->deliveryStatus));
		return tx_frame->deliveryStatus; 
	}
	return 0xFF;	/* -> Unknown Tx Status */
}

/* checks the buffer for measurement messages, decodes them and stores the
 * contained data in the database */
void XBee::xbee_receive_measurement() {
	GBeeFrameData frame;
	GBeeError error_code;
	uint16_t length = 0;
	uint16_t payload = 0;
	uint32_t timeout = config.timeout;
	memset(&frame, 0, sizeof(frame));

	/* try to receive a message */
	error_code = gbeeReceive(gbee_handle, &frame, &length, &timeout);
	if (error_code != GBEE_NO_ERROR) {
		printf("Error receiving measurement message: length=%d, error= %s\n",
		length, gbeeUtilCodeToString(error_code));
		return;
	}
	/* check if the received frame is a RxPacket frame */
	if (frame.ident == GBEE_RX_PACKET) {
		GBeeRxPacket *rx_frame = (GBeeRxPacket*) &frame;
		payload = length - 12;	/* 12 bytes of overhead data */
		printf("Received Measurement message from Node %04x\n", rx_frame->srcAddr16);
		printf("SH: %08x, SL: %08x\n", rx_frame->srcAddr64h, rx_frame->srcAddr64l);
		printf("Sensor type: %d, length: %d\n", rx_frame->data[0], payload);
		printf("Values: %02x, %02x\n", rx_frame->data[1], rx_frame->data[2]);
	}
	else {
		printf("Received unexpected message frame: ident=%02x\n",frame.ident);
	}
	
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
		//printf("Received package with length %d and type %02x\n", *length, frame.ident);
		return frame;
	}
	printf("Error receiving package: %s\n", gbeeUtilCodeToString(error_code));

	return frame;
}

/* converts a std::string into a ASCII coded byte array - the length of
 * the byte array is fixed to a length of an AT command (2 chars) */
uint8_t* XBee::xbee_at_cmd(const std::string at_cmd_str) {
	static uint8_t at_cmd[2];
	memcpy(at_cmd, at_cmd_str.c_str(), 2);
	return &at_cmd[0];
}
