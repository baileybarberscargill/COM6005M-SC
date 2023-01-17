#include <Arduino_LSM6DS3.h>
#include <Wire.h>
#include "rgb_lcd.h"
#include <StopWatch.h>
#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>
#include <WiFiNINA.h>

#include "wifi_settings.h"

bool tripStarted = false;
const int buttonPin = 2;

//Button Logic for Press/Hold
int buttonState = 0;     // current state of the button
int lastButtonState = 0; // previous state of the button
int startPressed = 0;    // the moment the button was pressed
int endPressed = 0;      // the moment the button was released
int holdTime = 0;        // how long the button was hold
int idleTime = 0;        // how long the button was idle
bool buttonHeld = NULL;
int displayInfo = 0;

//Accelerometer & G-Force Variables
float gForceCurrent[3];
float gForceAvg[3];
float xMax;
float yMax;
float xAvg;
float yAvg;

//Wifi
WiFiClient wifi;
const char wifiName[] = SECRET_WIFI;
const char wifiPass[]= SECRET_PASS;
bool connectedToWifi = false;


//MQTT
MqttClient mqttClient(wifi);
bool connectedToMQTT = false;
bool mqttDataToSend = false;
const char mqttBroker[] = SECRET_mqttBroker;
const int mqttPort = SECRET_mqttPort;
const char mqttTopic[] = SECRET_mqttTopic;

struct GForceData {
  unsigned long tripTimeElapsed;
  float xCurrent;
  float yCurrent;
};

int readingsTaken = 1;

struct TripData {
  unsigned long tripTimeElapsed;
  float xAvg;
  float yAvg;
  //struct GPSData gpsData[60];
  struct GForceData gForceData[6]; //1 data every 10 secs, max 60 seconds of data
};

int gForceDataCounter = 0;
struct GForceData tripGForceData[6];

//Only enough memory to store 1 trips at the moment.
//struct TripData tripLog[1];
TripData data;

StopWatch tripTimer;
StopWatch gForceDataTimer;
StopWatch connectionTimer;
int connectionTimeout = false;
rgb_lcd lcd;

void setup() {

  Serial.begin(9600);

  //Set up the gForce StopWatch for Seconds
  gForceDataTimer.setResolution(StopWatch::SECONDS);
  connectionTimer.setResolution(StopWatch::SECONDS);

  mqttClient.setTxPayloadSize(1024);

  //Start the LCD
  lcd.begin(16, 2);

  // Test if the IMU is working
  if (!IMU.begin()) {
    //Serial.println("Failed to initialize IMU");
    lcd.clear();
    lcd.setRGB(255,0, 0);
    lcd.print("Failed to start IMU");
    delay(1500);
  }
  else{
    //Let user know IMU is working
    lcd.clear();
    lcd.setRGB(0, 255, 0);
    lcd.print("IMU started.");
    delay(2000);
  }
  lcd.clear();
  lcd.setRGB(0, 234, 255);
  lcd.print(" Hold button to");
  lcd.setCursor(0,1);
  lcd.print("   start trip");
}

void loop() {
  buttonState = digitalRead(buttonPin); // read the button input

  if (buttonState != lastButtonState) { // button state changed
     updateState();
  }
  lastButtonState = buttonState;        // save state for next loop

  if (tripStarted) { 

    checkForDisplayInfo();

    //Update Max GForce for Accelerating/Breaking
    getCurrentGForce();
    if (gForceCurrent[0] > xMax){
      xMax = gForceCurrent[0];
    }
    if (gForceCurrent[1] > yMax){
      yMax = gForceCurrent[1];
    }

    //Add data to GForceData
    gForceDataAdd();

    //Update Average GForce;
    xAvg = (xAvg + gForceCurrent[0]);
    yAvg = (yAvg + gForceCurrent[1]);
    readingsTaken++;
  }

  if ((!tripStarted) && (mqttDataToSend == true)) {

    connectToWifi();

    if ((WiFi.status() != WL_CONNECTED) && (connectionTimeout == false)){
        connectToWifi();
    }

    if (connectedToWifi == true){

      if (connectedToMQTT == true){

        mqttClient.poll();

        lcd.clear();
        lcd.setCursor(0,0);
        lcd.setRGB(0,255,0);
        lcd.print("Station");
        lcd.setCursor(0,1);
        lcd.print("connected.");
        if (mqttDataToSend == true){
          delay(1500);
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.setRGB(0,255,0);
            lcd.print("Found data");
            lcd.setCursor(0,1);
            lcd.print("to send.");
            delay(1500);
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("Send Trip 1");
            lcd.setCursor(0, 1);
            for (int i = 0; i<6;i++){
              lcd.print(">");
              delay(500);
            }
            Serial.print(sizeof(data));
            String dataToSend = structToJson(data);
            Serial.print(dataToSend);
            Serial.print(sizeof(dataToSend));
            mqttClient.beginMessage(mqttTopic);
            mqttClient.print(dataToSend);
            mqttClient.endMessage();
            mqttDataToSend = false;
            delay(2000);
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("Sent Trip !");
            delay(2000);
            lcd.clear();
            lcd.setRGB(0, 234, 255);
            lcd.print(" Hold button to");
            lcd.setCursor(0,1);
            lcd.print("   start trip");
        }
        else if (mqttDataToSend == false){
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.setRGB(255,0,0);
          lcd.print("No data.");
          delay(2000);
          lcd.clear();
          lcd.setRGB(0, 234, 255);
          lcd.print(" Hold button to");
          lcd.setCursor(0,1);
          lcd.print("   start trip");
          return false;
        }
      }
    }
  }
}

String structToJson(TripData data) {
  // Create a JSON object with a capacity of 1024 bytes
  DynamicJsonDocument doc = DynamicJsonDocument(1024);

  // Add the values from the struct to the JSON object
  doc["tripTimeElapsed"] = data.tripTimeElapsed;
  doc["xAvg"] = data.xAvg;
  doc["yAvg"] = data.yAvg;
  // Iterate over the GforceData array
  JsonArray gForceDataArray = doc.createNestedArray("gForceData");
  for (int i = 0; i <= 6; i++) {
    JsonObject gForceDataObject = gForceDataArray.createNestedObject();
    gForceDataObject["tripTimeElapsed"] = data.gForceData[i].tripTimeElapsed;
    gForceDataObject["xCurrent"] = data.gForceData[i].xCurrent;
    gForceDataObject["yCurrent"] = data.gForceData[i].yCurrent;
  }

  // Print the JSON object to the serial monitor
  String jsonString;
  serializeJson(doc, jsonString);

  return jsonString;
}



void connectToWifi() {

  //We only want to try connecting every 30 seconds
  if (connectionTimer.isRunning() == true && connectionTimer.elapsed() < 30){
    return;
  }

  connectionTimeout = false;

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.setRGB(0,255,0);
  lcd.print("Connecting to");
  lcd.setCursor(0,1);
  lcd.print("WiFi.");

  delay(1000);

  int connectionCount = 0;

  while((connectionCount <= 5) && (WiFi.status() != WL_CONNECTED)){
    if (WiFi.status() == WL_CONNECTED){
      break;
    }
    connectionCount++;
    lcd.clear();
    lcd.setRGB(0,0,255);
    lcd.setCursor(0, 0);
    lcd.print("Wifi Attempt: ");
    lcd.setCursor(0, 1);
    lcd.print(connectionCount);
    WiFi.begin(wifiName, wifiPass);
    delay(1000);
  }

  if (WiFi.status() == WL_CONNECTED){
    connectedToWifi = true;
    connectionTimer.stop();
    connectionTimer.reset();
    lcd.clear();
    lcd.setRGB(0,255,0);
    lcd.setCursor(0, 0);
    lcd.print("Wifi Connected!");
    delay(1000);
    connectToMQTT();
  }

  else{
    lcd.clear();
    lcd.setRGB(255,0,0);
    lcd.setCursor(0, 0);
    lcd.print("Wifi not found.");
    connectedToWifi = false;
    connectionTimeout = true;
    connectionTimer.reset();
    connectionTimer.start();
    delay(1500);
    lcd.clear();
    lcd.setRGB(0, 234, 255);
    lcd.print(" Hold button to");
    lcd.setCursor(0,1);
    lcd.print("   start trip");
  }
}

void connectToMQTT() {

  if (WiFi.status() != WL_CONNECTED){
    Serial.print("NOT CONNECTED TO WIFI TO CONNECT MQTT");
    return;
  }

  else if (WiFi.status() == WL_CONNECTED){

    Serial.print("Attempting to connect to the MQTT broker: ");
    Serial.println(mqttBroker);
    Serial.print("On port: ");
    Serial.println(mqttPort);

    delay(1500);


    if (!mqttClient.connect(mqttBroker, mqttPort)){

      Serial.print(F("MQTT connection failed! Error code = "));
      Serial.println((mqttClient.connectError()));
      connectedToMQTT = false;
      return;
    }
    connectedToMQTT = true;
    lcd.clear();
    lcd.setRGB(0,255,0);
    lcd.setCursor(0,0);
    lcd.print("Connected to");
    lcd.setCursor(0,1);
    lcd.print("MQTT Topic");
    delay(2000);
  }

}

void gForceDataAdd() {

  if (gForceDataTimer.elapsed() >= 9){

    if (gForceDataCounter > 6){
      return;
    }
    else{
      gForceDataCounter = gForceDataCounter + 1;
      GForceData data = {
        data.tripTimeElapsed = tripTimer.elapsed(),
        data.xCurrent = gForceCurrent[0],
        data.yCurrent = gForceCurrent[1],
      };
      tripGForceData[gForceDataCounter] = data;
      gForceDataTimer.reset();
      gForceDataTimer.start();
    }
  }
}

void startStopTrip () {
  //If the trip was started, we need to stop the trip.
  if (tripStarted) {

    tripStarted = false;

    lcd.clear();
    lcd.setRGB(0, 0, 255);
    lcd.print("Ended trip.");
    delay(1000);
    //Display Trip Data
    lcd.clear();
    lcd.print("Trip Time:");
    lcd.setCursor(0, 1);      
    lcd.print(convertTripTime(tripTimer.elapsed()));
    delay(1500);
    //Show avg x G
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Avg Accel/Brake:");
    lcd.setCursor(0, 1);
    //Calculate xAvg
    xAvg = xAvg / readingsTaken;
    lcd.print(String(xAvg) + "g");
    delay(1500);
    //Show avg y-g
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Avg Turning G:");
    lcd.setCursor(0, 1);
    yAvg = yAvg / readingsTaken;
    lcd.print(String(yAvg)+"g");
    delay(1500);
    //Show max x-g
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Max Accel/Brake:");
    lcd.setCursor(0, 1);
    lcd.print(String(xMax)+"g");
    delay(1500);
    //Show max y-g
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Max Turning G:");
    lcd.setCursor(0, 1);
    lcd.print(String(yMax)+"g");

    delay(1500);
    lcd.clear();
    lcd.print("Saving Trip");
    lcd.setCursor(0, 1);
    for (int i = 0; i<6;i++){
      lcd.print(">");
      delay(500);
    }
    data.tripTimeElapsed = tripTimer.elapsed();
    data.xAvg = xAvg;
    data.yAvg = yAvg;
      //data.gpsData = NULL,
    for(int i=0; i<=6; i++)
    {
      data.gForceData[i] = tripGForceData[i];
    };

      Serial.print("PACKET SIZE : ");
      Serial.print(sizeof(data));
      //String dataToShow = structToJson(data);
      //Serial.print(dataToShow);
      mqttDataToSend = true;

    lcd.clear();
    lcd.setRGB(0, 255, 0);
    lcd.setCursor(0,0);
    lcd.print("SAVED!");
    delay(2000);
    lcd.clear();
    lcd.setRGB(0, 234, 255);
    lcd.print(" Hold button to");
    lcd.setCursor(0,1);
    lcd.print("   start trip");
    return;
  }

  //If no trip has begun, start it
  if (!tripStarted) {

    tripStarted = true;

    lcd.clear();
    lcd.setRGB(0, 255, 0);
    lcd.print("Started trip.");
    lcd.setCursor(0, 1);
    lcd.print("Drive safely!");

    delay(2000);

    //Set current display to the Trip Time
    displayInfo = 1;

    xAvg = 0;
    yAvg = 0;

    //Reset trip timer
    tripTimer.reset();
    tripTimer.start();

    gForceDataTimer.reset();
    gForceDataTimer.start();
    return;
  }

}

String convertTripTime(long timer) {

        //Convert current trip time into Hours, Minutes, Seconds
        int tripTimerSeconds = timer / 1000;
        int tripTimerMinutes = tripTimerSeconds / 60;
        int tripTimerHours = tripTimerMinutes / 60;

        tripTimerSeconds %= 60;
        tripTimerMinutes %= 60;
        tripTimerHours %= 24;

        String tripTimerHMS;

        if (tripTimerHours > 0) {
          tripTimerHMS += String(tripTimerHours) + ":";
        }  
        if (tripTimerMinutes < 10) {
          tripTimerHMS += "0";
        }    
        tripTimerHMS += String(tripTimerMinutes) + ":";

        if (tripTimerSeconds < 10) {
          tripTimerHMS += "0";
        }
        tripTimerHMS += String(tripTimerSeconds);

        return tripTimerHMS;

}

void updateState() {

  // the button has been just pressed
  if (buttonState == HIGH) {
      startPressed = millis();

  // the button has been just released
  } else {
      endPressed = millis();
      holdTime = endPressed - startPressed;

      if (holdTime >= 1000) {
        startStopTrip();
      }

      else {
        changeInfoDisplay();
      }
  }

}

void changeInfoDisplay() {

  //If a trip hasn't started, display this.tripStarted
  if (!tripStarted) {

    lcd.clear();
    lcd.setRGB(255, 0 , 0);
    lcd.print("No trip in");
    lcd.setCursor(0, 1);
    lcd.print("progress.");
    delay(2000);
    lcd.clear();
    lcd.setRGB(0, 234, 255);
    lcd.print(" Hold button to");
    lcd.setCursor(0,1);
    lcd.print("   start trip");
    return;
  }

  //If we reach the maximum display, go back to the beginning
  if (displayInfo == 3){
    displayInfo = 0;
  }
  displayInfo = displayInfo + 1;

  lcd.clear();
}

void checkForDisplayInfo() {

    //1 = CURRENT TRIP TIME
    //2 = Current Acceleration/Braking
    //3 = Current Turning Speed

  lcd.clear();

  lcd.setRGB(255,255,2550);

  if (displayInfo == 1){
    lcd.setCursor(0, 0);
    lcd.print("Trip Time:");
    lcd.setCursor(0, 1);
    lcd.print(convertTripTime(tripTimer.elapsed()));
  }

  if (displayInfo == 2) {
    getCurrentGForce();
    lcd.setCursor(0, 0);
    lcd.print("Curr Acc/Brake:");
    lcd.setCursor(0, 1);
    lcd.print("X: ");
    lcd.print(gForceCurrent[0]);
  }

  if (displayInfo == 3){
    getCurrentGForce();
    lcd.setCursor(0, 0);
    lcd.print("Curr Turn:");
    lcd.setCursor(0, 1);
    lcd.print("Y: ");
    lcd.print(gForceCurrent[1]);
  }

  delay(250);

}

void getCurrentGForce() {

  float x, y, z;

    if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);

    gForceCurrent[0] = fabs(x);
    gForceCurrent[1] = fabs(y);

  }
}
