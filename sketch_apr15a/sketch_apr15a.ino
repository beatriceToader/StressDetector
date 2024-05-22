#include "secrets.h"
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"
#include <Wire.h>  
#include "HT_SSD1306Wire.h"

#define SDA_OLED 4
#define SCL_OLED 15
#define RST_OLED 16

SSD1306Wire  factory_display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED); 

// The MQTT topics that this device should publish/subscribe
#define AWS_IOT_PUBLISH_TOPIC   "esp32/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/sub"

WiFiClientSecure net = WiFiClientSecure();
MQTTClient client = MQTTClient(256);

void logo(){
	factory_display.clear();
  factory_display.drawString(15, 15, "STRESS\nDETECTOR");
	factory_display.display();
}

void connectAWS()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("Connecting to Wi-Fi");

  factory_display.drawString(0, 0, "Connecting to Wi-Fi");
	factory_display.display(); 

  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print("nu.");
  }

  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.begin(AWS_IOT_ENDPOINT, 8883, net);

  // Create a message handler
  client.onMessage(messageHandler);

  Serial.print("Connecting to AWS IOT");

  factory_display.clear();
  factory_display.drawString(0, 0, "Connecting to AWS IOT");
	factory_display.display();

  while (!client.connect(THINGNAME)) {
    Serial.print("aws.");
    delay(100);
  }

  if(!client.connected()){
    Serial.println("AWS IoT Timeout!");

  factory_display.clear();
  factory_display.drawString(0, 0, "Connecting to AWS IOT");
	factory_display.display();
    return;
  }

  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);

  Serial.println("AWS IoT Connected!");
  factory_display.clear();
  factory_display.drawString(0, 0, "AWS IoT Connected!");
	factory_display.display();
}

void publishMessage()
{
  StaticJsonDocument<200> doc;
  doc["time"] = millis();
  doc["sensor_a0"] = analogRead(0);
  doc["number"] = 12;
  char jsonBuffer[512];

  unsigned long time = doc["time"];
  int sensor_a0 = doc["sensor_a0"];
  int number = doc["number"];

  String printString = "Time: " + String(time) + "\n" + "Sensor Value: " + String(sensor_a0) + "\n" + "Number: "+ String(number);

  factory_display.clear();
  factory_display.drawString(0, 0, printString.c_str());
	factory_display.display();

  serializeJson(doc, jsonBuffer); 
  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}

void messageHandler(String &topic, String &payload) {
  Serial.println("incoming: " + topic + " - " + payload);

  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload);
  const char* message = doc["message"];

  factory_display.clear();
  factory_display.drawString(0, 0, message);
	factory_display.display();
}

void setup() {
  Serial.begin(9600);

  factory_display.init();
  logo();

	delay(300);
	factory_display.clear();
  connectAWS();
}

void loop() {
  publishMessage();
  client.loop();
  delay(1000);
}
