#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>
#include <WiFiNINA.h>
#include "ThingSpeak.h"

#include "wifi_settings.h"

const char wifiName[] = SECRET_WIFI;
const char wifiPass[] = SECRET_PASS; 
const unsigned long tsChannel_ID = SECRET_tsChannelID;
const char tsWriteApiKey[] = SECRET_tsWriteAPIKey;

const char mqttBroker[] = SECRET_mqttBroker;
int mqttPort = SECRET_mqttPort;
const char mqttTopic[] = SECRET_mqttTopic;

struct GForceData {
  long tripTimeElapsed;
  float xCurrent;
  float yCurrent;
};

//Data Variables
long tripTimeElapsed;
float xAvg;
float yAvg;
struct GForceData gForceData[6];

WiFiClient wifi;
MqttClient mqttClient(wifi);

void setup() {

  mqttClient.setTxPayloadSize(1024);

  Serial.begin(9600);
  Serial.print("Connecting to ");
  Serial.println(wifiName);

  while(WiFi.begin(wifiName, wifiPass) != WL_CONNECTED) {
    Serial.println("Attempting connection..");
    delay(5000);
  }

  Serial.print("Connected.");
  //connectToMQTT();

    ThingSpeak.begin(wifi);

  String testJson = "{\"tripTimeElapsed\":75926,\"xAvg\":0.014295,\"yAvg\":0.062031,\"gForceData\":[{\"tripTimeElapsed\":0,\"xCurrent\":0,\"yCurrent\":0},{\"tripTimeElapsed\":9084,\"xCurrent\":0.010376,\"yCurrent\":0.062012},{\"tripTimeElapsed\":17908,\"xCurrent\":0.01001,\"yCurrent\":0.060303},{\"tripTimeElapsed\":26992,\"xCurrent\":0.014282,\"yCurrent\":0.062134},{\"tripTimeElapsed\":36075,\"xCurrent\":0.013916,\"yCurrent\":0.062866},{\"tripTimeElapsed\":44899,\"xCurrent\":0.014648,\"yCurrent\":0.054077},{\"tripTimeElapsed\":339039,\"xCurrent\":4.852726e30,\"yCurrent\":7.555079e31}]}";
  printJsonData(testJson);

}

void loop() {
  mqttClient.poll();

}

void connectToMQTT() {

  if (!mqttClient.connect(mqttBroker, mqttPort)){

    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());
    return;
  }

   Serial.println("You're connected to the MQTT broker!");
  Serial.println();

  // set the message receive callback
  mqttClient.onMessage(onMqttMessage);

  Serial.print("Subscribing to topic: ");
  Serial.println(mqttTopic);
  Serial.println();

  // subscribe to a topic
  mqttClient.subscribe(mqttTopic);

  // topics can be unsubscribed using:
  // mqttClient.unsubscribe(topic);

  Serial.print("Waiting for messages on topic: ");
  Serial.println(mqttTopic);
  Serial.println();

}

void printJsonData(String json){

  DynamicJsonDocument receivedData(1024);
  deserializeJson(receivedData, json);

  // Extract the data into individual variables
  tripTimeElapsed = receivedData["tripTimeElapsed"];
  xAvg = receivedData["xAvg"];
  yAvg = receivedData["yAvg"];
    
  Serial.print("");
  Serial.println(tripTimeElapsed);
  Serial.println(xAvg);
  Serial.println(yAvg);

  // Extract the GforceData array
  JsonArray gForceDataArray = receivedData["gForceData"];
  for (int i = 0; i < gForceDataArray.size(); i++) {
    long tripTimeElapsed = gForceDataArray[i]["tripTimeElapsed"];
    float xCurrent = gForceDataArray[i]["xCurrent"];
    float yCurrent = gForceDataArray[i]["yCurrent"];
    Serial.println("tripTimeElapsed: " + String(tripTimeElapsed));
    Serial.println("xCurrent: " + String(xCurrent));
    Serial.println("yCurrent: " + String(yCurrent));
  }

  //Now we want to upload our data to ThingSpeak
  ThingSpeak.setField(1, String((tripTimeElapsed/1000)));
  ThingSpeak.setField(2, String(xAvg));
  ThingSpeak.setField(3, String(yAvg));   

  ThingSpeak.writeFields(tsChannel_ID, tsWriteApiKey);

     Serial.println("Written TIME: " + String((tripTimeElapsed/1000)));
     Serial.println("Written XAVG: " + String(xAvg));
     Serial.println("Written YAVG: " + String(yAvg));


  delay(20000);
  
     Serial.println("ARRAY SIZE: " + String(gForceDataArray.size()));

  for (int i=0; i < gForceDataArray.size(); i++) {
    gForceData[i].tripTimeElapsed = receivedData["gForceData"][i]["tripTimeElapsed"];
    gForceData[i].xCurrent = receivedData["gForceData"][i]["xCurrent"];
    gForceData[i].yCurrent = receivedData["gForceData"][i]["yCurrent"];
    ThingSpeak.setField(4, gForceData[i].tripTimeElapsed);
    ThingSpeak.setField(5, gForceData[i].xCurrent);
    ThingSpeak.setField(6, gForceData[i].yCurrent);
    Serial.println("Written " + String(gForceData[i].tripTimeElapsed));
    Serial.println("Written " + String(gForceData[i].xCurrent));
    Serial.println("Written " + String(gForceData[i].yCurrent));
    ThingSpeak.writeFields(tsChannel_ID, tsWriteApiKey);
    Serial.println("Written gForce: " + String(i) + "/" + String(gForceDataArray.size()));
    delay(20000);
  }

  Serial.println("ALL DATA WRITTEN!");

}

String gForceDataToString(struct GForceData data[6]){
  String dataString;
  for (int i = 0; i < 6; i++) {
    dataString += String(data[i].tripTimeElapsed) + "," + String(data[i].xCurrent) + "," + String(data[i].yCurrent);
    if (i < 5) {
      dataString += ",";
    }
  }
  return dataString;
}

void onMqttMessage(int messageSize) {
  // we received a message, print out the topic and contents
  Serial.println("Received a message with topic '");
  Serial.print(mqttClient.messageTopic());
  Serial.print("', length ");
  Serial.print(messageSize);
  Serial.println(" bytes:");

  // use the Stream interface to print the contents
  while (mqttClient.available()) {
    Serial.print((char)mqttClient.read());
  }

  DynamicJsonDocument receivedData(1024);
  deserializeJson(receivedData, mqttClient);

  // Extract the data into individual variables
  tripTimeElapsed = receivedData["tripTimeElapsed"];
  xAvg = receivedData["xAvg"];
  yAvg = receivedData["yAvg"];
    
  Serial.print("");
  Serial.println(tripTimeElapsed);
  Serial.println(xAvg);
  Serial.println(yAvg);

  // Extract the GforceData array
  JsonArray gForceDataArray = receivedData["gForceData"];
  for (int i = 0; i < gForceDataArray.size(); i++) {
    long tripTimeElapsed = gForceDataArray[i]["tripTimeElapsed"];
    float xCurrent = gForceDataArray[i]["xCurrent"];
    float yCurrent = gForceDataArray[i]["yCurrent"];
    Serial.println("tripTimeElapsed: " + String(tripTimeElapsed));
    Serial.println("xCurrent: " + String(xCurrent));
    Serial.println("yCurrent: " + String(yCurrent));
  }

  //Now we want to upload our data to ThingSpeak
  ThingSpeak.setField(1, String((tripTimeElapsed/1000)));
  ThingSpeak.setField(2, String(xAvg));
  ThingSpeak.setField(3, String(yAvg));   

  ThingSpeak.writeFields(tsChannel_ID, tsWriteApiKey);

     Serial.println("Written TIME: " + String((tripTimeElapsed/1000)));
     Serial.println("Written XAVG: " + String(xAvg));
     Serial.println("Written YAVG: " + String(yAvg));


  delay(20000);
  
     Serial.println("ARRAY SIZE: " + String(gForceDataArray.size()));

  for (int i=0; i < gForceDataArray.size(); i++) {
    gForceData[i].tripTimeElapsed = receivedData["gForceData"][i]["tripTimeElapsed"];
    gForceData[i].xCurrent = receivedData["gForceData"][i]["xCurrent"];
    gForceData[i].yCurrent = receivedData["gForceData"][i]["yCurrent"];
    ThingSpeak.setField(4, gForceData[i].tripTimeElapsed);
    ThingSpeak.setField(5, gForceData[i].xCurrent);
    ThingSpeak.setField(6, gForceData[i].yCurrent);
    Serial.println("Written " + String(gForceData[i].tripTimeElapsed));
    Serial.println("Written " + String(gForceData[i].xCurrent));
    Serial.println("Written " + String(gForceData[i].yCurrent));
    ThingSpeak.writeFields(tsChannel_ID, tsWriteApiKey);
    Serial.println("Written gForce: " + String(i) + "/" + String(gForceDataArray.size()));
    delay(20000);
    //ThingSpeak.setField(4, gForceData[i].tripTimeElapsed);
    //ThingSpeak.setField(5, gForceData[i].xCurrent);
    //
    //ThingSpeak.setField(6, gForceData[i].yCurrent);
  }

  Serial.println("ALL DATA WRITTEN!");

  //ThingSpeak.writeFields(tsChannel_ID, tsWriteApiKey);
}
