#include "secrets.h"
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"
#include <Wire.h>  
#include "HT_SSD1306Wire.h" //display
#include "MAX30105.h" //hr
#include "heartRate.h" //hr
#include <OneWire.h>
#include <DallasTemperature.h>

//display set-up
#define SDA_OLED 5 //(asta il modific cu 4)
#define SCL_OLED 15
#define RST_OLED 16
SSD1306Wire  factory_display(0x3c, 400000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

//define SDA and SLC for the wire of the MAX30102 sensor
#define SDA_HR 21
#define SCL_HR 22

//heart-rate sensor declaration
MAX30105 hrSensor;
int foundHR = 0;      //checks if a heart beat was found and can be sent
long lastBeat = 0;    //time at which the last beat occurred
float beatsPerMinute; //the bpm value

//define the pin where the DS18B20 sensor is connected
#define TEMP_PIN 4 //(trebuie modificat dupa ce il mut)

//temperature sensor declaration   
OneWire oneWireTemp(TEMP_PIN);
DallasTemperature tempSensor(&oneWireTemp);

// MQTT topics used to publish/subscribe
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
    Serial.print("wifi.");
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
  if (!hrSensor.begin(Wire1, I2C_SPEED_FAST)) //400kHz speed
  {
    Serial.println("Something wrong with the MAX30102 sensor.");
    while (1);
  }

  //set up the heart rate sensor and turn on the red led
  hrSensor.setup();
  hrSensor.setPulseAmplitudeRed(0x0A);
  hrSensor.setPulseAmplitudeGreen(0);
}

//function that retrieves the value of the temperature
float getTemperature(){
  //get the value of the temperature in Celsius
  tempSensor.requestTemperatures();
  float tempValue = tempSensor.getTempCByIndex(0);

  Serial.print("Temp=");
  Serial.println(tempValue);

  return tempValue;
}

//function that retrieves the value of the heart rate
int getHeartRate(){
  while(!foundHR){
    long intensity = hrSensor.getIR();

    if (checkForBeat(intensity) == true)
    {
      long timePassed = millis() - lastBeat;
      lastBeat = millis();

      beatsPerMinute = 60 / (timePassed / 1000.0);

      if (beatsPerMinute > 20 && beatsPerMinute < 255)
      {
        foundHR=1;
        return beatsPerMinute;
      }
    }

     Serial.print("IR=");
     Serial.print(intensity);
     Serial.print(", BPM=");
     Serial.print(beatsPerMinute);

    if (intensity < 50000)
      Serial.print(" No finger?");

    Serial.println(); 
    client.loop();  //make sure you don't lose the aws connection
  }
}

void publishMessage()
{
  StaticJsonDocument<200> doc;
  doc["time"] = millis();
  doc["temperature"] = getTemperature();
  doc["heart_rate"] = getHeartRate();
  foundHR=0;

  char jsonBuffer[512];

  unsigned long time = doc["time"];
  float temperature = doc["temperature"];
  int heart_rate = doc["heart_rate"];

  String printString = "Time: " + String(time) + "\n" + "Temperature (ºC): " + String(temperature) + "\n" + "Heart Rate: "+ String(heart_rate);

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

  tempSensor.begin();         //set up temperature sensor

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
