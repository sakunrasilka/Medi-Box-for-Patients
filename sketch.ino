#include <PubSubClient.h>
#include <WiFi.h>
#include "DHTesp.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP32Servo.h>

#define BUZZER_PIN 4
#define LDR_R_PIN 33
#define LDR_L_PIN 32

const int servoPin = 18;
double gammavalve = 0.75;  
Servo myservo;
int current_pos =0;
int pos = 0;
double T_offset = 30;
float min_angle = 0;
int D = 0;
int I = 0;

const int dhtPin = 14;
char tempar[6];
char sensorLarr[6];
char sensorRarr[6];

WiFiClient espClient;
PubSubClient mqttClient(espClient);
DHTesp dhtSensor;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

bool isScheduledON = false;
unsigned long scheduledOnTime;

void setup() {
  Serial.begin(115200);
  setupWifi();
  setupMqtt();
  dhtSensor.setup(dhtPin,DHTesp::DHT22);
  timeClient.begin();
  timeClient.setTimeOffset(5.5*3600);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LDR_R_PIN, INPUT);
  pinMode(LDR_L_PIN, INPUT);
  digitalWrite(BUZZER_PIN, LOW);
  myservo.attach(servoPin, 500, 2400);
}

void loop() {
  if(!mqttClient.connected()){
    connectToBroker();
  }

  mqttClient.loop();
  updatetemp();
  mqttClient.publish("EE-21",tempar);
  mqttClient.publish("EE-21_LDR_L", sensorLarr);
  mqttClient.publish("EE-21_LDR_R", sensorRarr);
  checkSchedule();
  motorAngle();
  if(min_angle>current_pos){
    for (pos = current_pos; pos <= min_angle; pos += 1) {
    myservo.write(pos);
    delay(10);}
    current_pos= min_angle;
  }
  else if(min_angle<current_pos){
  for (pos = current_pos; pos >= min_angle; pos -= 1) {
    myservo.write(pos);
    delay(10);}
    current_pos= min_angle;
  }}

void setupWifi(){
  WiFi.begin("Wokwi-GUEST", "");
  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("wiFi Connected");
  Serial.println("IP address :");
  Serial.println(WiFi.localIP());
}

void setupMqtt(){
  mqttClient.setServer("test.mosquitto.org",1883);
  mqttClient.setCallback(recieveCallback);
}

void connectToBroker(){
  while(!mqttClient.connected()){
    Serial.print("Attempting the mqtt connection ");
    if(mqttClient.connect("ESP32-12212332")){
      Serial.println("Connected");
      mqttClient.subscribe("EE-21_Manager");
      mqttClient.subscribe("EE-21_Alarm");
      mqttClient.subscribe("EE-21_Angle");
      mqttClient.subscribe("EE-21_Control");
    }
    else{
      Serial.print("Failed");
      Serial.print(mqttClient.state());
      delay(5000);
    }
  }
}

void recieveCallback(char* topic, byte* payload, unsigned int length){
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  char payloadCharAr[length];
  for(int i=0;i < length; i++){
    Serial.print((char)payload[i]);
    payloadCharAr[i] = (char)payload[i];
  }
  Serial.println();

  if(strcmp(topic,"EE-21_Manager") == 0){
    if(payloadCharAr[0] =='1'){
      //tone(BUZZER_PIN,256);
    }
    else{
      noTone(BUZZER_PIN);
    }}
    else if(strcmp(topic,"EE-21_Alarm") == 0){
    if(payloadCharAr[0] =='A'){
      isScheduledON = false;
    }
    else{
      isScheduledON = true;
      scheduledOnTime = atol(payloadCharAr);
    }}
      else if(strcmp(topic,"EE-21_Angle") == 0){
        T_offset = atof(payloadCharAr);
    }
      else if(strcmp(topic,"EE-21_Control") == 0){
        gammavalve = atof(payloadCharAr);}}

void updatetemp(){
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  String(data.temperature,2).toCharArray(tempar,6);
}

unsigned long getTime(){
  timeClient.update();
  return timeClient.getEpochTime();
}

void checkSchedule(){
  if(isScheduledON){
    unsigned long currentTime = getTime();
    if(currentTime >= scheduledOnTime){
      tone(BUZZER_PIN,256);
      isScheduledON = false;
      mqttClient.publish("EE-21_SwitchON","1");
      mqttClient.publish("EE-21_AlarmON","0");
      Serial.print("Schedule ON");
    }
  }
}

void motorAngle(){
  int sensorL =analogRead(LDR_L_PIN);
  int sensorR =analogRead(LDR_R_PIN);
  String(sensorL).toCharArray(sensorLarr,6);
  String(sensorR).toCharArray(sensorRarr,6);

  double D = (sensorR>sensorL) ? 0.5 : 1.5;
  double min_VAL = min(sensorR,sensorL);
  double I = 1-(min_VAL-32)/4031;
  min_angle = min(180.0,((T_offset*D)+(180-T_offset)*I*gammavalve));
}

