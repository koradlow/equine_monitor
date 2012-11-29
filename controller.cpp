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

#include "controller.h"
#include "xbee_if.h"
#include "ini.h"
#include <gbee.h>
#include <gbee-util.h>
#include <array> 
#include <string>
#include <unistd.h>
#include <sys/time.h>
#include <sqlite3.h>

using std::string;

void settings_test(struct Settings *s) {
	printf("%s\n", s->database_path.c_str());
	printf("%s\n", s->config_file_path.c_str());
	printf("%s\n", s->identifier.c_str());
	printf("%s\n", s->tty_port.c_str());
	printf("controller mode: %s\n", s->controller_mode ? "true" : "false");
	printf("timeout: %u\n", s->timeout);
	printf("baud: %u\n", (uint8_t) s->baud_rate);
	printf("hops: %u\n", s->max_unicast_hops);
	printf("pan: ");
	for(uint8_t i = 0; i < 8; i++)
		printf("%02x ", s->pan_id[i]);
	printf("\n");
	uint8_t pan_test[4] = {0x00, 0x01, 0x02, 0x03};
	for(uint8_t i = 0; i < 4; i++)
		printf("%02x ", pan_test[i]);
}
int main(int argc, char** argv){
	/* try to load the settings from the config file */
	struct Settings settings;
	controller_parse_cl(argc, argv, &settings);
	
	settings_test(&settings);
	
	/* setup XBee interface with settings from config file */
	XBee_Config config(settings.tty_port, settings.identifier, settings.controller_mode,
			settings.pan_id, settings.timeout, settings.baud_rate, settings.max_unicast_hops);
	//XBee interface(config);
	//if (interface.xbee_init() != GBEE_NO_ERROR) {
	//	printf("Error: unable to configure XBee device");
	//	return 0;
	//}
	//do {
	//	printf("Waiting for ZigBee Network to connect \n");
	//} while (interface.xbee_status());

	return 0;
}

void controller_usage_hint() {
	fprintf(stderr, "please give the path of the config file as the first argument\n");
}

int controller_ini_cb(void* buffer, const char* section, const char* name, const char* value) {
	#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
	struct Settings *settings = (struct Settings*) buffer;

	/* Controller Settings */
	if (MATCH("CONTROLLER", "database")) {
		if (access(value, F_OK))
			settings->database_path = string(value);
		else
			printf("Error: DB file not found\n");
	}
	
	/* ZigBee Settings */
	if (MATCH("ZIGBEE", "identifier")) 
		settings->identifier = string(value);
	else if (MATCH("ZIGBEE", "tty_port"))
		settings->tty_port = string(value);
	else if (MATCH("ZIGBEE", "controller_mode"))
		settings->controller_mode = strcmp(value, "true")? false : true;
	else if (MATCH("ZIGBEE", "timeout"))
		settings->timeout = strtol(value, 0L, 0);
	else if (MATCH("ZIGBEE", "max_unicast_hops"))
		settings->max_unicast_hops = strtol(value, 0L, 0);
	else if (MATCH("ZIGBEE", "baudrate"))
		settings->baud_rate = (xbee_baud_rate) strtol(value, 0l, 0);
	else if (MATCH("ZIGBEE", "pan_id")) {
		controller_parse_pan(settings, value);
	}
	return 0;
}

void controller_parse_pan(struct Settings *settings, const char *value)
{
	const uint8_t PAN_SIZE = 8;
	char pan_tmp[PAN_SIZE];
	char buffer[PAN_SIZE];
	int length = strlen(value);
	int pos = 0;
	int start = 0; 
	int size = 0;

	/* find sub-strings, delimited by ',' or ' ' and convert them into
	 * integer values, interpret values as hex */ 
	while(pos <= length) {
		if (value[pos] == ',' || value[pos] == ' ' || pos == length) {
			memset(buffer, 0, PAN_SIZE*sizeof(char));
			strncpy(buffer, &value[start], pos-start);
			pan_tmp[size++] = strtol(buffer, NULL, 16);
			start = pos + 1 ;
			/* PAN Id has a max length of 64 byte */	
			if (size >= PAN_SIZE)
				break;
		}
		pos++;
	}
	/* shift the data according to the length, in case the specified value
	 * in the config file is shorter than 64 byte. 
	 * Example: 0xAB 0xCD should be interpreted as 00 00 00 00 00 00 AB CD*/
	memset(settings->pan_id, 0, PAN_SIZE*sizeof(uint8_t));
	for (uint8_t i = 0; i < size; i++)
		settings->pan_id[i + PAN_SIZE - size] = pan_tmp[i];
	printf("size: %u\n", size);
}

void controller_parse_cl(int argc,char **argv, struct Settings *settings) {
	if (argc == 1) {
		controller_usage_hint();
		exit(1);
	}
	/* check if the config file exists */
	if (access(argv[1], F_OK) == -1) {
		fprintf(stderr, "Unable to open ini file: %s\n", argv[1]);
		exit(1);
	}
	/* store the config file path */
	settings->config_file_path = string(argv[1]);

	/* parse the config file */
	ini_parse(argv[1], controller_ini_cb, settings);
}
