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


XBee_config::XBee_config(std::string port, bool mode, const uint8_t pan[8]):
	serial_port(port),
	coordinator_mode(mode) {
		memcpy(pan_id, pan, 8);
	
}

XBee::XBee(XBee_config& config) :
	config(config) {}

XBee::~XBee() {
	gbeeDestroy(gbee_handle);
}

/* the init function initializes the internally used libgbee library by creating
 * a handle for the xbee device.
 * The function also checks that the xbee device is in the correct mode (API) */
void XBee::xbee_init() {
	GBeeError error_code;
	GBeeMode mode;

	//gbee_handle = gbeeCreate(config.serial_port.c_str());
	gbee_handle = gbeeCreate("/dev/ttyUSB0");
	if (!gbee_handle) {
		printf("Error creating handle for XBee device\n");
		// TODO: throw exception instead of exiting
		exit(-1);
	}
	sleep(1);
	/* check if device is operating in API mode */
	error_code = gbeeGetMode(gbee_handle, &mode); 
	if (error_code != GBEE_NO_ERROR)
		printf("Error getting XBee mode: %s\n", gbeeUtilCodeToString(error_code));
	if (mode == GBEE_MODE_TRANSPARENT) {
		printf("Xbee module in Transparent mode, switching to API mode\n");
		mode = GBEE_MODE_API;
		error_code = gbeeSetMode(gbee_handle, mode); 
	} else if (mode == GBEE_MODE_API) {
		printf("Xbee module in API mode\n");
	}
}

void XBee::xbee_start_network() {
	GBeeError error_code;
	uint16_t length;
	uint8_t value[64];

	/* set the 64bit PAN ID */
	error_code = gbeeSendAtCommandQueue(gbee_handle, 0, xbee_at_cmd("ID"), &config.pan_id[0], 8);
	printf("Set 64bit PAN ID: %s\n", gbeeUtilCodeToString(error_code));
	xbee_receive_and_print(250, &length);
	
	/* enable sleep mode for end devices, disable for controllers */
	if (config.coordinator_mode) {
		value[0] = 0x00;
		error_code = gbeeSendAtCommandQueue(gbee_handle, 0, xbee_at_cmd("SL"), &value[0], 1); 
	} else {
		value[0] = 0x04; /* cyclic sleep */
		error_code = gbeeSendAtCommandQueue(gbee_handle, 0, xbee_at_cmd("SL"), &value[0], 1); 
		
	}
	printf("%s sleep mode: %s\n", (config.coordinator_mode)? "Disabled" : "Enabled", gbeeUtilCodeToString(error_code));
	xbee_receive_and_print(250, &length);

	/* if configuring end device, set the destination address registers
	 * to the default address of the coordinator */
	if (!config.coordinator_mode) {
		value[0] = 0x00; 
		error_code = gbeeSendAtCommandQueue(gbee_handle, 0, xbee_at_cmd("DH"), &value[0], 1);
		printf("Set DH Address: %s\n", gbeeUtilCodeToString(error_code)); 
		xbee_receive_and_print(250, &length);
		error_code = gbeeSendAtCommandQueue(gbee_handle, 0, xbee_at_cmd("DL"), &value[0], 1); 
		printf("Set DL Address: %s\n", gbeeUtilCodeToString(error_code)); 
		xbee_receive_and_print(250, &length);
	}

	/* write the changes to the internal memory of the xbee module */
	error_code = gbeeSendAtCommandQueue(gbee_handle, 0, xbee_at_cmd("WR"), &value[0], 0);
	printf("Write settings to memory: %s\n", gbeeUtilCodeToString(error_code)); 
	// wait for 'OK'
	xbee_receive_and_print(500, &length);

	/* apply the queued changes */
	error_code = gbeeSendAtCommandQueue(gbee_handle, 0, xbee_at_cmd("AC"), &value[0], 0);
	printf("Apply queued changes: %s\n", gbeeUtilCodeToString(error_code)); 
	// wait for 'Associated'
	xbee_receive_and_print(500, &length);
}

void XBee::xbee_status() {
	GBeeFrameData frame;
	GBeeError error_code;
	uint16_t length;
	uint8_t frame_id = 3;	/* used to identify response frame */

	/* query the current network status and print the response in cleartext */
	error_code = gbeeSendAtCommand(gbee_handle, frame_id, xbee_at_cmd("AI"), NULL, 0);
	if (error_code != GBEE_NO_ERROR)
		printf("Error requesting XBee status: %s\n", gbeeUtilCodeToString(error_code));
	
	frame = xbee_receive_and_print(10000, &length);
	if (frame.ident == 0x88 ) {
		GBeeAtCommandResponse *at_frame = (GBeeAtCommandResponse*) &frame;
		printf("Received %c%c Status frame with length: %d and status: %d\n", 
		at_frame->atCommand[0], at_frame->atCommand[1],length, at_frame->status);
		switch(at_frame->value[0]) {
		case 0x00:
			printf("Status: Successfully formed or joined a network\n");
			break;
		case 0x21:
			printf("Status: Scan found no PANs\n");
			break;
		case 0x22:
			printf("Status: Scan found no valid PANs based on current SC and ID settings\n");
			break;
		case 0x23:
			printf("Status: Valid Coordinator or Routers found, but they are not allowing joining (NJ expired)\n");
			break;
		case 0x24:
			printf("Status: No joinable beacons were found\n");
			break;
		case 0x25:
			printf("Status: Unexpected state, node should not be attempting to join at this time\n");
			break;
		case 0x27:
			printf("Status: Node Joining attempt failed (typically due to incompatible security settings)\n");
			break;
		case 0x2A:
			printf("Status: Coordinator Start attempt failed\n");
			break;
		case 0x2B:
			printf("Status: Checking for an existing coordinator\n");
			break;
		case 0x2C:
			printf("Status: Attempt to leave the network failed\n");
			break;
		case 0xAB:
			printf("Status: Attempted to join a device that did not respond\n");
			break;
		case 0xAC:
			printf("Status: Secure join error - network security key received unsecured\n");
			break;
		case 0xAD:
			printf("Status: Secure join error - network security key not received\n");
			break;
		case 0xAF:
			printf("Status: Secure join error - joining device does not have the right preconfigured link key\n");
			break;
		case 0xFF:
			printf("Status: Scanning for a ZigBee network (routers and end devices)\n");
			break;
		default:
			printf("Status: undefined status received: %02x\n", at_frame->value[0]);
		}
	}
}

GBeeFrameData& XBee::xbee_receive_and_print(uint32_t timeout, uint16_t *length) {
	static GBeeFrameData frame;
	GBeeError error_code;
	
	error_code = gbeeReceive(gbee_handle, &frame, length, &timeout);
	if (error_code == GBEE_NO_ERROR) {
		printf("Received package with length %d and type %02x\n", *length, frame.ident);
		return frame;
	}
	printf("Error receiving package: %s\n", gbeeUtilCodeToString(error_code));
	
	return frame;
}

uint8_t* XBee::xbee_at_cmd(const std::string at_cmd_str) {
	static uint8_t at_cmd[2];
	memcpy(at_cmd, at_cmd_str.c_str(), 2);
	return &at_cmd[0];
}
