#include "secrets.h"
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"
#include <Wire.h>  
#include "HT_SSD1306Wire.h" //display
#include "MAX30105.h" //hr
#include "heartRate.h" //hr

//display set-up
#define SDA_OLED 4
#define SCL_OLED 15
#define RST_OLED 16
SSD1306Wire  factory_display(0x3c, 400000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

//define SDA and SLC for MAX30102 sensor
#define SDA_HR 21
#define SCL_HR 22

//heart-rate sensor declaration
MAX30105 particleSensor;
int foundHR = 0;      //checks if a heart beat was found and can be sent
long lastBeat = 0;    //time at which the last beat occurred
float beatsPerMinute; //the bpm value

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

void connectAWS(){
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

void heartRateSensorSetup(){
   //initialize sensor
  if (!particleSensor.begin(Wire1, I2C_SPEED_FAST)) //400kHz speed
  {
    Serial.println("MAX30105 was not found. Please check wiring/power.");
    while (1);
  }
  Serial.println("Place your index finger on the sensor with steady pressure.");

  particleSensor.setup();                     //configure sensor with default settings
  particleSensor.setPulseAmplitudeRed(0x0A);  //turn Red LED to low to indicate sensor is running
  particleSensor.setPulseAmplitudeGreen(0);   //Turn off Green LED
}

//get the heart rate value
int getHeartRate(){
  while(!foundHR){
    long irValue = particleSensor.getIR();

    if (checkForBeat(irValue) == true)
    {
      long delta = millis() - lastBeat;
      lastBeat = millis();

      beatsPerMinute = 60 / (delta / 1000.0);

      if (beatsPerMinute < 255 && beatsPerMinute > 20)
      {
        foundHR=1;
        return beatsPerMinute;
      }
    }

    // Serial.print("IR=");
    // Serial.print(irValue);
    // Serial.print(", BPM=");
    // Serial.print(beatsPerMinute);

    if (irValue < 50000)
      Serial.print(" No finger?");

    Serial.println(); 
    client.loop();  //make sure you don't lose the aws connection
  }
}

void publishMessage()
{
  StaticJsonDocument<200> doc;
  doc["time"] = millis();
  doc["sensor_a0"] = analogRead(0);
  doc["heart_rate"] = getHeartRate();
  foundHR=0;

  char jsonBuffer[512];

  unsigned long time = doc["time"];
  int sensor_a0 = doc["sensor_a0"];
  int heart_rate = doc["heart_rate"];

  String printString = "Time: " + String(time) + "\n" + "Sensor Value: " + String(sensor_a0) + "\n" + "Heart Rate: "+ String(heart_rate);

  factory_display.clear();
  factory_display.drawString(0, 0, printString.c_str());
  factory_display.display();

  serializeJson(doc, jsonBuffer);
  if(client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer)){
    Serial.println("Success");
  }
  else{
    Serial.println("Failed");
  }
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
  Serial.begin(115200);
  delay(10);

  Wire1.begin(SDA_HR,SCL_HR); //set upt the wire for the MAX30102 sensor
  heartRateSensorSetup();     //initialize the sensor

  factory_display.init();     //initialize the display
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
