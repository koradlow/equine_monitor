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
#include "messagetypes.h"
#include "sqlite_helper.h"
#include <gbee.h>
#include <gbee-util.h>
#include <array> 
#include <string>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>
#include <sstream>
#include <time.h>		// Only for testing
#include <math.h>

using std::string;
using std::stringstream;

static sqlite3 *db;

static void signal_handler_interrupt(int signum);

void test_settings(struct Settings *s) {
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

/* Function will de-serialize data into a MessagePacket structure.
 * data[in]: const Pointer to memory to de-serialize
 * msg[out]: Pointer to preallocated MessagePacket struct of sufficient size 
 * 
 * Hint: use size returned by XBee_Message->get_payload(&size) and add 
 * sizeof(MessagePacket) + sizeof(void*) to get the exact required size*/
void deserialize(const uint8_t *data, MessagePacket *msg) {
	/* MessageType is always the first byte of the serialized data */
	msg->mainType = (MessageType)data[0];
	msg->payload = (uint8_t *)&msg->payload + sizeof(msg->payload);
	
	/* Copy the message group specific fields into the MessagePacket */
	SensorMessage *sensor_msg = NULL; 
	ConfigMessage *config_msg = NULL;
	DebugMessage *debug_msg = NULL;

	switch (msg->mainType) {
	case msgSensorData:
		sensor_msg = (SensorMessage *)msg->payload;
		memcpy(msg->payload, &data[1], sizeof(SensorMessage));
		sensor_msg->sensorMsgArray = (uint8_t *)&sensor_msg->sensorMsgArray + sizeof(void *);
		break;
	case msgSensorConfig:
		config_msg = (ConfigMessage *)msg->payload;
		memcpy(msg->payload, &data[1], sizeof(ConfigMessage));
		config_msg->configMsgArray = (uint8_t *)&config_msg->configMsgArray + sizeof(void *);
		break;
	case msgDebug:
		debug_msg = (DebugMessage *)msg->payload;
		memcpy(msg->payload, &data[1], sizeof(DebugMessage));
		debug_msg->debugData = (uint8_t *)&debug_msg->debugData + sizeof(void *);
		break;
	}
	/* copy the message type specific payload */
	if (msg->mainType == msgSensorData) {
		/* calculate the address offset for the sensorMsgArray in the
		 * serialized data. The offset can be found by adding the size of
		 * the structs that come before the sensorMsgArray and deducting
		 * the size of the pointer members of the struct, because they 
		 * are not serialized. In this case there is one pointer member that
		 * needs to be deducted (*sensorMsgArray in SensorMessage) 
		 * !sizeof(MessagePacket) returns 8, but we do not serialize the pointer
		 * member and can pack the mainType into the first byte of the data,
		 * hence the "+1" instead of sizeof(MessagePacket) */
		int sensorMsgArrayOffset = 1 + sizeof(SensorMessage) - sizeof(void *);
		
		/* copy the sensorMsgArray into the serialized data structure */
		switch (sensor_msg->sensorType) {
		case typeHeartRate:
			memcpy(sensor_msg->sensorMsgArray, &data[sensorMsgArrayOffset], 
				sensor_msg->arrayLength*sizeof(HeartRateMessage));
			break;
		case typeRawTemperature:
			memcpy(sensor_msg->sensorMsgArray, &data[sensorMsgArrayOffset], 
				sensor_msg->arrayLength*sizeof(RawTemperatureMessage));
			break;
		case typeAccelerometer:
			memcpy(sensor_msg->sensorMsgArray, &data[sensorMsgArrayOffset], 
				sensor_msg->arrayLength*sizeof(AccelerometerMessage));
			break;
		case typeGPS:
			memcpy(sensor_msg->sensorMsgArray, &data[sensorMsgArrayOffset], 
				sensor_msg->arrayLength*sizeof(GPSMessage));
		break;
		default:
			printf("Cannot de-serialize messages with sensorType %u\n", sensor_msg->sensorType);
		}
	} else if (msg->mainType == msgSensorConfig) {
		/* calculate the address offset for the configMsgArray in the
		 * serialized data. See above for explanation */
		int configMsgArrayOffset = 1 + sizeof(ConfigMessage) - sizeof(void *);
		
		/* copy the configMsgArray into the serialized data structure */
		memcpy(config_msg->configMsgArray, &data[configMsgArrayOffset], 
				config_msg->arrayLength*sizeof(ConfigSensor));
	} else if (msg->mainType == msgDebug) {
		/* calculate the address offset for the debugData in the
		 * serialized data. See above for explanation */
		int debugDataOffset = 1 + sizeof(DebugMessage) - sizeof(void *);
		
		/* copy the debug string into the de-serialized data structure,
		 * for now the length of the copy operation is based the first
		 * occurrence of a terminating null byte */
		strcpy((char *)debug_msg->debugData, (const char *)&data[debugDataOffset]);
		}
}

/* Function will serialize data in the MessagePacket structure into continuous
 * memory area, that has to be preallocated and passed to the function 
 * msg[in]: Pointer to MessagePacket structure 
 * data[out]: Pointer to preallocated memory
 * returns: length of *data */ 
uint16_t serialize(const MessagePacket *msg, uint8_t *data) {
	uint16_t size = 0;
	
	/* copy the header information of the MessagePacket structure and
	 * set the destination of the payload pointer to the next element in the mem-space */
	data[size++] = (uint8_t)msg->mainType;

	/* copy the header information of the main message groups
	 * and set the sensorMsgArray pointer to the next element in the mem-space */
	switch (msg->mainType) {
	case msgSensorData:
		memcpy(&data[size], msg->payload, sizeof(SensorMessage) - sizeof(void *)); 
		size += sizeof(SensorMessage) - sizeof(void *);
		break;
	case msgSensorConfig:
		memcpy(&data[size], msg->payload, sizeof(ConfigMessage) - sizeof(void *)); 
		size += sizeof(ConfigMessage) - sizeof(void *);
		break;
	case msgDebug:
		memcpy(&data[size], msg->payload, sizeof(DebugMessage) - sizeof(void *));
		size += sizeof(DebugMessage) - sizeof(void *);
		break;
	}
	
	/* copy the message type specific payload */
	if (msg->mainType == msgSensorData) {
		const SensorMessage *sensor_msg = (const SensorMessage *)msg->payload;
		
		printf("SensorMsg: arrayLength %u\n", sensor_msg->arrayLength);
		/* copy the sensorMsgArray into the serialized data structure */
		switch (sensor_msg->sensorType) {
		case typeHeartRate:
			memcpy(&data[size], sensor_msg->sensorMsgArray, 
				sensor_msg->arrayLength*sizeof(HeartRateMessage));
			size += sensor_msg->arrayLength*sizeof(HeartRateMessage);
			break;
		case typeRawTemperature:
			memcpy(&data[size], sensor_msg->sensorMsgArray, 
				sensor_msg->arrayLength*sizeof(RawTemperatureMessage));
			size += sensor_msg->arrayLength*sizeof(RawTemperatureMessage);
			break;
		case typeAccelerometer:
			memcpy(&data[size], sensor_msg->sensorMsgArray, 
				sensor_msg->arrayLength*sizeof(AccelerometerMessage));
			size += sensor_msg->arrayLength*sizeof(AccelerometerMessage);
			break;
		case typeGPS:
			memcpy(&data[size], sensor_msg->sensorMsgArray, 
				sensor_msg->arrayLength*sizeof(GPSMessage));
			size += sensor_msg->arrayLength*sizeof(GPSMessage);
		break;
		default:
			printf("Cannot serialize messages with sensorType %u\n", sensor_msg->sensorType);
		}
	} else if (msg->mainType == msgSensorConfig) {
		const ConfigMessage *config_msg = (const ConfigMessage *)msg->payload;
		
		/* copy the configMsgArray into the serialized data structure */
		memcpy(&data[size], config_msg->configMsgArray, 
				config_msg->arrayLength*sizeof(ConfigSensor));
		size += config_msg->arrayLength*sizeof(ConfigSensor);
	} else if (msg->mainType == msgDebug) {
		DebugMessage *debug_msg = (DebugMessage *)msg->payload;
		
		/* copy the debug string into the de-serialized data structure,
		 * for now the length of the copy operation is based the first
		 * occurrence of a terminating null byte */
		strcpy((char *)&data[size], (const char *)debug_msg->debugData);
		size += strlen((const char *)debug_msg->debugData);
	}
	return size;
}

void test_serializing() {
	/* create Message Packet */
	const uint8_t arrayLength = 10;
	MessagePacket msg;
	SensorMessage sensor_msg;
	HeartRateMessage *heart_msg_array = new HeartRateMessage[arrayLength];
	/* set internal values of Message Packet */
	msg.mainType = msgSensorData;
	msg.payload = (uint8_t *)&sensor_msg;
	sensor_msg.sensorType = typeHeartRate;
	sensor_msg.endTimestampS = time(NULL);
	sensor_msg.sampleIntervalMs = 1234;
	sensor_msg.sensorMsgArray = (uint8_t *)heart_msg_array;
	sensor_msg.arrayLength = arrayLength;
	for (uint8_t i = 0; i < arrayLength; i++) {
		heart_msg_array[i].bpm = 80 + i;
	}
	int msg_size = sizeof(MessagePacket) + sizeof(SensorMessage) + 10 * sizeof(HeartRateMessage);
	printf("s(packet): %i, s(sensormsg): %i, s(heartmsg): %i \n", sizeof(MessagePacket), sizeof(SensorMessage), sizeof(HeartRateMessage));
	/* serialize the Message Packet */
	int ser_size;
	uint8_t *data = new uint8_t[msg_size];
	ser_size = serialize(&msg, data);
	printf("size: %i, serialized size: %i\n", msg_size, ser_size);

	/* deserialize the Message Packet */
	MessagePacket *des_msg = (MessagePacket *)new uint8_t[msg_size];
	deserialize(data, des_msg);
	
	/* check the values in the deserialized object */
	SensorMessage *des_sensor_msg = (SensorMessage *)des_msg->payload;
	printf("Type %u, interval %u, length %u\n", des_sensor_msg->sensorType, des_sensor_msg->sampleIntervalMs, des_sensor_msg->arrayLength);
	HeartRateMessage *des_msg_array = (HeartRateMessage *)des_sensor_msg->sensorMsgArray;
	for (int i = 0; i < des_sensor_msg->arrayLength; i++) {
		printf("heart rate %i: %u\n", 0, des_msg_array[i].bpm);
	}
}
void test_msg_storing(sqlite3 *db) {
	const uint8_t arrayLength = 10;
	uint16_t msg_size;
	Message_Storage storage;
	MessagePacket msg;
	SensorMessage sensor_msg;
	HeartRateMessage *heart_msg_array = new HeartRateMessage[arrayLength];
	
	msg_size = sizeof(MessagePacket) + sizeof(SensorMessage) + 10 * sizeof(HeartRateMessage);
	printf("msg_size: %u ", msg_size);
	XBee_Address addr;
	XBee_Message xbee_msg(addr, NULL, msg_size);
	printf(" : %u\n", msg_size);
	
	msg.mainType = msgSensorData;
	msg.payload = (uint8_t *)&sensor_msg;
	sensor_msg.sensorType = typeHeartRate;
	sensor_msg.endTimestampS = time(NULL);
	sensor_msg.sampleIntervalMs = 100;
	sensor_msg.arrayLength = arrayLength;
	sensor_msg.sensorMsgArray = (uint8_t *)heart_msg_array;
	
	for (uint8_t i = 0; i < arrayLength; i++) {
		heart_msg_array[i].bpm = 80 + i;
		printf("heart rate %i: %u\n", 0, heart_msg_array[i].bpm);
	}
	
	uint8_t *data = xbee_msg.get_payload(&msg_size);
	serialize(&msg, data);

	MessagePacket *deserialized_msg = (MessagePacket *)new uint8_t[msg_size];
	deserialize(data, deserialized_msg);
	
	XBee_Message xbee_msg_copy(xbee_msg);
	delete[] heart_msg_array;
	
	storage.store_msg(db, &xbee_msg_copy);
}

int main(int argc, char** argv){
	/* register signal handler for interrupt signal, to exit gracefully */
	signal(SIGINT, signal_handler_interrupt);
	
	/* try to load the settings from the config file */
	struct Settings settings;
	controller_parse_cl(argc, argv, &settings);
	
	test_settings(&settings);
	
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
	printf("Testing Storing\n");
	//test_msg_storing(db);
	test_serializing();
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
		printf("db file: %s\n", value);
		settings->database_path = string(value);
		if (access(value, F_OK) == -1)
			printf("DB file not found\n");
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
	printf("config file: %s\n", argv[1]);
	/* store the config file path */
	settings->config_file_path = string(argv[1]);

	/* parse the config file */
	ini_parse(argv[1], controller_ini_cb, settings);
}

/* creates the neccessary tables for storing sensor data, node addresses and
 * configuration options in the database */
void create_db_tables(sqlite3 *db) {
	/* define common SQL command substrings */
	string create = "CREATE TABLE IF NOT EXISTS ";
	string common_sensor_columns = "(addr64 UNSIGNED BIGINT,\
				timestamp UNSIGNED INT,\
				offset_ms UNSIGNED INT,";
	string common_debug_columns = "(addr64 UNSIGNED BIGINT, ";

	/* create SQL command strings by concatenating the SQL command substrings
	 * with the table name */
	string table_heart = create + TABLE_SENSOR_HEART + common_sensor_columns;
	string table_temperature = create + TABLE_SENSOR_TEMP + common_sensor_columns;
	string table_accel = create + TABLE_SENSOR_ACCEL + common_sensor_columns;
	string table_gps = create + TABLE_SENSOR_GPS + common_sensor_columns;
	string table_debug = create + TABLE_DEBUG_MESSAGES + common_debug_columns;

	/* append the custom fields of each table to the SQL commands */
	table_heart += "bmp INT)";
	table_temperature += "temp DOUBLE)";
	table_accel += 	"x INT, \
			y INT, \
			z INT)";
	table_gps += 	"lat_h INT, \
			lat_min INT, \
			lat_s INT, \
			lat_north BOOL, \
			long_h INT, \
			long_min INT, \
			long_s INT, \
			long_west BOOL, \
			valid_pos_fix BOOL)";
	table_debug +=	"message TEXT)";

	// TODO: Remove debug print statements
	printf("%s \n", table_heart.c_str());
	printf("%s \n", table_temperature.c_str());
	printf("%s \n", table_accel.c_str());
	printf("%s \n", table_gps.c_str());
	printf("%s \n", table_debug.c_str());
	
	/* try to create the tables */
	CALL_SQLITE(exec(db, table_heart.c_str(), 0, 0, 0));
	CALL_SQLITE(exec(db, table_temperature.c_str(), 0, 0, 0));
	CALL_SQLITE(exec(db, table_accel.c_str(), 0, 0, 0));
	CALL_SQLITE(exec(db, table_gps.c_str(), 0, 0, 0));
	CALL_SQLITE(exec(db, table_debug.c_str(), 0, 0, 0));
}

/* inserts one new row of data in the table identified by the table parameter.
 * db[in]: handle to an open sqlite3 db
 * table[in]: tablename of destination table for insert operation
 * value[in]: set of values that this table contains.
 * 		!expects the values to be delimited by brackets e.g "(val0, val1)" */
void insert_into_table(sqlite3 *db, const string &table, const string &values) {
	sqlite3_stmt *stmt;

	/* compile the SQL insert statement */
	string sql_insert = "INSERT INTO " + table +" VALUES" + values;
	CALL_SQLITE(prepare_v2(db, sql_insert.c_str(), -1, &stmt, NULL));
	/* execute the SQL query */
	CALL_SQLITE_EXPECT(step(stmt), DONE);
	/* destroy the SQL statement after execution */
	CALL_SQLITE(finalize(stmt));
}

/* decodes the type of the network message and passes it on the appropriate 
 * decoder function */
void Message_Storage::store_msg(sqlite3 *db, XBee_Message *msg) {
	uint16_t length;
	MessagePacket *message_packet;
	
	message_packet = (MessagePacket *)msg->get_payload(&length);

	switch (message_packet->mainType) {
	case msgSensorData:
		printf("storing sensor message\n");
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
void Message_Storage::store_sensor_msg(sqlite3 *db, XBee_Message *msg) {
	uint16_t length;
	MessagePacket *message_packet;
	SensorMessage *sensor_msg;
	DeviceType type;

	message_packet = (MessagePacket *)msg->get_payload(&length);
	sensor_msg = (SensorMessage*) message_packet->payload;
	type = sensor_msg->sensorType;
	uint64_t addr64 = msg->get_address().get_addr64();
	
	printf("Sensor Message: %u, %u , %u\n", sensor_msg->endTimestampS, sensor_msg->sampleIntervalMs, sensor_msg->arrayLength);
	switch (type) {
	case typeHeartRate:
		printf("storing hear rate message\n");
		store_sensor_heart(db, sensor_msg, addr64);
		break;
	case typeRawTemperature:
		store_sensor_raw_temperature(db, sensor_msg, addr64);
		break;
	case typeAccelerometer:
		store_sensor_accelerometer(db, sensor_msg, addr64);
		break;
	case typeGPS:
		store_sensor_gps(db, sensor_msg, addr64);
		break;
	default:;
	}
}

/* decodes messages containing configuration data */
void Message_Storage::store_config_msg(sqlite3 *db, XBee_Message *msg) {
	
}

/* decodes messages containing debug strings */
void Message_Storage::store_debug_msg(sqlite3 *db, XBee_Message *msg) {
	uint16_t length;
	MessagePacket *message_packet;
	DebugMessage *debug_msg;

	message_packet = (MessagePacket *)msg->get_payload(&length);
	debug_msg = (DebugMessage*) message_packet->payload;
	uint64_t addr64 = msg->get_address().get_addr64();

	string debug_string((char *)debug_msg->debugData);
	stringstream command_data;
	command_data << "("
		<< addr64 <<", "
		<< debug_string
		<< ")";
	insert_into_table(db, TABLE_DEBUG_MESSAGES, command_data.str());
	printf("%s \n", command_data.str().c_str());
}

void Message_Storage::store_sensor_heart(sqlite3 *db,
		SensorMessage *sensor_msg, uint64_t addr64) {
	HeartRateMessage *msg_array = (HeartRateMessage*) sensor_msg->sensorMsgArray;
	for (uint8_t i = 0; i < sensor_msg->arrayLength; i++) {
		stringstream command_data;
		command_data << "(" 
			<< addr64 <<", " 
			<< sensor_msg->endTimestampS << ", "
			<< (-i * sensor_msg->sampleIntervalMs) <<", " 
			<< (int)msg_array[i].bpm 
			<< ")";
		insert_into_table(db, TABLE_SENSOR_HEART, command_data.str());
		printf("%s \n", command_data.str().c_str());
	} 
}

void Message_Storage::store_sensor_raw_temperature(sqlite3 *db,
		SensorMessage *sensor_msg, uint64_t addr64) {
	RawTemperatureMessage *msg_array = (RawTemperatureMessage *)sensor_msg->sensorMsgArray;

	for (uint8_t i = 0; i < sensor_msg->arrayLength; i++) {
		double temp = calculate_temperature(msg_array[i].Vobj, msg_array[i].Tenv);
		stringstream command_data;
		command_data << "(" 
			<< addr64 <<", " 
			<< sensor_msg->endTimestampS << ", "
			<< (-i * sensor_msg->sampleIntervalMs) <<", "
			<< temp
			<< ")";
		insert_into_table(db, TABLE_SENSOR_TEMP, command_data.str());
		printf("%s \n", command_data.str().c_str());
	} 
}

void Message_Storage::store_sensor_accelerometer(sqlite3 *db,
		SensorMessage *sensor_msg, uint64_t addr64) {
	AccelerometerMessage *msg_array = (AccelerometerMessage *)sensor_msg->sensorMsgArray;
	
	for (uint8_t i = 0; i < sensor_msg->arrayLength; i++) {
		stringstream command_data;
		command_data << "(" 
			<< addr64 << ", " 
			<< sensor_msg->endTimestampS << ", "
			<< (-i * sensor_msg->sampleIntervalMs) << ", " 
			<< (int)msg_array[i].x << ", "
			<< (int)msg_array[i].y << ", "
			<< (int)msg_array[i].z
			<< ")";
		insert_into_table(db, TABLE_SENSOR_ACCEL, command_data.str());
		printf("%s \n", command_data.str().c_str());
	}
}

void Message_Storage::store_sensor_gps(sqlite3 *db, 
		SensorMessage *sensor_msg, uint64_t addr64) {
	GPSMessage *msg_array = (GPSMessage *)sensor_msg->sensorMsgArray;
	
	for (uint8_t i = 0; i < sensor_msg->arrayLength; i++) {
		stringstream command_data;
		command_data << "(" 
			<< addr64 << ", " 
			<< sensor_msg->endTimestampS << ", "
			<< (-i * sensor_msg->sampleIntervalMs) << ", " 
			<< (int)msg_array[i].latitude.degree << ", "
			<< (int)msg_array[i].latitude.minute << ", "
			<< (int)msg_array[i].latitude.minute << ", "
			<< (int)msg_array[i].latitudeNorth << ", "
			<< (int)msg_array[i].longitude.degree << ", "
			<< (int)msg_array[i].longitude.minute << ", "
			<< (int)msg_array[i].longitude.minute << ", "
			<< (int)msg_array[i].longitudeWest << ", "
			<< (int)msg_array[i].validPosFix << ", "
			<< ")";
		insert_into_table(db, TABLE_SENSOR_GPS, command_data.str());
		printf("%s \n", command_data.str().c_str());
	}
}

double calculate_temperature(double v_obj, double t_env)
{
	// Calculate Tobj, based on data/formula from TI
	double S0 = 0.00000000000006;       /* Default S0 cal value,  6 * pow(10, -14) */
	double a1 = 0.00175; /* 1.75 * pow(10, -3) */
	double a2 = -0.00001678; /* -1.678 * pow(10, -5) */
	double b0 = -0.0000294; /* -2.94 * pow(10, -5) */
	double b1 = -0.00000057;  /* -5.7*pow(10, -7); */
	double b2 = 0.00000000463 /* 4.63*pow(10, -9) */;
	double c2 = 13.4;
	double Tref = 298.15;
	double S = S0*(1+a1*(t_env - Tref)+a2*pow((t_env - Tref),2));
	double Vos = b0 + b1*(t_env - Tref) + b2*pow((t_env - Tref),2);
	double fObj = (v_obj - Vos) + c2*pow((v_obj - Vos),2);
	double Tobj = pow(pow(t_env,4) + (fObj/S), (double).25) - 273.15;

	return Tobj;
}
/* a signal handler for the ctrl+c interrupt, in order to end the program
 * gracefully (restoring terminal settings and closing fd) */
static void signal_handler_interrupt(int signum)
{
	fprintf(stderr, "Interrupt received: Closing DB connection & Terminating program\n");
	sqlite3_close(db);
	exit(1);
}
