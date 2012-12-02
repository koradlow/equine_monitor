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
 * Code Based on www.lemoda.net/s/sqlite-insert
 */
 
#ifndef SQLITE_HELPER_H
#define SQLITE_HELPER_H

#define TABLE_SENSOR_HEART "sensorHeart"
#define TABLE_SENSOR_TEMP "sensorTemperature"
#define TABLE_SENSOR_ACCEL "sensorAccelerometer"
#define TABLE_SENSOR_GPS "sensorGPS"
#define TABLE_DEBUG_MESSAGES "debugMessages"

#define CALL_SQLITE(FUNC) 						\
{									\
	int i; 								\
	i = sqlite3_ ## FUNC;						\
	if (i != SQLITE_OK) {						\
		fprintf (stderr, "%s failed with status %d: %s\n",	\
		#FUNC, i, sqlite3_errmsg (db));				\
	}								\
}				\

#define CALL_SQLITE_EXPECT(FUNC,EXPECT)					\
{									\
	int i;								\
	i = sqlite3_ ## FUNC;						\
	if (i != SQLITE_ ## EXPECT) {					\
		fprintf (stderr, "%s failed with status %d: %s\n",	\
		#FUNC, i, sqlite3_errmsg (db));				\
	}								\
}									\

/*** Group of functions to deserialize messages and store the data***/
/* a class without internal state that hides away the helper functions
 * used do de-serialize messages */
class Message_Storage {
public:
	void store_msg(sqlite3 *db, XBee_Message *msg);
private:
	/* intermediate functions for passing data on to the store functions */
	void store_sensor_msg(sqlite3 *db, XBee_Message *msg);
	void store_debug_msg(sqlite3 *db, XBee_Message *msg);
	void store_config_msg(sqlite3 *db, XBee_Message *msg);
	
	/* functions to store the sensor messages */
	void store_sensor_heart(sqlite3 *db, SensorMessage *sensor_msg, uint64_t addr64);
	void store_sensor_raw_temperature(sqlite3 *db, SensorMessage *sensor_msg, uint64_t addr64);
	void store_sensor_accelerometer(sqlite3 *db, SensorMessage *sensor_msg, uint64_t addr64);
	void store_sensor_gps(sqlite3 *db, SensorMessage *sensor_msg, uint64_t addr64);
};

/* function will try to insert a new row of data into the table */ 
void insert_into_table(sqlite3 *db, const string &table, const string &values);

/* this function verifies that all tables that we need to store the received
 * data and configuration options exist */
void create_db_tables(sqlite3 *db);

/* this function will request the node identifier from the device identified
 * by the address and update the node table accordingly to the response */
void update_node_table(sqlite3 *db, XBee_Address *addr);
#endif
