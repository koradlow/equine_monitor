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

 #ifndef CONTROLLER_H
 #define CONTROLLER_H

#include "xbee_if.h"
#include <string>
#include <sqlite3.h>

/*** struct containing all available settings for the controller ***/
struct Settings {
	/* Controller Configuration */
	std::string database_path;
	std::string config_file_path;

	/* ZigBee Configuration */
	std::string identifier;
	std::string tty_port;
	uint8_t pan_id[8];
	bool controller_mode;
	uint32_t timeout;
	xbee_baud_rate baud_rate;
	uint8_t max_unicast_hops;
};

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
	void store_sensor_heart(sqlite3 *db, XBee_Message *msg);
	void store_sensor_temperature(sqlite3 *db, XBee_Message *msg);
	void store_sensor_accelerometer(sqlite3 *db, XBee_Message *msg);
	void store_sensor_gps(sqlite3 *db, XBee_Message *msg);
};
/* this function verifies that all tables that we need to store the received
 * data and configuration options exist */
void create_db_tables(sqlite3 *db);

/* this function will request the node identifier from the device identified
 * by the address and update the node table accordingly to the response */
void update_node_table(sqlite3 *db, XBee_Address *addr);

/*** Group of helper functions ***/
/* parse the config file to set up the program settings */
void controller_parse_cl(int argc,char **argv, struct Settings *settings);

/* parse the PAN ID field of the config file */
void controller_parse_pan( struct Settings *settings, const char *value);

/* print an explanation of how to use the program to the command line */
void controller_usage_hint();

/* callback function for the ini file parsing library */
int controller_ini_cb(void* buffer, const char* section, const char* name, const char* value);

 #endif 
