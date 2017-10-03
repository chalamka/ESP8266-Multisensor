#include <ESP8266WiFi.h>

#include <WiFiUdp.h>

#include <ArduinoJson.h>

#include <PubSubClient.h>

#include <Adafruit_Sensor.h>

#include <DHT.h>




// Sensor Pins
#define DHTPIN D7
#define DHTTYPE DHT22
#define PIRPIN D5
#define LDRPIN A0

// Timestamp globals
const char* host = "time.nist.gov";
String TimeDate = "";
const int httpPort = 13;
int count = 0;

// Sensor globals
float diff_temp = 0.2;
float temp;

float diff_humidity = 1;
float humidity;

int pir_value;
String motion_status = "No Motion";

float ldr_value;
float diff_ldr = 25;

// JSON buffer
const int JSON_BUFFER_SIZE = 300;

// Set up Wifi, MQTT, DHT
WiFiClient wfc;
PubSubClient client(wfc);
DHT dht(DHTPIN, DHTTYPE);


void setup() {
  Serial.begin(115200);
  
  pinMode(DHTPIN, INPUT);
  pinMode(PIRPIN, INPUT);
  pinMode(LDRPIN, INPUT);
  
  setup_wifi();
  
  client.setServer(mqtt_broker_ip, mqtt_port);
  Serial.println("Setup complete");
}

void setup_wifi() {
  delay(10);
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.persistent(false);
  WiFi.begin(wifi_ssid, wifi_password);

  WiFi.mode(WIFI_STA);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

// the loop function runs over and over again forever
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  update_dht();
  update_pir();
  update_ldr();
  if(count == 10) {
    get_nist_time(); 
    count = 0;
  } else {
    count++;
  }
  delay(100);
}

bool check_motion_changed(float new_pir) {
  String motion;

  if(new_pir == HIGH) {
    motion = "Motion Detected";
  } else {
    motion = "No Motion";
  }

  return !motion.equals(motion_status);
}

void update_dht() {
  float new_temp = dht.readTemperature(true);
  float new_humidity = dht.readHumidity();

  if(check_sensor_bounds(new_temp, temp, diff_temp) || check_sensor_bounds(new_humidity, humidity, diff_humidity)) {
    temp = new_temp;
    humidity = new_humidity;

    create_sensor_json();
  }
}

void update_ldr() {
  float new_ldr = analogRead(LDRPIN);

  if(check_sensor_bounds(new_ldr, ldr_value, diff_ldr)) {
    ldr_value = new_ldr;
    create_sensor_json();
  }
}

void update_pir() {
  float new_pir = digitalRead(PIRPIN);
   
  if(check_motion_changed(new_pir)) {
    if(new_pir == HIGH) {
      motion_status = "Motion Detected";
    } else {
      motion_status = "No Motion";
    }
    create_sensor_json();
  }
}

bool check_sensor_bounds(float new_val, float old_val, float diff) {
  return new_val < old_val - diff || new_val > old_val + diff; 
}

JsonObject& create_sensor_json() {
  StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;

  JsonObject& sensor_json = jsonBuffer.createObject();
  sensor_json["Temperature"] = temp;
  sensor_json["Humidity"] = humidity;
  sensor_json["Motion"] = motion_status;
  sensor_json["Light"] = ldr_value;
  sensor_json["UTC Time"] = TimeDate;

  char json_buffer[sensor_json.measureLength() + 1];
  sensor_json.printTo(json_buffer, sizeof(json_buffer));
  
  Serial.println(json_buffer);
  client.publish(state_topic, json_buffer, true);

  return sensor_json;
}

void get_nist_time() {
  WiFiClient timeclient;
  if (timeclient.connect(host, httpPort)) {
    timeclient.print("HEAD / HTTP/1.1\r\nAccept: */*\r\nUser-Agent: Mozilla/4.0 (compatible; ESP8266 NodeMcu Lua;)\r\n\r\n");
    delay(100);
    char buffer[12];
    while(timeclient.available()) {
      String line = timeclient.readStringUntil('\r');

      if(!line.indexOf("Date") != -1) {
        TimeDate = line.substring(16,24);
      }
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(sensor_name, mqtt_user, mqtt_password)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(state_topic, "hello world");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
