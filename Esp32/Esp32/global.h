
const char *thingspeak = "api.thingspeak.com";
#include <ESPmDNS.h>

#include <WiFiClient.h>

#include "Pump.h"
#include "TempSensors.h"

WiFiClient client; // needed for sending data to thingspeak

String apiKey;
String thingspeakChannelID;
const char* loginUsername = "admin";
String loginPassword = "";

// Pin settings
#define boilerSensorPin 32
#define collectorSensorPin 33
#define t1SensorPin 25
#define t2SensorPin 26
//rgb pins
int redPin = 27;
int greenPin = 14;
int bluePin = 12;
// use first 3 channels of 16 channels (started from zero)
#define LEDC_CHANNEL_0_R  0
#define LEDC_CHANNEL_1_G  1
#define LEDC_CHANNEL_2_B  2
// use 13 bit precission for LEDC timer
#define LEDC_TIMER_13_BIT  13

// use 5000 Hz as a LEDC base frequency
#define LEDC_BASE_FREQ     5000

/*Struct for saving arduino settings*/
struct Settings {
  int tdiffmin = 0;
  int tdiffmininput = 0;
  int tkmax = 0;
  int tkmaxinput = 0;
  int tkmin = 0;
  int tkmininput = 0;
  int tbmax = 0;
  int tbmaxinput = 0;
  int altitude = 0;
  int altitudeinput = 0;
  String IP = "";
}   SettingsValues;

struct statistics {
  double roomMinTemp;
  double roomMaxTemp;
  double roomMaxHumidity;
  double roomMinHumidity;
  double roomMinPressure;
  double roomMaxPressure;
}   statisticsValues;

TempSensor boilerSensor(boilerSensorPin);
TempSensor collectorSensor(collectorSensorPin);
TempSensor t1Sensor(t1SensorPin);
TempSensor t2Sensor(t2SensorPin);

Pump pumps[4] = { Pump("1", 3, true), Pump("2", 4, true), Pump("3", 5, true), Pump("4", 6, true) };

DateTime now;

unsigned long lastUpdate = 0;
unsigned long updateInterval = 60L * 1000L;//1 min
unsigned long lastSensorsUpdate = 0;
unsigned long lastThingspeak = 0;
unsigned long thingspeakInterval = 5L * 60L * 1000L;//5 mins

float roomTemp, roomHumidity = 0;
float roomPressure = 1111;
bool autoMode = true;

int problemID = 0; // 0-No problem, 1-BME280 Problem


void wifiConnect() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  delay(500);

  WiFi.setHostname("SSHProject");
  if (!MDNS.begin("SSHProject")) {
    while (1) {
      delay(1000);
    }
  }
  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", 80);
  SettingsValues.IP = WiFi.localIP().toString();
}

void SPIFFSInitReadData() {
  SPIFFS.begin();
  File f = SPIFFS.open("/data.txt", "r");
  loginPassword = f.readStringUntil('|');
  if (loginPassword == "")
    loginPassword = "admin";
  thingspeakChannelID = f.readStringUntil('|');
  apiKey = f.readStringUntil('|');
  SettingsValues.tdiffmin = f.readStringUntil('|').toInt();
  SettingsValues.tkmax = f.readStringUntil('|').toInt();
  SettingsValues.tkmin = f.readStringUntil('|').toInt();
  SettingsValues.tbmax = f.readStringUntil('|').toInt();
  SettingsValues.altitude = f.readStringUntil('|').toInt();
  f.close();
}

void TempHandler() {
  if (autoMode) {
    if (boilerSensor.tempDouble() < SettingsValues.tbmax && ((collectorSensor.tempDouble() - boilerSensor.tempDouble() >= SettingsValues.tdiffmin && collectorSensor.tempDouble() >= SettingsValues.tkmin) || collectorSensor.tempDouble() > SettingsValues.tkmax) )
    {
      if (!pumps[0].isOperating()) pumps[0].on();
    }

    else if (pumps[0].isOperating()) pumps[0].off();
  }
  else {
    if (collectorSensor.tempDouble() > SettingsValues.tkmax && boilerSensor.tempDouble() < SettingsValues.tbmax) {
      if (!pumps[0].isOperating()) pumps[0].on();
    }

    else if (pumps[0].isOperating()) pumps[0].off();
  }
}

void statisticshandler() {
  if (roomPressure > statisticsValues.roomMaxPressure) statisticsValues.roomMaxPressure = roomPressure;
  if (roomPressure < statisticsValues.roomMinPressure) statisticsValues.roomMinPressure = roomPressure;
  if (roomHumidity > statisticsValues.roomMaxHumidity) statisticsValues.roomMaxHumidity = roomHumidity;
  if (roomHumidity < statisticsValues.roomMinHumidity) statisticsValues.roomMinHumidity = roomHumidity;
  if (roomTemp > statisticsValues.roomMaxTemp) statisticsValues.roomMaxTemp = roomTemp;
  if (roomTemp < statisticsValues.roomMinTemp) statisticsValues.roomMinTemp = roomTemp;
}

void resetStatistics() {
  collectorSensor.resetStatistics();
  boilerSensor.resetStatistics();
  t1Sensor.resetStatistics();
  t2Sensor.resetStatistics();
  statisticsValues.roomMinPressure = roomPressure;
  statisticsValues.roomMaxPressure = roomPressure;
  statisticsValues.roomMinHumidity = roomHumidity;
  statisticsValues.roomMaxHumidity = roomHumidity;
  statisticsValues.roomMinTemp = roomTemp;
  statisticsValues.roomMaxTemp = roomTemp;
}

void TempUpdate() {
  boilerSensor.updateTemp();
  collectorSensor.updateTemp();
  t1Sensor.updateTemp();
  t2Sensor.updateTemp();
}

double bmeAtSealevel(double pressure)
{
  return pressure / pow((1.0 - (float)SettingsValues.altitude / 44330.0), 5.255);
}

void BMEUpdate() {
  BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
  BME280::PresUnit presUnit(BME280::PresUnit_Pa);

  //bme.read(roomPressure, roomTemp, roomHumidity, tempUnit, presUnit);
  roomPressure = bme.pres();
  roomTemp = bme.temp();
  roomHumidity = bme.hum();
  Serial.println(roomPressure);
  roomPressure = bmeAtSealevel(roomPressure / 100);
  Serial.println(roomHumidity);
}

void sensorUpdate() {
  now = rtc.now();
  TempUpdate();
  BMEUpdate();
  lastSensorsUpdate = millis();
  statisticshandler();
}

void ledSetup() {
  ledcSetup(LEDC_CHANNEL_0_R, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
  ledcAttachPin(redPin, LEDC_CHANNEL_0_R);
  ledcSetup(LEDC_CHANNEL_1_G, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
  ledcAttachPin(greenPin, LEDC_CHANNEL_1_G);
  ledcSetup(LEDC_CHANNEL_2_B, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
  ledcAttachPin(bluePin, LEDC_CHANNEL_2_B);
}

void ledcAnalogWrite(uint8_t channel, uint32_t value, uint32_t valueMax = 255) {
  // calculate duty
  uint32_t duty = (LEDC_BASE_FREQ / valueMax) * value;
  // write duty to LEDC
  ledcWrite(channel, duty);
}
void setColor(int red = 0, int green = 0, int blue = 0)
{
  ledcAnalogWrite(LEDC_CHANNEL_0_R, map(red, 0, 255, 255, 0));
  ledcAnalogWrite(LEDC_CHANNEL_1_G, map(green, 0, 255, 255, 0));
  ledcAnalogWrite(LEDC_CHANNEL_2_B, map(blue, 0, 255, 255, 0));
  //analogWrite(redPin, map(red, 0, 255, 255, 0));
  //analogWrite(greenPin, map(green, 0, 255, 255, 0));
  //analogWrite(bluePin, map(blue, 0, 255, 255, 0));
}

void ledHandler() {
  switch (problemID) {
    case 0:
      setColor(0, 255); //green
      break;
    case 1:
      setColor(255);
      break;
  }
}

void sendToThingspeak() {
  if (client.connect(thingspeak, 80)) {  //   "184.106.153.149" or api.thingspeak.com
    String postStr = apiKey;
    postStr += "&field1=";
    postStr += String(boilerSensor.tempDouble());
    postStr += "&field2=";
    postStr += String(collectorSensor.tempDouble());
    postStr += "&field3=";
    postStr += String(roomTemp);
    postStr += "&field4=";
    postStr += String(roomHumidity);
    postStr += "&field5=";
    postStr += String(roomPressure);
    postStr += "\r\n\r\n";
    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: " + apiKey + "\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(postStr.length());
    client.print("\n\n");
    client.print(postStr);
    client.stop();
  }
}

