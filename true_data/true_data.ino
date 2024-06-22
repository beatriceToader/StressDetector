#include "secrets.h"
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"
#include <Wire.h>  
#include "HT_SSD1306Wire.h"
#include "MAX30105.h"
#include "heartRate.h"
#include <OneWire.h>
#include <DallasTemperature.h>

//declare the pins used for the display
#define SDA_OLED 4 
#define SCL_OLED 15
#define RST_OLED 16
//define the OLED display object
SSD1306Wire  factory_display(0x3c, 400000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

//declare the pins for the EDA analog readings and the circuit's parameters
#define EDA_PIN 34
#define ADC_REF_VOLTAGE 5
#define SERIES_RESISTOR 100000.0

//declare SDA and SLC pins used for the MAX30102 sensor
#define SDA_HR 21
#define SCL_HR 22

//heart-rate sensor declaration
MAX30105 hrSensor;
int foundHR = 0;      //variable that checks if a heart beat was found and can be sent
long lastBeat = 0;    //variable used to store the time at which the last beat occurred
float beatsPerMinute; //variable used to store the bpm value

//declare the pin where the data line of the DS18B20 sensor is connected
#define TEMP_PIN 17 

//temperature sensor declaration   
OneWire oneWireTemp(TEMP_PIN);
DallasTemperature tempSensor(&oneWireTemp);

//declare the MQTT topics used to publish/subscribe
#define AWS_IOT_PUBLISH_TOPIC   "esp32/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/sub"

//declare the MQTT client
WiFiClientSecure net = WiFiClientSecure();
MQTTClient client = MQTTClient(256);

//function to display the logo of the project
void logo(){
	factory_display.clear();
  factory_display.drawString(15, 15, "STRESS\nDETECTOR");
	factory_display.display();
}

//function used to connect the ESP32 to Wifi and to AWS
void connectAWS(){

  //connect to wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("Connecting to Wi-Fi");

  factory_display.drawString(0, 0, "Connecting to Wi-Fi");
	factory_display.display(); 

  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print("wifi.");
  }

  //configure the connection using the AWS IoT device credentials 
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  //connect the device to AWS 
  client.begin(AWS_IOT_ENDPOINT, 8883, net);

  //create the messageHandler for the messages received from AWS
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

  //subcribe to the topic that is used to send data from AWS to the ESP32
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);

  Serial.println("AWS IoT Connected!");

  factory_display.clear();
  factory_display.drawString(0, 0, "AWS IoT Connected!");
	factory_display.display();
}

//function used to do the setup for the MAX30102 sensor
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

//function that retrieves the temperature value
float getTemperature(){
  //get the value of the temperature in Celsius
  tempSensor.requestTemperatures();
  float tempValue = tempSensor.getTempCByIndex(0);

  return tempValue;
}

//function that retrieves the value of the heart rate
int getHeartRate(){
  //loop that goes until a new value for the heart rate is found or for 5 seconds 
  while(!foundHR && (millis() - lastBeat < 5000)){
    //get the ir value
    long intensity = hrSensor.getIR();

    //check for a new heart beat
    if (checkForBeat(intensity) == true)
    {
      long timePassed = millis() - lastBeat;
      lastBeat = millis();

      //compute a new value for the heart rate
      beatsPerMinute = 60 / (timePassed / 1000.0);

      if (beatsPerMinute > 20 && beatsPerMinute < 255)
      {
        foundHR=1;
        return beatsPerMinute;
      }
    }

    //make sure you don't lose the aws connection
    client.loop();
  }
}

//function that returns the skin conductance value
float getEDA(){
  //read the digital value from the ADC pin
  int potValue = analogRead(EDA_PIN);

  //compute the voltage measured in the point of the ADC pin
  float voltage = (potValue / 4095.0) * ADC_REF_VOLTAGE;

  //compute the corresponding value for the skin condunctance
  float conductance = voltage / SERIES_RESISTOR;

  //turn the conductance value in micro-siemens
  float edaValue = conductance * 1000000.0;

  return edaValue;
}

//function used to publish new data extracted from the sensors on the AWS IoT Core
void publishMessage()
{
  //get the data from the sensors
  StaticJsonDocument<200> doc;
  doc["eda"] = getEDA();
  doc["heart_rate"] = getHeartRate();
  doc["temperature"] = getTemperature();
  foundHR=0;

  //transform the data to obtain the required format for the AWS IoT Core
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer);
  StaticJsonDocument<300> AWSdoc;
  AWSdoc["body"] = jsonBuffer;
  char AWSjsonBuffer[512];
  serializeJson(AWSdoc, AWSjsonBuffer);

  //publish the data on the topic that is used to send data from the ESP32 to AWS 
  if(client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer)){
    Serial.println("Success");
  }
  else{
    Serial.println("Failed");
  }
}

//variable to keep track of the time passed between predictions
float lastMess = 0;

//function that is used when messages arrive from AWS
void messageHandler(String &topic, String &payload) {
  //deserialize the incoming message to the doc 
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.println("Failed to parse JSON payload");
    return;
  }

  //check if the message has the stressLevel field
  if(doc.containsKey("stressLevel")){

    //compute the time passed from the last prediction
    float delayMess = millis() - lastMess;
    lastMess = millis();
    float delay = delayMess/1000.0;

    Serial.println("incoming: " + topic + " - " + payload);
    Serial.println("Message arrived after: " + String(delay) + " seconds");

    int stressLevel = doc["stressLevel"];

    String stressMessage;
    if(stressLevel==0){
      stressMessage = "No Stress";
    }
    else if(stressLevel==1){
      stressMessage = "Low Stress";
    }
    else if(stressLevel==2){
      stressMessage = "High Stress";
    }

    //print the progress bar according with the stressLevel value
    int progressBar = map(stressLevel, 0, 2, 0, 100);

    //diplay the level of stress
    factory_display.clear();
    factory_display.drawString(0, 0, stressMessage);
    factory_display.drawProgressBar(0, 10, 120, 10, progressBar);
    factory_display.display();
  }
  else{
    //display the notification message
    Serial.println("incoming: " + topic + " - " + payload);

    String message = doc["message"];

    factory_display.clear();
    factory_display.drawString(0, 0, message);
    factory_display.display();
    delay(5000);
  }
}

void setup() {
  Serial.begin(115200);
  delay(10);

  //initialize the I2C communication for the MAX30102 sensor
  Wire1.begin(SDA_HR,SCL_HR); 
  heartRateSensorSetup();

  //initialize the temperature sensor
  tempSensor.begin();

  //initialize the display
  factory_display.init();

  //display the logo
  logo();      

	delay(300);
	factory_display.clear();

  //connect the ESP32 to AWS
  connectAWS();
}

//publish new sensor data and keep the AWS connection
void loop() {
  publishMessage();
  client.loop();
  delay(1000);
}
