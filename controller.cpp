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
#include "messagestorage.h"
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

int main(int argc, char** argv){
	/* register signal handler for interrupt signal, to exit gracefully */
	signal(SIGINT, signal_handler_interrupt);
	
	/* try to load the settings from the config file */
	Settings settings;
	controller_parse_cl(argc, argv, &settings);

	/* setup XBee interface with settings from config file */
	XBee_Config config(settings.tty_port, settings.identifier, settings.controller_mode,
			settings.pan_id, settings.timeout, settings.baud_rate, settings.max_unicast_hops);
	XBee interface(config);
	if (interface.xbee_init() != GBEE_NO_ERROR) {
		printf("Error: unable to configure XBee device");
		return -1;
	}
	while (interface.xbee_status());
	printf("Successfully formed or joined ZigBee Network\n");

	/* connect to the database, and set it up */
	int error_code;
	sqlite3 *db;
	error_code = sqlite3_open(settings.database_path.c_str(), &db);
	if (error_code) {
		printf("Error: cannot open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}
	create_db_tables(db);
	
	/* system initialization complete - start main control loop */
	Message_Storage database;
	printf("Waiting for messages\n");
	while (1) {
		XBee_Message *msg = NULL;
		/* try to decode a message if there's data in the receive buffer */
		if (interface.xbee_bytes_available()) {
			msg = interface.xbee_receive_message();
			/* if a message was decoded, store it in the database */
			if (msg->is_complete()) {
				database.store_msg(db, msg);
			}
			delete msg;
		}
		
		// TODO: figure out if the configuration of one of the nodes
		// was changed, and update it accordingly
		usleep(500);
	}

	sqlite3_close(db);
	return 0;
}

void controller_usage_hint() {
	fprintf(stderr, "please give the path of the config file as the first argument\n");
}

int controller_ini_cb(void* buffer, const char* section, const char* name, const char* value) {
	#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
	Settings *settings = (Settings*) buffer;

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

void controller_parse_pan(Settings *settings, const char *value)
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

void controller_parse_cl(int argc,char **argv, Settings *settings) {
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
	string common_sensor_columns = "(addr64 UNSIGNED BIGINT, "
				"timestamp UNSIGNED INT, "
				"offset_ms UNSIGNED INT,";
	string common_debug_columns = "(addr64 UNSIGNED BIGINT, "
				"timestamp UNSIGNED INT, ";
	string common_node_columns = "(addr64 UNSIGNED BIGINT UNIQUE, "
				"addr16 UNSIGNED INT, "
				"identifier VARCHAR(20))";

	/* create SQL command strings by concatenating the SQL command substrings
	 * with the table name */
	string table_heart = create + TABLE_SENSOR_HEART + common_sensor_columns;
	string table_temperature = create + TABLE_SENSOR_TEMP + common_sensor_columns;
	string table_accel = create + TABLE_SENSOR_ACCEL + common_sensor_columns;
	string table_gps = create + TABLE_SENSOR_GPS + common_sensor_columns;
	string table_gps_alt = create + TABLE_SENSOR_GPS_ALT + common_sensor_columns;
	string table_debug = create + TABLE_DEBUG_MESSAGES + common_debug_columns;
	string table_nodes = create + TABLE_MONITORING_NODES + common_node_columns;
	
	/* append the custom fields of each table to the SQL commands */
	table_heart += "bmp INT)";
	table_temperature += "temp DOUBLE)";
	table_accel += 	"x INT, "
			"y INT, "
			"z INT)";
	table_gps += 	"lat_h INT,"
			"lat_min INT, "
			"lat_s INT, "
			"lat_north BOOL, "
			"long_h INT, "
			"long_min INT, "
			"long_s INT, "
			"long_west BOOL, "
			"valid_pos_fix BOOL)";
	table_gps_alt +="latitude REAL, "
			"longitude REAL)";
	table_debug +=	"message TEXT)";

	// TODO: Remove debug print statements
	printf("%s \n", table_heart.c_str());
	printf("%s \n", table_temperature.c_str());
	printf("%s \n", table_accel.c_str());
	printf("%s \n", table_gps.c_str());
	printf("%s \n", table_gps_alt.c_str());
	printf("%s \n", table_debug.c_str());
	printf("%s \n", table_nodes.c_str());
	
	/* try to create the tables */
	CALL_SQLITE(exec(db, table_heart.c_str(), 0, 0, 0));
	CALL_SQLITE(exec(db, table_temperature.c_str(), 0, 0, 0));
	CALL_SQLITE(exec(db, table_accel.c_str(), 0, 0, 0));
	CALL_SQLITE(exec(db, table_gps.c_str(), 0, 0, 0));
	CALL_SQLITE(exec(db, table_gps_alt.c_str(), 0, 0, 0));
	CALL_SQLITE(exec(db, table_debug.c_str(), 0, 0, 0));
	CALL_SQLITE(exec(db, table_nodes.c_str(), 0, 0, 0));
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
	uint8_t *data;
	MessagePacket *message_packet;

	/* de-serialze the message */
	data = msg->get_payload(&length);
	message_packet = (MessagePacket *) new uint8_t[length+10];
	MessageStorage::deserialize(data, message_packet);
	
	/* store messages into the appropriate db tables */
	switch (message_packet->mainType) {
	case msgSensorData:
		printf("storing sensor message\n");
		store_sensor_msg(db, msg);
		break;
	case msgSensorConfig:
		printf("storing config message\n");
		store_config_msg(db, msg);
		break;
	case msgDebug:
		printf("storing debug message\n");
		store_debug_msg(db, msg);
		break;
	default: 
		printf("message with unknown mainType: %u\n", message_packet->mainType);
	}
	/* try to store the source address in the database */
	store_address(db, msg->get_address());
	
	delete[] message_packet;
}

/* checks the type of sensor messages and passes them on the the correct store function */
void Message_Storage::store_sensor_msg(sqlite3 *db, XBee_Message *msg) {
	uint16_t length;
	uint8_t *data;
	MessagePacket *message_packet;
	SensorMessage *sensor_msg;
	DeviceType type;
	uint64_t addr64;

	/* de-serialze the message */
	data = msg->get_payload(&length);
	message_packet = (MessagePacket *) new uint8_t[length+10];
	MessageStorage::deserialize(data, message_packet);
	sensor_msg = (SensorMessage*) message_packet->payload;
	type = sensor_msg->sensorType;
	addr64 = msg->get_address().get_addr64();

	/* the monitoring devices work with a relative timestamp, because
	 * it cannot be guaranteed that they are synced with a proper RTC.
	 * Calculate the absolute timestamp of the endTimestampS value and
	 * update the SensorMessage object */
	uint32_t absEndTimestampS = time(NULL) - (message_packet->relTimestampS - sensor_msg->endTimestampS);
	sensor_msg->endTimestampS = absEndTimestampS;
	
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
		store_sensor_gps_alt(db, sensor_msg, addr64);
		break;
	default:;
	}

	delete [] message_packet;
}

/* decodes messages containing configuration data */
void Message_Storage::store_config_msg(sqlite3 *db, XBee_Message *msg) {
	
}

/* decodes messages containing debug strings */
void Message_Storage::store_debug_msg(sqlite3 *db, XBee_Message *msg) {
	uint16_t length;
	uint8_t *data;
	MessagePacket *message_packet;
	DebugMessage *debug_msg;
	uint64_t addr64;

	/* de-serialze the message */
	data = msg->get_payload(&length);
	message_packet = (MessagePacket *) new uint8_t[length+10];
	MessageStorage::deserialize(data, message_packet);
	debug_msg = (DebugMessage*) message_packet->payload;
	addr64 = msg->get_address().get_addr64();
	
	/* the monitoring devices transmit a relative timestamp.
	 * Calculate the absolute timestamp of the debug message before
	 * storing it the db */
	uint32_t timestampS = time(NULL) - (message_packet->relTimestampS - debug_msg->timestampS);
	
	string debug_string((char *)debug_msg->debugData);
	stringstream command_data;
	command_data << "("
		<< addr64 <<", "
		<< timestampS << ", "
		<< debug_string
		<< ")";
	insert_into_table(db, TABLE_DEBUG_MESSAGES, command_data.str());
	printf("%s \n", command_data.str().c_str());
	
	delete [] message_packet;
}

/* tries to store the source address of the message in the db */
void Message_Storage::store_address(sqlite3 *db, const XBee_Address &addr) {
	stringstream command_data;
	command_data << "("
		<< addr.get_addr64() << ", "
		<< addr.addr16 << ", "
		<< "Undefined" 
		<< ")";
	insert_into_table(db, TABLE_MONITORING_NODES, command_data.str());
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
			<< (int)msg_array[i].latitude.second << ", "
			<< (int)msg_array[i].latitudeNorth << ", "
			<< (int)msg_array[i].longitude.degree << ", "
			<< (int)msg_array[i].longitude.minute << ", "
			<< (int)msg_array[i].longitude.second << ", "
			<< (int)msg_array[i].longitudeWest << ", "
			<< (int)msg_array[i].validPosFix
			<< ")";
		insert_into_table(db, TABLE_SENSOR_GPS, command_data.str());
		printf("%s \n", command_data.str().c_str());
	}
}

/* calculates the latitude and longitude values before storing the data into the db */
void Message_Storage::store_sensor_gps_alt(sqlite3 *db, 
		SensorMessage *sensor_msg, uint64_t addr64) {
	GPSMessage *msg_array = (GPSMessage *)sensor_msg->sensorMsgArray;
	
	for (uint8_t i = 0; i < sensor_msg->arrayLength; i++) {
		stringstream command_data;
		GPSPosition position = calculate_gps_position(&msg_array[i]);
		command_data << "(" 
			<< addr64 << ", " 
			<< sensor_msg->endTimestampS << ", "
			<< (-i * sensor_msg->sampleIntervalMs) << ", " 
			<< position.latitude << ", "
			<< position.longitude
			<< ")";
		insert_into_table(db, TABLE_SENSOR_GPS_ALT, command_data.str());
		printf("%s \n", command_data.str().c_str());
	}
}

GPSPosition calculate_gps_position(const GPSMessage* gps) {
	GPSPosition position;
	/* calculate latitude */
	position.latitude = gps->latitude.degree +  gps->latitude.minute / 60.0 +
			 gps->latitude.second / 3600.0;
	position.latitude = (gps->latitudeNorth)? position.latitude : -(position.latitude);

	/* calculte longitude */
	position.longitude = gps->longitude.degree +  gps->longitude.minute / 60.0 +
			 gps->longitude.second / 3600.0;
	position.longitude = (gps->longitudeWest)? -position.longitude : (position.longitude);

	return position;
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
