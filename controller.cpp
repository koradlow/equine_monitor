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
#include <signal.h>
#include <sstream>

using std::string;
using std::stringstream;

static sqlite3 *db;

static void signal_handler_interrupt(int signum);

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
}

int main(int argc, char** argv){
	/* register signal handler for interrupt signal, to exit gracefully */
	signal(SIGINT, signal_handler_interrupt);
	
	/* try to load the settings from the config file */
	struct Settings settings;
	controller_parse_cl(argc, argv, &settings);
	
	settings_test(&settings);
	
	/* setup XBee interface with settings from config file */
	/*
	XBee_Config config(settings.tty_port, settings.identifier, settings.controller_mode,
			settings.pan_id, settings.timeout, settings.baud_rate, settings.max_unicast_hops);
	XBee interface(config);
	if (interface.xbee_init() != GBEE_NO_ERROR) {
		printf("Error: unable to configure XBee device");
		return 0;
	}
	while (interface.xbee_status());
		printf("Successfully formed or joined ZigBee Network\n");
	*/
	/* connect to the database, and set it up */
	int error_code;
	sqlite3 *db;
	error_code = sqlite3_open(settings.database_path.c_str(), &db);
	if (error_code) {
		printf("Error: cannot open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return 1;
	}
	create_db_tables(db);
	
	/* system initialization complete - start main control loop */
	Message_Storage database;
	while (1) {
		XBee_Message *msg = NULL;
		/* try to decode a message if there's data in the receive buffer */
		//if (interface.xbee_bytes_available()) {
		//	msg = interface.xbee_receive_message();
		//}
		/* if a message was decoded, store it in the database */
		if (msg)
			database.store_msg(db, msg);
		
		// TODO: figure out if the configuration of one of the nodes
		// was changed, and update it accordingly
		usleep(200);
	}

	sqlite3_close(db);
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

/* creates the neccessary tables for storing sensor data, node addresses and
 * configuration options in the database */
void create_db_tables(sqlite3 *db) {
	sqlite3_stmt *stmt;
	
	/* define common SQL command substrings */
	string create = "CREATE TABLE IF NOT EXISTS ";
	string common_columns = "(addr64 UNSIGNED BIGINT, timestamp UNSIGNED INT, offset_ms UNSIGNED INT,";
	
	/* define the table names */
	string table_sensor_heart_name = "sensorHeart";
	string table_sensor_temperature_name = "sensorTemperature";
	string table_sensor_accel_name = "sensorAccel";
	string table_sensor_gps_name = "sensorGPS";
	
	/* concatenate the first part of the SQL command */
	string table_heart = create + table_sensor_heart_name + common_columns;
	string table_temperature = create + table_sensor_temperature_name + common_columns;
	string table_accel = create + table_sensor_accel_name + common_columns;
	string table_gps = create + table_sensor_gps_name + common_columns;
	
	/* append the custom fields of the table to the SQL command */
	table_heart += "bmp INT)";
	table_temperature += "temp DOUBLE)";
	table_accel += "x INT, y INT, z INT)";
	table_gps += "lat_h INT, lat_min INT, lat_s INT, lat_north BOOL, long_h INT, long_min INT, long_s INT, long_west BOOL, valid_pos_fix BOOL)";
	
	printf("%s \n", table_heart.c_str());
	printf("%s \n", table_temperature.c_str());
	printf("%s \n", table_accel.c_str());
	printf("%s \n", table_gps.c_str());
	
	/* try to create the tables */
	sqlite3_exec(db, table_heart.c_str(), 0, 0, 0);
	sqlite3_exec(db, table_temperature.c_str(), 0, 0, 0);
	sqlite3_exec(db, table_accel.c_str(), 0, 0, 0);
	sqlite3_exec(db, table_gps.c_str(), 0, 0, 0);
}

/* decodes the type of the network message and passes it on the appropriate 
 * decoder function */
void Message_Storage::store_msg(sqlite3 *db, XBee_Message *msg) {
	uint16_t length;
	uint8_t *data;
	MessageType type;
	
	data = msg->get_payload(&length);
	type = static_cast<MessageType>(data[0]);
	
	switch (type) {
	case msgSensorData:
		store_sensor_msg(db, msg);
		break;
	case msgSensorConfig:
		store_config_msg(db, msg);
		break;
	case msgDebug:
		store_debug_msg(db, msg);
		break;
	}
}

/* checks the type of sensor messages and passes them on the the correct store function */
// TODO: Is this the right level to react if the "isReadRequest" flag is set?
void Message_Storage::store_sensor_msg(sqlite3 *db, XBee_Message *msg) {
	uint16_t length;
	uint8_t *data;
	SensorMessage *sensor_msg;
	DeviceType type;

	data = msg->get_payload(&length);
	sensor_msg = (SensorMessage*) data;
	type = sensor_msg->sensorType;
	
	switch (type) {
	case typeHeartRate:
		store_sensor_heart(db, msg);
		break;
	case typeRawTemperature:
		store_sensor_temperature(db, msg);
		break;
	case typeAccelerometer:
		store_sensor_accelerometer(db, msg);
		break;
	case typeGPS:
		store_sensor_gps(db, msg);
		break;
	default:;
	}
}

/* decodes messages containing configuration data */
void Message_Storage::store_config_msg(sqlite3 *db, XBee_Message *msg) {
	
}

/* decodes messages containing debug strings */
void Message_Storage::store_debug_msg(sqlite3 *db, XBee_Message *msg) {

}


void Message_Storage::store_sensor_heart(sqlite3 *db, XBee_Message *msg) {
	uint16_t length;
	uint8_t array_length;
	uint8_t *data = msg->get_payload(&length);
	SensorMessage *sensor_msg = (SensorMessage*) data;
	HeartRateMessage *msg_array = (HeartRateMessage*) sensor_msg->sensorMsgArray;
	array_length = sensor_msg->arrayLength;
	uint64_t addr64 = msg->get_address().get_addr64();
	const string common = "INSERT INTO sensorHeart values(";
	string command;
	for (uint8_t i = 0; i < array_length; i++) {
		stringstream command_data;
		command_data << addr64 <<", " << sensor_msg->endTimestampS << ", "
			<< (-i * sensor_msg->sampleIntervalMs) <<", " << msg_array[i].bpm
			<< ")";
		command = common + command_data.str();
		sqlite3_exec(db, command.c_str(), 0, 0, 0);
	} 
}

void Message_Storage::store_sensor_temperature(sqlite3 *db, XBee_Message *msg) {
	// TODO: Implement Covert function from RawTemp into Temp
}

void Message_Storage::store_sensor_accelerometer(sqlite3 *db, XBee_Message *msg) {
}

void Message_Storage::store_sensor_gps(sqlite3 *db, XBee_Message *msg) {
}

/* a signal handler for the ctrl+c interrupt, in order to end the program
 * gracefully (restoring terminal settings and closing fd) */
static void signal_handler_interrupt(int signum)
{
	fprintf(stderr, "Interrupt received: Closing DB connection & Terminating program\n");
	sqlite3_close(db);
	exit(1);
}
