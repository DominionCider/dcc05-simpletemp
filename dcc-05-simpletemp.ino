#include <ESP8266WiFi.h>

// Define these in the config.h file
//#define WIFI_SSID "yourwifi"
//#define WIFI_PASSWORD "yourpassword"
//#define INFLUX_HOSTNAME "data.example.com"
//#define INFLUX_PORT 8086
//#define INFLUX_PATH "/write?db=<database>&u=<user>&p=<pass>"
//#define WEBSERVER_USERNAME "something"
//#define WEBSERVER_PASSWORD "something"
#include "config.h"

#define DEVICE_NAME "dcc-05-simpletemp"

#define AMBER_LED_PIN 16
#define GREEN_LED_PIN 14
#define ONE_WIRE_PIN 2

#define N_SENSORS 2

byte sensorAddr[N_SENSORS][8] = {
  {0x28, 0xFF, 0x07, 0x18, 0x01, 0x15, 0x04, 0xE4}, // (temp)
  {0x28, 0x72, 0x05, 0x80, 0x06, 0x00, 0x00, 0x32}  // (board)
};
char * sensorNames[N_SENSORS] = {
  "temp",
  "board",
};



#include "libdcc/webserver.h"
#include "libdcc/onewire.h"
#include "libdcc/influx.h"


void setup() {
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(AMBER_LED_PIN, OUTPUT);
  digitalWrite(AMBER_LED_PIN, HIGH);
  digitalWrite(GREEN_LED_PIN, LOW);

  Serial.begin(115200);

  WiFi.mode(WIFI_STA);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  server.on("/restart", handleRestart);
  server.on("/status", handleStatus);
  server.on("/sensors", handleSensors);
  server.onNotFound(handleNotFound);
  server.begin();
}


unsigned long lastIteration;
void loop() {
  server.handleClient();
  delay(100);

  if (millis() < lastIteration + 10000) return;
  lastIteration = millis();

  String sensorBody = String(DEVICE_NAME) + " uptime=" + String(millis()) + "i";
  char readSuccess[N_SENSORS];

  digitalWrite(GREEN_LED_PIN, HIGH);

  takeAllMeasurements();

  // Read each ds18b20 device individually
  float temp[N_SENSORS];
  for (int i=0; i<N_SENSORS; i++) {
    Serial.print("Temperature sensor ");
    Serial.print(i);
    Serial.print(": ");
    if (readTemperature(sensorAddr[i], &temp[i])) {
      Serial.print(temp[i]);
      Serial.println();
      sensorBody += String(",") + sensorNames[i] + "=" + String(temp[i], 3);
      readSuccess[i] = 1;
    } else {
      readSuccess[i] = 0;
    }
    delay(100);
  }
  Serial.println(sensorBody);

  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(AMBER_LED_PIN, HIGH);
    delay(1000);
    Serial.println("Connecting to wifi...");
    return;
  }
  digitalWrite(AMBER_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, HIGH);
  Serial.println("Wifi connected to " + WiFi.SSID() + " IP:" + WiFi.localIP().toString());

  WiFiClient client;
  if (client.connect(INFLUX_HOSTNAME, INFLUX_PORT)) {
    Serial.println(String("Connected to ") + INFLUX_HOSTNAME + ":" + INFLUX_PORT);
    delay(50);

    postRequest(sensorBody, client);

    client.stop();
  } else {
    digitalWrite(AMBER_LED_PIN, HIGH);
  }
  digitalWrite(GREEN_LED_PIN, LOW);
  delay(100);
}


