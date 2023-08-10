#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncTCP.h>
#include "LittleFS.h"
#include <Wire.h>
#include <GyverOLED.h>
#include <WebSerial.h>


AsyncWebServer server(80);

GyverOLED<SSD1306_128x32, OLED_NO_BUFFER> oled;

const char* PARAM_SSID = "ssid";
const char* PARAM_PASS = "pass";
const char* PARAM_IP = "ip";
const char* PARAM_GATEWAY = "gateway";
const char* PARAM_SUBNET = "subnet";
const char* ssidPath = "/ssid.txt";
const char* passPath = "/pass.txt";
const char* ipPath = "/ip.txt";
const char* gatewayPath = "/gateway.txt";
const char* subnetPath = "/subnet.txt";
const int PARAM_PORT = 8765;

#define bufferSize 8192

String ssid;
String pass;
String ip;
String gateway;
String subnet;

IPAddress localIP;
IPAddress localGateway;
IPAddress localSubnet;

unsigned long previousMillis = 0;
const long interval = 10000;
boolean restart = false;

WiFiServer TCP_SERVER(PARAM_PORT);
WiFiClient Client;

uint8_t buf1[bufferSize];
uint16_t i1=0;

uint8_t buf2[bufferSize];
uint16_t i2=0;


void recvMsg(uint8_t *data, size_t len){
  WebSerial.println("Received Data...");
  String d = "";
  for(int i=0; i < len; i++){
    d += char(data[i]);
  }
  WebSerial.println(d);
}

void initFS()
{
  if (!LittleFS.begin())
  {
    Serial.println("Error mount LittleFS");
  }
  else
  {
    Serial.println("LittleFS mounted");
  }
}

String readFile(fs::FS &fs, const char* path)
{
  Serial.printf("Reading file: %s\r\n", path);
  File file = fs.open(path, "r");
  if(!file || file.isDirectory())
  {
    Serial.println("- failed to open file for reading");
    return String();
  }

  String fileContent;
  while(file.available())
  {
    fileContent = file.readStringUntil('\n');
    break;
  }
  file.close();
  return fileContent;
}

void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, "w");
  if(!file){
    Serial.println("- failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("- file written");
  } else {
    Serial.println("- frite failed");
  }
  file.close();
}

bool initWiFi() {
  if(ssid=="" || ip==""){
    Serial.println("Undefined SSID or IP address.");
    return false;
  }

  WiFi.mode(WIFI_STA);
  localIP.fromString(ip.c_str());
  localGateway.fromString(gateway.c_str());
  localSubnet.fromString(subnet.c_str());

  if (!WiFi.config(localIP, localGateway, localSubnet)){
    Serial.println("STA Failed to configure");
    return false;
  }
  WiFi.begin(ssid.c_str(), pass.c_str());

  Serial.println("Connecting to WiFi...");
  delay(20000);
  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("Failed to connect.");
    return false;
  }

  Serial.println(WiFi.localIP());
  return true;
}

void setup() {
  oled.init();
  oled.setScale(1);
  oled.home();
  oled.isEnd();
  oled.clear();
  oled.print("Strarting");
  delay(5000);
  oled.clear();

  // Serial port for debugging purposes
  Serial.begin(115200);

  initFS();
  //Load values saved in LittleFS
  ssid = readFile(LittleFS, ssidPath);
  pass = readFile(LittleFS, passPath);
  ip = readFile(LittleFS, ipPath);
  gateway = readFile (LittleFS, gatewayPath);
  subnet = readFile (LittleFS, subnetPath);
  Serial.println(ssid);
  oled.print(ssid);
  Serial.println(pass);
  Serial.println(ip);
  oled.setCursor(0, 1);
  oled.print(ip);
  Serial.println(gateway);
  oled.setCursor(0, 2);
  oled.print(gateway);
  Serial.println(subnet);
  oled.setCursor(0, 3);
  oled.print(subnet);
  if(initWiFi()) {
    WebSerial.begin(&server);
    /* Attach Message Callback */
    WebSerial.msgCallback(recvMsg);
     server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(LittleFS, "/webserial.html", "text/html");
    });
        server.serveStatic("/", LittleFS, "/");
    server.begin();
     }
    else {
    // Connect to Wi-Fi network with SSID and password
    Serial.println("Setting AP (Access Point)");
    oled.clear();
    oled.home();
    oled.println("Setting AP");;
    // NULL sets an open Access Point
    WiFi.softAP("ESP-WIFI-MANAGER", NULL);

    IPAddress IP = WiFi.softAPIP();
    Serial.println("Starting TCP Server");
    server.begin(); // start TCP server
    Serial.print("AP IP address: ");
    Serial.println(IP); 
    oled.clear();
    oled.home();
    oled.println(IP);
    // Web Server Root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(LittleFS, "/wifimanager.html", "text/html");
    });
    
    server.serveStatic("/", LittleFS, "/");
    
    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
      int params = request->params();
      for(int i=0;i<params;i++){
        AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          // HTTP POST ssid value
          if (p->name() == PARAM_SSID) {
            ssid = p->value().c_str();
            Serial.print("SSID set to: ");
            oled.clear();
            oled.setCursor(0, 0);
            oled.print("SSID set to: ");
            Serial.println(ssid);
            oled.setCursor(0, 1);
            oled.print(ssid);
            // Write file to save value
            writeFile(LittleFS, ssidPath, ssid.c_str());
          }
          // HTTP POST pass value
          if (p->name() == PARAM_PASS) {
            pass = p->value().c_str();
            Serial.print("Password set to: ");
            Serial.println(pass);
            oled.setCursor(0, 2);
            oled.print("Password set to: ");
            oled.setCursor(0, 3);
            oled.print(pass);

            // Write file to save value
            writeFile(LittleFS, passPath, pass.c_str());
          }
          // HTTP POST ip value
          if (p->name() == PARAM_IP) {
            ip = p->value().c_str();
            Serial.print("IP Address set to: ");
            Serial.println(ip);
            oled.setCursor(0, 4);
            oled.print(ip);
            // Write file to save value
            writeFile(LittleFS, ipPath, ip.c_str());
          }
          // HTTP POST gateway value
          if (p->name() == PARAM_GATEWAY) {
            gateway = p->value().c_str();
            Serial.print("Gateway set to: ");
            Serial.println(gateway);
            // Write file to save value
            writeFile(LittleFS, gatewayPath, gateway.c_str());
          }
          if (p->name() == PARAM_SUBNET) {
            subnet = p->value().c_str();
            Serial.print("Subnet set to: ");
            Serial.println(subnet);
            // Write file to save value
            writeFile(LittleFS, subnetPath, subnet.c_str());
          }

        }
      }
      restart = true;
      request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: " + ip);
    });
    server.begin();
  }
}

void loop() {
  if (restart){
    delay(5000);
    ESP.restart();
  }
}