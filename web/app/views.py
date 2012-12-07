import sqlite3
import time
import pprint
from flask import render_template
from flask import url_for
from flask import request
from flask import g
from app import app

# Constant defines for the program
DATABASE = "equine.db"
TABLENAMES = { 'heart': 'sensorHeart',
		'temp' : 'sensorTemperature',
		'accel' : 'sensorAccelerometer',
		'gps' : 'sensorGPS',
		'debug' : 'debugMessages',
		'nodes' : 'monitoringNodes' }
TABLE_LENGTH = '50'

# Display URL functions
@app.route("/index")
@app.route("/")
def index():
	return render_template("index.html", title = None, menu = get_main_menu())

@app.route('/config/', defaults={'horse_id': ''})
@app.route("/config/<horse_id>")
def display_config(horse_id):
	print 'rendering config'
	return render_template("config.html", title = horse_id, menu = get_main_menu())

@app.route('/data/', defaults={'horse_id': ''})
@app.route("/data/<horse_id>")
def display_data(horse_id):
	print 'rendering data'
	return render_template("data.html", title = horse_id, menu = get_main_menu() , 
		sensor_menu = get_sensor_menu(horse_id), horse_id = horse_id, tables = {})

@app.route("/data/<horse_id>/<sensor_id>")
def display_sensor_data(horse_id, sensor_id):
	print 'rendering sensor data'
	table = TABLENAMES[sensor_id] if TABLENAMES.has_key(sensor_id) else None
	return render_template("data.html", title = horse_id, menu = get_main_menu(),
		sensor_menu = get_sensor_menu(horse_id), horse_id = horse_id,
		tables = get_table(table, horse_id))

@app.route('/status')
def display_status():
	print 'rendering status'
	return render_template("status.html", menu = get_main_menu())

@app.route('/debug/', defaults={'horse_id': ''})
@app.route("/debug/<horse_id>")
def display_debug(horse_id):
	print 'rendering debug data'
	table = TABLENAMES['debug']
	return render_template("debug.html", title = horse_id, menu = get_main_menu(),
		horse_id = horse_id, tables = get_table(table, horse_id))

# Helper functions

# loads all items in the monitoringNodes table and puts them into a list
# of dictionaries with the indexes 'href' and 'caption'
def get_node_list(basepath):
	node_list = []
	test = query_db('SELECT * FROM monitoringNodes')
	for node in query_db('SELECT * FROM monitoringNodes'):
		link_destination = basepath + node['identifier']
		node_list.append({ 'href' : link_destination, 'caption' : node['identifier']})
	return node_list

# returns the items of the main menu in a list of dictionaries with the
# keys 'href', 'caption' and 'submenu' (list of dictionaries with same keys)
def get_main_menu():
	menu = [ {'href' : '/data', 'caption' : 'Data', 'submenu' : get_node_list('/data/')},
		{'href' : '/config', 'caption' : 'Config', 'submenu' : get_node_list('/config/')},
		{'href' : '/status', 'caption' : 'Status' },
		{'href' : '/debug', 'caption' : 'Debug', 'submenu' : get_node_list('/debug/')}
		]
	return menu

# returns the items of the sensor menu in a list of dictionaries with the
# keys 'href' and 'caption'
def get_sensor_menu(horse_id):
	sensor_menu = 	 [ {'href' : '/data/'+horse_id+'/heart', 'caption' : 'Heart'},
		{'href' : '/data/'+horse_id+'/temp', 'caption' : 'Temperature'},
		{'href' : '/data/'+horse_id+'/accel', 'caption' : 'Accelerometer'},
		{'href' : '/data/'+horse_id+'/gps', 'caption' : 'GPS'}
		]
	return sensor_menu

# returns the items of the table where the addr64(horse_id) == table.row.addr64 
def get_table(tablename, horse_id=None):
	table = []
	if(horse_id and tablename):
		address = get_addr64(horse_id)
		heart_table = query_db('SELECT * FROM ' + tablename + ' WHERE addr64=' + address + ' LIMIT ' + TABLE_LENGTH)
		if (heart_table):
			table.append([tablename, heart_table[0].keys(), replace_timestamp(heart_table)])
	elif(tablename):
		heart_table = query_db('SELECT * FROM ' + tablename + ' LIMIT ' + TABLE_LENGTH)
		if (heart_table):
			table.append([tablename, heart_table[0].keys(), replace_timestamp(heart_table)])

	return table

def replace_timestamp(table):
	for row in table:
		row['timestamp'] = time.ctime(int(row['timestamp']))
	return table

# TODO: Implement setting of Node identifier for horse from web interface
def get_addr64(horse_id):
	addr64 = 101010
	nodes = query_db('SELECT * from monitoringNodes')
	for node in nodes:
		if node['identifier'] == horse_id:
			addr64 = node['addr64']
	return str(addr64)

# database related functions
def connect_db():
	return sqlite3.connect(DATABASE)

@app.before_request
def before_request():
	g.db = connect_db()

@app.teardown_request
def teardown_request(exception):
	if hasattr(g, 'db'):
		g.db.close()

def query_db(query, args=(), one=False):
	cur = g.db.execute(query, args)
	rv = [dict((cur.description[idx][0], value) for idx, value in enumerate(row)) for row in cur.fetchall()]
	return (rv[0] if rv else None) if one else rv
