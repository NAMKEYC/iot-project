from flask import Flask, render_template, request, jsonify, json
from flask_mqtt import Mqtt
from flask_sqlalchemy import SQLAlchemy
from datetime import date, datetime
from flask_socketio import SocketIO
from flask_bootstrap import Bootstrap
import eventlet
import json
import mysql.connector
eventlet.monkey_patch()

app = Flask(__name__, template_folder= 'templates')

app.config['TEMPLATES_AUTO_RELOAD'] = True
app.config['MQTT_BROKER_URL'] = 'broker.emqx.io'
app.config['MQTT_BROKER_PORT'] = 1883
app.config['MQTT_USERNAME'] = ''
app.config['MQTT_PASSWORD'] = ''
app.config['MQTT_KEEPALIVE'] = 5
app.config['MQTT_TLS_ENABLED'] = False
app.config['SQLALCHEMY_DATABASE_URI'] = 'mysql://nam:123456789@127.0.0.1/weather'
# Parameters for SSL enabled
# app.config['MQTT_BROKER_PORT'] = 8883
# app.config['MQTT_TLS_ENABLED'] = True
# app.config['MQTT_TLS_INSECURE'] = True
# app.config['MQTT_TLS_CA_CERTS'] = 'ca.crt'

mqtt_client = Mqtt(app)
db = SQLAlchemy(app)
app.app_context().push()

mydb = mysql.connector.connect(
  host="127.0.0.1",
  user="nam",
  password="123456789",
  database="weather"
)
mycursor = mydb.cursor()

# 123
def insert(value):
    value2 = ["",""]
    value2 = value[:-1].split(" ")
    # value2[1] = value2[1].replace("\u0000\u0000\u0000\u0000", "")
    s = ''.join(value2[0])
    s1 = ''.join(value2[1])
    s2 = ''.join(value2[2])
    print(s)
    today = date.today()
    print(today)
    now = datetime.now()
    current_time = now.strftime("%H:%M:%S")
    print(current_time)
    #sql = "INSERT INTO data (temp, date, hour) VALUES (%s, %s, %s)"
    # #sql = "INSERT INTO `Data`(`idSensor`, `date`, `time`) VALUES (%d, %s, %s)"
    sql = "INSERT INTO data (temp, humi, rain, date, hour) VALUES (%s, %s, %s, %s, %s)"
   # val = ("Michelle", "Blue Village")
    val = (s, s1, s2, today, current_time)
    print(val)
    mycursor.execute(sql, val)

    mydb.commit()


    # #val = ("1", "1 gio", "abc")
    # db.engine.execute(sql,val )
    # db.session.commit()

@app.route('/')
def index():
    return render_template('./index.html')

@app.route('/data', methods=['GET'])
def data():
    rows = db.engine.execute('SELECT id, temp, date, hour FROM data ORDER BY id DESC LIMIT 10')
    listdata = []
    for i in rows:
        dictdata = {
            "id" : i.id,
            "temp" : i.temp,
            "date" : i.date.__str__(),
            "hour" : i.hour.__str__()
        }
        listdata.append(dictdata)
    return json.dumps(listdata)

@app.route('/hud', methods=['GET'])
def data2():
    rows = db.engine.execute('SELECT id, humi, date, hour FROM data ORDER BY id DESC LIMIT 10')
    listdata = []
    for i in rows:
        dictdata = {
            "id" : i.id,
            "hud" : i.humi,
            "date" : i.date.__str__(),
            "hour" : i.hour.__str__()
        }
        listdata.append(dictdata)
    return json.dumps(listdata)

@app.route('/rain', methods=['GET'])
def data3():
    rows = db.engine.execute('SELECT id, rain, date, hour FROM data ORDER BY id DESC LIMIT 10')
    listdata = []
    for i in rows:
        dictdata = {
            "id" : i.id,
            "rain" : i.rain,
            "date" : i.date.__str__(),
            "hour" : i.hour.__str__()
        }
        listdata.append(dictdata)
    return json.dumps(listdata)

@mqtt_client.on_connect()
def handle_connect(client, userdata, flags, rc):
   if rc == 0:
       print('Connected successfully')
       mqtt_client.subscribe('crawldata') # subscribe topic
   else:
       print('Bad connection. Code:', rc)


@mqtt_client.on_message()
def handle_mqtt_message(client, userdata, message):
   data = dict(
       topic=message.topic,
       payload=message.payload.decode()
  )
   print('Received message on topic: {topic} with payload: {payload}'.format(**data))
   insert(data['payload'])

if __name__ == '__main__':
    mycursor = mydb.cursor()    
    db.engine.execute('SET SQL_SAFE_UPDATES = 0')
    db.engine.execute('DELETE FROM data;')
    db.engine.execute('ALTER TABLE data AUTO_INCREMENT = 1')
    app.run(host='127.0.0.1', port=5000)