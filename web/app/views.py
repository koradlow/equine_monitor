import sqlite3
import pprint
from flask import render_template
from flask import url_for
from flask import g
from app import app

DATABASE = "test.db"

@app.route("/index")
@app.route("/")
def index():
	return render_template("index.html", title = None, menu = get_main_menu())

@app.route('/config/', defaults={'horse_id': ''})
@app.route("/config/<horse_id>")
def display_config(horse_id):
	return render_template("config.html", title = horse_id, menu = get_main_menu())

@app.route('/data/', defaults={'horse_id': ''})
@app.route("/data/<horse_id>")
def display_data(horse_id):
	return render_template("data.html", title = horse_id, menu = get_main_menu() , 
	horse_id = horse_id, tables = get_tables(horse_id))

@app.route('/status')
def display_status():
	return render_template("status.html", menu = get_main_menu())

# loads all items in the monitoringNodes table and puts them into a list
# with the indexes 'href' and 'caption'
def get_node_list(basepath):
	node_list = []
	test = query_db('SELECT * FROM monitoringNodes')
	for node in query_db('SELECT * FROM monitoringNodes'):
		link_destination = basepath + node['identifier']
		node_list.append({ 'href' : link_destination, 'caption' : node['identifier']})
	return node_list

def get_main_menu():
	menu = [ {'href' : '/data', 'caption' : 'Data', 'submenu' : get_node_list('/data/')},
		{'href' : '/config', 'caption' : 'Config', 'submenu' : get_node_list('/config/')},
		{'href' : '/status', 'caption' : 'Status' }
		]
	return menu

def get_tables(horse_id=None):
	table = []
	if(horse_id):
		address = get_addr64(horse_id)
		print address
		heart_table = query_db('SELECT * FROM sensorHeart WHERE addr64='+address)
		if (heart_table):
			table.append(['Heart Rate Sensor', heart_table[0].keys(), heart_table])
	return table

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
