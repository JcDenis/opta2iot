/*
 * opta2iot
 *
 * Arduino Opta Industrial IoT gateway
 *
 * Author: Jean-Christian Paul Denis
 * Source: https://github.com/JcDenis/opta2iot
 *
 * Based on "Remoto" at https://github.com/albydnc/remoto by 
 * Author: Alberto Perro
 * Date: 27-12-2024
 * License: CERN-OHL-P
 *
 * see README.md file
 */

#include "BlockDevice.h"
#include "MBRBlockDevice.h"
#include "FATFileSystem.h"
#include "wiced_resource.h"
//#include "certificates.h"

#include <ArduinoJson.h>
#include <Ethernet.h>
#include <SPI.h>
#include <MQTT.h>
#include <base64.hpp>
#include "KVStore.h"
#include "kvstore_global_api.h"
#include "opta_info.h"

#include "config.h"
#include "html.h"

using namespace opta2iot;

// Loop
unsigned long loopTime = 0;

// Board
void setupBoard();
bool boardSetup = false;
constexpr int OPTA_NONE = 0;
constexpr int OPTA_LITE = 1;
constexpr int OPTA_WIFI = 2;
constexpr int OPTA_RS485 = 3;
int boardSerie = OPTA_NONE;
String boardName = "";
String boardMacEthernet = "";
String boardMacWifi = "";

// Led
void setupLed();
void loopLed();
bool ledOK = false;
bool ledSetup = false;
unsigned long ledLoopState = LOW;
unsigned long ledLoopTime = 0;

// Serial
REDIRECT_STDOUT_TO(Serial);
void setupSerial();
void loopSerial();
bool serialOK = false;
bool serialSetup = false;
const byte serialMaxLen = 50;
char serialMessage[serialMaxLen + 1];
unsigned long serialLoopTime = 0;
unsigned long serialLoopCount = 0;
unsigned long serialLoopRepeat = 0;

// Format
void setupFormat();

// Config
void setupConfig();
void loopConfig();
bool configOK = false;
bool configSetup = false;
config conf;

// Net
void loopNet();
bool netOK = false;
bool netSetup = false;
unsigned long netLastRetry = 0;
unsigned long netLastMaintain = 0;

// Web
void setupWeb();
void loopWeb();
bool webOK = false;
bool webSetup = false;
EthernetServer webServer(80);

// Mqtt
void loopMqtt();
bool mqttOK = false;
bool mqttSetup = false;
MQTTClient mqttClient;
EthernetClient netClient;
long mqttLastPublish = -1;
bool mqttForcePublish = false;
unsigned long mqttLastRetry = 0;

void setup() {
  setupSerial();
  setupBoard();
  setupLed();
  setupFormat();
  setupConfig();
  setupWeb();
}

void loop() {
  loopTime = millis();

  loopSerial();
  loopLed();
  loopConfig();
  loopNet();
  loopWeb();
  loopMqtt();
}

/**
 * Board
 */

void setupBoard() {
  boardSetup = true;
  OptaBoardInfo* info;
  OptaBoardInfo* boardInfo();
  
  info = boardInfo();
  if (info->magic == 0xB5) {
    if (info->_board_functionalities.ethernet == 1) {
      boardSerie = OPTA_LITE;
      boardName = "Opta Lite AFX00003";
      boardMacEthernet = String(info->mac_address[0], HEX) + ":" + String(info->mac_address[1], HEX) + ":" + String(info->mac_address[2], HEX) + ":" + String(info->mac_address[3], HEX) + ":" + String(info->mac_address[4], HEX) + ":" + String(info->mac_address[5], HEX);
    }
    if (info->_board_functionalities.rs485 == 1) {
      boardSerie = OPTA_RS485;
      boardName = "Opta RS485 AFX00001";
    }
    if (info->_board_functionalities.wifi == 1) {
      boardSerie = OPTA_WIFI;
      boardName = "Opta Wifi AFX00002";
      boardMacWifi = String(info->mac_address_2[0], HEX) + ":" + String(info->mac_address_2[1], HEX) + ":" + String(info->mac_address_2[2], HEX) + ":" + String(info->mac_address_2[3], HEX) + ":" + String(info->mac_address_2[4], HEX) + ":" + String(info->mac_address_2[5], HEX);
    }
  }
  deviceInfo();
}

/**
 * Serial
 */

void setupSerial() {
  Serial.begin(115200);
  delay(5000);
  Serial.println("");
  Serial.println("+—————————————————————————————————————+");
  Serial.println("| Arduino Opta Industrial IoT gateway |");
  Serial.println("+—————————————————————————————————————+");
  Serial.println("");
  serialSetup = true;
  serialOK = true;
}

void loopSerial() {
  static byte serialMessageIndex = 0;
  bool serialMessageEnd = false;

  // build serial message
  while (Serial.available() && !serialMessageEnd) {
    int c = Serial.read();
    if (c != -1) {
      switch (c) {
        case '\n':
          serialMessage[serialMessageIndex] = '\0';
          serialMessageIndex = 0;
          serialMessageEnd = true;
          break;
        default:
          if (serialMessageIndex <= serialMaxLen - 1) {
            serialMessage[serialMessageIndex++] = (char)c;
          }
          break;
      }
    }
  }

  // calculate loops time
  if (serialLoopTime > 0) {
    serialLoopCount++;
    if (loopTime - serialLoopTime > 1000) {
      Serial.print(" > ");
      Serial.print(serialLoopCount);
      Serial.println(" loops per second");

      serialLoopTime = serialLoopRepeat < 10 ? loopTime : 0;
      serialLoopCount = 0;
      serialLoopRepeat++;
    }
  }

  // execute serial message
  if (serialMessageEnd) {
    String message = String(serialMessage);
    if (message.equals("IP")) {  // print local Ip to terminal
      Serial.println("* Getting ethernet IP");
      Serial.print(" > ");
      Serial.println(Ethernet.localIP());
    }
    if (message.equals("CONFIG")) {  // print configuration json to terminal
      Serial.println("* Getting user configuration");
      Serial.print(" > ");
      Serial.println(conf.toJson(false));
    }
    if (message.equals("REBOOT")) {
      deviceReboot();
    }
    if (message.equals("INFO")) {
      deviceInfo();
    }
    if (message.equals("FORMAT")) {
      deviceFormat(true);
    }
    if (message.equals("RESET")) {
      deviceReset();
    }
    if (message.equals("DHCP")) {  // update configuration to toggle DHCP mode
      deviceDhcp();
    }
    if (message.equals("LOOP")) {  // print 10 times the number of loops per second to terminal
      Serial.println("* Getting loop time");
      serialLoopTime = loopTime;
      serialLoopCount = 0;
      serialLoopRepeat = 0;
    }
  }
}

/**
 * Led
 */

void setupLed() {
  pinMode(LEDR, OUTPUT); // RED led for ethernet cable
  pinMode(LED_RESET, OUTPUT);  // GREEN led for mqtt and config setup
  ledSetup = true;
  ledOK = true;
}

void loopLed() {
  if (loopTime - ledLoopTime > 750) {  // blink 0.5s
    ledLoopTime = loopTime;
    ledLoopState = ledLoopState == LOW ? HIGH : LOW;

    // blink red = no ethernet, blik green = mqtt ok
    if (!configSetup) {
      digitalWrite(LED_RESET, HIGH);
    } else {
      digitalWrite(LED_RESET, mqttOK && Ethernet.linkStatus() != LinkOFF ? ledLoopState : LOW);
    }
    if (!netSetup) {
      digitalWrite(LEDR, HIGH);
    } else {
      digitalWrite(LEDR, Ethernet.linkStatus() == LinkOFF ? ledLoopState : LOW);
    }
  }
}

/**
 * Format
 */

void setupFormat() {
  deviceFormat(false);
}

/**
 * Config
 */

void setupConfig() {
  pinMode(BTN_USER, INPUT);    // init USER button

  // read config from flash memory
  Serial.println("* Reading configuration from flash");
  char readBuffer[1024];
  kv_get("config", readBuffer, 1024, 0);

  // waiting for user to reset config using USER button
  Serial.print("?> Hold the user button to reset configuration. Waiting");

  for (int i = 0; i < CONFIG_RESET_DELAY; i++) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println();

  // if we have a blank flash or the user button is being held then write default config
  if (conf.loadFromJson(readBuffer, 1024) < 1 || !digitalRead(BTN_USER)) {
    kv_reset("/kv/");
    Serial.println("!> Warning: configuration not found, writing defaults");
    conf.loadDefaults();
    String def = conf.toJson(false);
    kv_set("config", def.c_str(), def.length(), 0);
    Serial.println("* Reading configuration");
    kv_get("config", readBuffer, 1024, 0);
    conf.loadFromJson(readBuffer, 1024);
  } else {
    Serial.println(" > Configuration found");
  }
  
  // configure board IO pins
  Serial.println("* Configuring IO board pins");
  conf.initializePins();

  configSetup = true;
  configOK = true;
}

void loopConfig() {
  // if no ethernet cable plugged and User button push: switch DHCP mode in config and reboot
  if (Ethernet.linkStatus() != LinkON && !digitalRead(BTN_USER)) {
    deviceDhcp();
  }

  // poll input
  const uint32_t now = millis();
  static uint32_t ioLastPoll = 0;
  static String prev_di_mask[NUM_INPUTS];

  if (now - ioLastPoll > IO_POLL_DELAY) {  // Inputs loop delay
    String cur_di_mask[NUM_INPUTS];
    for (size_t i = 0; i < NUM_INPUTS; i++) {
      if (conf.getInputType(i) == ANALOG) {
        float value = analogRead(conf.getInputPin(i)) * (3.249 / ((1 << ADC_BITS) - 1)) / 0.3034;
        char buffer[10];
        int ret = snprintf(buffer, sizeof(buffer), "%0.1f", value);

        cur_di_mask[i] = String(buffer).c_str();
      } else {
        int value = digitalRead(conf.getInputPin(i));
        cur_di_mask[i] = String(value).c_str();
      }

      if (!cur_di_mask[i].equals(prev_di_mask[i])) {
        //ts ... && only 1 for pulse
        if (ioLastPoll > 0 && (conf.getInputType(i) != PULSE || cur_di_mask[i].equals(String('1')))) {
          String inTopic = "I" + String(i + 1);
          String rootTopic = conf.getMqttBase() + conf.getDeviceId() + "/";
          if (mqttOK) {
            mqttClient.publish(String(rootTopic + inTopic + "/val").c_str(), String(cur_di_mask[i]).c_str());
           mqttClient.publish(String(rootTopic + inTopic + "/type").c_str(), String(conf.getInputType(i)).c_str());
          }

          Serial.println(String("[" + inTopic + "] " + prev_di_mask[i] + " => " + cur_di_mask[i]).c_str());
        }
        prev_di_mask[i] = cur_di_mask[i];
      }
    }

    ioLastPoll = now;
  }
}

/**
 * Ethernet
 */

void loopNet() {
  if (netOK && netLastMaintain > 0 && loopTime < netLastMaintain + (NET_RETRY_DELAY*1000)) {
    Ethernet.maintain();
  }
  if (netSetup && !netOK && Ethernet.linkStatus() == LinkON) {
    Serial.println("* Ethernet cable connected");
    netOK = true;
  }
  if (netSetup && netOK && Ethernet.linkStatus() != LinkON) {
    Serial.println("* Ethernet cable disconnected");
    netOK = false;
  }
  if (netSetup && Ethernet.linkStatus() == LinkON) {
    netOK = true;
    return;
  }
  if (netLastRetry > 0 && loopTime < netLastRetry + (NET_RETRY_DELAY*1000)) { // retry every x seconds
    return;
  }

  netLastRetry = loopTime;

  if (Ethernet.hardwareStatus() == EthernetNoHardware) { // should not happened on opta
    Serial.println("!> Ethernet shield not found.");
    netOK = false;
    return;
  }

  if (!netSetup) {
    int ret = 0;

    if (conf.getNetDhcp()) {
      Serial.println("* Configuring Ethernet using DHCP");
      ret = Ethernet.begin(); // If failed this can take 1 minute long...
    } else {
      Serial.println("* Configuring Ethernet using static IP");
      uint8_t ip[4];
      sscanf(conf.getNetIp().c_str(), "%u.%u.%u.%u", &ip[0], &ip[1], &ip[2], &ip[3]);
      IPAddress ipAddr(ip[0], ip[1], ip[2], ip[3]);
      ret = Ethernet.begin(ipAddr); // If failed this can take 1 minute long...
    }

    if (ret == 0) {
      Serial.println("!> Ethernet failed to connect.");

      if (Ethernet.linkStatus() == LinkOFF) {
        Serial.println("!> Ethernet cable not connected.");
        netOK = false;
      }
    } else {
      Serial.print(" > Ethernet connected with IP ");
      Serial.println(Ethernet.localIP());
      netOK = true;
    }

    netSetup = true;
    delay(1000);
  }
}

/**
 * Web
 */

void setupWeb() {
  Serial.println("* Starting web server");
  webServer.begin();
  webSetup = true;
}

void loopWeb() {
  if (!netOK) {
    return;
  }

  // Listen for incoming client requests on Ethernet
  EthernetClient currentClient = webServer.available();
  if (currentClient) { // a client is connected
    webOK = true;

    // prepare to manage resquest
    String currentRequest = "";
    int currentCharcount = 0;
    bool currentAuthentication = false;
    bool currentLineIsBlank = true;
    char currentLinebuffer[80];
    memset(currentLinebuffer, 0, sizeof(currentLinebuffer));

    while (currentClient.connected()) {
      if (currentClient.available()) {
        // Read client request
        char currentChar = currentClient.read();
        currentLinebuffer[currentCharcount] = currentChar;
        if (currentCharcount < sizeof(currentLinebuffer) - 1) {
          currentCharcount++;
        }

        if (currentChar == '\n' && currentLineIsBlank) {
          if (currentAuthentication) {
            if (!currentRequest) {
              // grab end of request
              currentRequest = currentClient.readStringUntil('\r');
            }
            currentClient.flush();

            // find page to serv
            if (currentRequest.startsWith("GET /style.css")) {
              webSendStyle(currentClient);
            } else if (currentRequest.startsWith("POST /form")) {
              webReceiveConfig(currentClient);
            } else if (currentRequest.startsWith("GET /publish ")) {
              webReceivePublish(currentClient);
            } else if (currentRequest.startsWith("GET /config ")) {
              webSendConfig(currentClient);
            } else if (currentRequest.startsWith("GET /data ")) {
              webSendData(currentClient);
            } else if (currentRequest.startsWith("GET /device ")) {
              webSendDevice(currentClient);
            } else if (currentRequest.startsWith("GET / ")) {
              webSendHome(currentClient);
            } else if (currentRequest.startsWith("GET /favicon.ico")) {
              webSendFavicon(currentClient);
            } else {
              webSendError(currentClient);
            }
          } else {
            webSendAuth(currentClient);
          }
          currentClient.stop();
          break;
        }

        if (currentChar == '\n') {
          currentLineIsBlank = true;

          // prepare basic auth
          String stringData = conf.getDeviceUser() + ":" + conf.getDevicePassword();
          char charData[255];
          stringData.toCharArray(charData, sizeof(stringData));
          unsigned char *unsData = (unsigned char *)charData;
          unsigned char base64[21];
          encode_base64(unsData, strlen((char *)unsData), base64);

          // check basic auth
          if (strstr(currentLinebuffer, "Authorization: Basic") > 0 && strstr(currentLinebuffer, (char *)base64) > 0) {
            currentAuthentication = true;
          }
          // if current line buffer is the request
          if (strstr(currentLinebuffer, "GET /") > 0 || strstr(currentLinebuffer, "POST /") > 0) {
            currentRequest = currentLinebuffer;
          }

          memset(currentLinebuffer, 0, sizeof(currentLinebuffer));
          currentCharcount = 0;
        } else if (currentChar != '\r') {
          currentLineIsBlank = false;
        }
      }
    }

    webOK = false;
    //currentClient.stop();
  }
}

void webSendDevice(EthernetClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  client.write(htmlDevice, strlen_P(htmlDevice));
}

void webSendHome(EthernetClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  client.write(htmlHome, strlen_P(htmlHome));
}

void webSendStyle(EthernetClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/css");
  client.println("Connection: close");
  client.println();
  client.write(htmlStyle, strlen_P(htmlStyle));
}

void webSendConfig(EthernetClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  client.println(conf.toJson(true));
}

void webSendData(EthernetClient &client) {
  StaticJsonDocument<512> doc;
  doc["deviceId"] = conf.getDeviceId();
  doc["version"] = VERSION;
  doc["mqttConnected"] = mqttOK;

  // Digital Inputs
  JsonObject inputsObject = doc.createNestedObject("inputs");
  for (int i = 0; i < NUM_INPUTS; i++) {
    String name = "I" + String(i + 1);
    if (conf.getInputType(i) == ANALOG) {
      JsonObject obj = inputsObject.createNestedObject(name);
      obj["value"] = analogRead(conf.getInputPin(i)) * (3.249 / ((1 << ADC_BITS) - 1)) / 0.3034;
      obj["type"] = conf.getInputType(i);
    } else {
      JsonObject obj = inputsObject.createNestedObject(name);
      obj["value"] = digitalRead(conf.getInputPin(i));
      obj["type"] = conf.getInputType(i);
    }
  }
  JsonObject outputsObj = doc.createNestedObject("outputs");
  for (int i = 0; i < NUM_OUTPUTS; i++) {
    String name = "O" + String(i + 1);
    outputsObj[name] = digitalRead(conf.getOutputPin(i));
  }
  String jsonString;
  serializeJson(doc, jsonString);

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  client.println(jsonString);
}

void webSendFavicon(EthernetClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: image/x-icon");
  client.println("Connnection: close");
  client.println();

  const byte bufferSize = 48;
  uint8_t buffer[bufferSize];
  const size_t n = sizeof htmlFavicon / bufferSize;
  const size_t r = sizeof htmlFavicon % bufferSize;
  for (size_t i = 0; i < sizeof htmlFavicon; i += bufferSize) {
    memcpy_P(buffer, htmlFavicon + i, bufferSize);
    client.write(buffer, bufferSize);
  }
  if (r != 0) {
    memcpy_P(buffer, htmlFavicon + n * bufferSize, r);
    client.write(buffer, r);
  }
}

void webSendError(EthernetClient &client) {
  client.println("HTTP/1.1 404 Not Found");
  client.println("Content-Type: text/html");
  client.println("Connnection: close");
  client.println();
  client.println("<!DOCTYPE HTML>");
  client.println("<HTML><HEAD><TITLE>Error</TITLE></HEAD><BODY><H1>404 Not Found.</H1></BODY></HTML>");
}

void webSendAuth(EthernetClient &client) {
  client.println("HTTP/1.1 401 Authorization Required");
  client.println("WWW-Authenticate: Basic realm=\"Secure Area\"");
  client.println("Content-Type: text/html");
  client.println("Connnection: close");
  client.println();
  client.println("<!DOCTYPE HTML>");
  client.println("<HTML><HEAD><TITLE>Error</TITLE></HEAD><BODY><H1>401 Unauthorized.</H1></BODY></HTML>");
}

void webReceiveConfig(EthernetClient &client) {
  bool isValid = true;
  config oldConf = conf;
  String jsonString = "";

  Serial.println("* Parsing received configuration");
  while (client.available()) {
    String line = client.readStringUntil('\n');  // Read line-by-line

    if (line == "\r") {  // Detect the end of headers (an empty line)
      isValid = false;
      break;
    }

    jsonString += line;
  }

  if (!isValid || conf.loadFromJson(jsonString.c_str(), jsonString.length()) < 1) {
    isValid = false;
    Serial.println("!> Failed to load configuration from response");
  } else {
    if (conf.getDeviceId() == "") {  // device ID must be set
      Serial.println("!> Missing device ID");
      isValid = false;
    }
    if (conf.getDeviceUser() == "") {  // device user must be set
      Serial.println("!> Missing device user");
      isValid = false;
    }
    if (conf.getDevicePassword() == "") {  // get old device password if none set
      Serial.println("!> Get old device password");
      conf.setDevicePassword(oldConf.getDevicePassword());
    }
    if (conf.getMqttPassword() == "" && conf.getMqttUser() != "") {  // get old mqtt password if none set
      Serial.println("!> Get old MQTT password");
      conf.setMqttPassword(oldConf.getMqttPassword());
    }
  }

  if (isValid) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println();
    client.println("{\"status\":\"success\",\"message\":\"Configuration updated\"}");
    client.stop();

    Serial.println("* Writing new configuration to flash");
    String newJson = conf.toJson(false);
    //Serial.println(newJson);
    kv_set("config", newJson.c_str(), newJson.length(), 0);

    deviceReboot();
  }

  if (!isValid) {
    client.println("HTTP/1.1 403 FORBIDDEN");
    client.println("Cache-Control: no-cache");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println();
    client.println("{\"status\":\"error\",\"message\":\"Configuration not updated\"}");
  }
}

void webReceivePublish(EthernetClient &client) {
  delay(1000);
  mqttForcePublish = true;
  client.println("HTTP/1.1 200 OK");
  client.println("Cache-Control: no-cache");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  client.println("{\"status\":\"success\",\"message\":\"Informations published\"}");
}

/**
 * Mqtt
 */

void loopMqtt() {
  if (!netOK) {
    return;
  }

  if (!mqttSetup && netOK) {
    Serial.println("* Configuring MQTT on server " + conf.getMqttIp() + ":" + String(conf.getMqttPort()));
    //mqttClient.setTimeout(5000); // does not work
    //mqttClient.setOptions(10, true, 5000); // does not work
    mqttClient.begin(conf.getMqttIp().c_str(), conf.getMqttPort(), netClient);
    mqttClient.onMessage(mqttReceiveInfo);
    mqttSetup = true;
  }
  
  mqttConnect();

  if (!mqttOK) {
    return;
  }

  if (!digitalRead(BTN_USER)) {  // publish to MQTT on button USER push
    mqttForcePublish = true;
    delay(500);
  }

  // if MQTT update interval = 0, (EDIT: NO! we also do not update input on startup)
  if (mqttOK && (conf.getMqttInterval() > 0 && (millis() / 1000) - mqttLastPublish > conf.getMqttInterval() || mqttLastPublish == -1 || mqttForcePublish == true)) {
    // update the client state
    mqttForcePublish = false;
    mqttLastPublish = millis() / 1000;

    mqttPublishDeviceInfo();  // also update device info

    // Inputs
    String rootTopic = conf.getMqttBase() + conf.getDeviceId() + "/";
    for (size_t i = 0; i < NUM_INPUTS; i++) {
      String inTopic = "I" + String(i + 1) + "/";
      if (conf.getInputType(i) == ANALOG) {
        float value = analogRead(conf.getInputPin(i)) * (3.249 / ((1 << ADC_BITS) - 1)) / 0.3034;
        char buffer[10];
        int ret = snprintf(buffer, sizeof(buffer), "%0.2f", value);
        mqttClient.publish(String(rootTopic + inTopic + "val").c_str(), buffer);
        mqttClient.publish(String(rootTopic + inTopic + "type").c_str(), String(conf.getInputType(i)).c_str());
      } else {
        mqttClient.publish(String(rootTopic + inTopic + "val").c_str(), String(digitalRead(conf.getInputPin(i))).c_str());
        mqttClient.publish(String(rootTopic + inTopic + "type").c_str(), String(conf.getInputType(i)).c_str());
      }
    }
    Serial.println("* Publishing inputs informations to MQTT. " + String(mqttLastPublish));
  }

  mqttClient.loop();
}

void mqttConnect() {
  if (!netOK) {
    mqttOK = false;
    return;
  }
  if (mqttClient.connected()) {
    mqttOK = true;
    mqttLastRetry = 0;
    return;
  }

  if (mqttLastRetry > 0 && loopTime < mqttLastRetry + (MQTT_RETRY_DELAY*1000)) { // retry every x seconds
    return;
  }

  mqttOK = false;
  mqttLastRetry = loopTime;
  Serial.println("* Connecting to MQTT broker");
  if (!mqttClient.connect(conf.getDeviceId().c_str(), conf.getMqttUser().c_str(), conf.getMqttPassword().c_str(),true)) {
    Serial.println("!> Failed to connect to MQTT broker");
    return;
  }

  Serial.println(" > MQTT broker found");
  mqttOK = true;

  String topic = conf.getMqttBase() + conf.getDeviceId() + "/device/get"; // command for device information
  mqttClient.subscribe(topic);
  Serial.println(" > Subcribed to " + topic);

  for (size_t i = 0; i < NUM_OUTPUTS; i++) {
    String topic = conf.getMqttBase() + conf.getDeviceId() + "/O" + String(i + 1); // command for outputs
    mqttClient.subscribe(topic);
    Serial.println(" > Subcribed to " + topic);
  }
}

void mqttReceiveInfo(String &topic, String &payload) {
  Serial.println(" > Received " + topic + ": " + payload);

  String match = conf.getMqttBase() + conf.getDeviceId() + "/device/get";
  if (topic == match) {
    mqttPublishDeviceInfo();
  }

  for (size_t i = 0; i < NUM_OUTPUTS; i++) {
    String match = conf.getMqttBase() + conf.getDeviceId() + "/O" + String(i + 1);
    if (topic == match) {
      Serial.println(" > Setting output " + String(i + 1));
      digitalWrite(conf.getOutputPin(i), payload.toInt());
      digitalWrite(conf.getOutputLed(i), payload.toInt());
    }
  }
}

void mqttPublishDeviceInfo() {
  if (!mqttOK) {
    return;
  }

  Serial.println("* Publishing device informations to MQTT");
  String rootTopic = conf.getMqttBase() + conf.getDeviceId() + "/device/";
  mqttClient.publish(String(rootTopic + "type").c_str(), boardName);
  mqttClient.publish(String(rootTopic + "ip").c_str(), Ethernet.localIP().toString().c_str());
  mqttClient.publish(String(rootTopic + "version").c_str(), String(VERSION).c_str());
}

/**
 * Device
 */

void deviceReboot() {
  Serial.println("!> Device reboot");
  for (int i = 0; i < 10; i++) {
    digitalWrite(LEDR, LOW);
    digitalWrite(LED_RESET, HIGH);
    delay(100);
    digitalWrite(LEDR, HIGH);
    digitalWrite(LED_RESET, LOW);
    delay(100);
  }
  NVIC_SystemReset();
}

void deviceInfo() {
  if (!boardSetup) {
    return;
  }

  Serial.println("* Getting board informations");
  if (boardSerie == OPTA_NONE) {
    Serial.println("!> Failed to find secure boot information");
    return;
  }

  Serial.println(" > Board serie: " + boardName);
  Serial.println(" > Board has Ethernet with Mac address " + boardMacEthernet);

  if (boardSerie == OPTA_WIFI) {
    Serial.println(" > Board has Wifi with Mac address " + boardMacWifi);
  }
  if (boardSerie != OPTA_LITE) {
    Serial.println(" > Board has RS485");
  }
}

void deviceFormat(bool force) {
  mbed::BlockDevice* root = mbed::BlockDevice::get_default_instance();
  mbed::MBRBlockDevice wifi_data(root, 1);
  mbed::MBRBlockDevice ota_data(root, 2);
  mbed::MBRBlockDevice kvstore_data(root, 3);
  mbed::MBRBlockDevice user_data(root, 4);
  mbed::FATFileSystem wifi_data_fs("wlan");
  mbed::FATFileSystem ota_data_fs("fs");
  mbed::FATFileSystem user_data_fs("user");

  if (root->init() != mbed::BD_ERROR_OK) {
    Serial.println("!> Error: QSPI init failure");
    return;
  }

  Serial.println("* Checking partitions");

  bool perform = true;
  bool perform_user = true;
  if (!wifi_data_fs.mount(&wifi_data)) {
    Serial.println(" > Wifi partition already exist");
    perform = false;
  }
  if (!ota_data_fs.mount(&ota_data)) {
    Serial.println(" > OTA partition already exist");
    perform = false;
  }
  if (!user_data_fs.mount(&user_data)) {
    Serial.println(" > User partition already exist");
    perform = false;
    perform_user = false;
  }
  if (perform) {
    Serial.println("!> Partition does not exist");
  }
  if (force) {
    Serial.println("* Forcing partitions creation");
    perform = true;
  }

  if (!perform) {
    return;
  }

  Serial.println("* Erasing partitions, please wait...");
  //root->erase(0x0, root->size());
  root->erase(0x0, root->get_erase_size());
  Serial.println(" > Full erase completed");
  
  mbed::MBRBlockDevice::partition(root, 1, 0x0B, 0, 1 * 1024 * 1024);
  mbed::MBRBlockDevice::partition(root, 2, 0x0B, 1 * 1024 * 1024,  6 * 1024 * 1024);
  mbed::MBRBlockDevice::partition(root, 3, 0x0B, 6 * 1024 * 1024,  7 * 1024 * 1024);
  mbed::MBRBlockDevice::partition(root, 4, 0x0B, 7 * 1024 * 1024, 14 * 1024 * 1024);
  // use space from 15.5MB to 16 MB for another fw, memory mapped
  
  Serial.println("* Formatting Wifi partition");
  if (wifi_data_fs.reformat(&wifi_data)) { // not used yet
    Serial.println("!> Error formatting WiFi partition");
    return;
  }
  Serial.println("* Formatting OTA partition");
  if (ota_data_fs.reformat(&ota_data)) {
    Serial.println("!> Error formatting OTA partition");
    return;
  }
  if (perform_user) { // do not erase user partition if it exists !
      user_data_fs.unmount();
      Serial.println("* Formatting USER partition");
      if (user_data_fs.reformat(&user_data)) {
        Serial.println("!> Error formatting user partition");
        return;
      }
  }
  
  Serial.println("* QSPI Flash formatted!");

  deviceReboot();
}

void deviceDhcp() {
  bool mode = conf.getNetDhcp() ? false : true;
  Serial.println("!> " + String(conf.getNetDhcp() ? "Disable" : "Enable") + " DHCP mode.");
  conf.setNetDhcp(mode);
  String json = conf.toJson(false);
  kv_set("config", json.c_str(), json.length(), 0);

  deviceReboot();
}

void deviceReset() {
  Serial.println("!> Resetting configuration to default");
  kv_reset("/kv/");
  conf.loadDefaults();
  String def = conf.toJson(false);
  kv_set("config", def.c_str(), def.length(), 0);

  deviceReboot();
}

/**
 * Various
 */

String boardUsbSpeed(uint8_t flag) {
  switch (flag) {
    case 1:
      return "USB 2.0/Hi-Speed (480 Mbps)";
    case 2:
      return "USB 1.1/Full-Speed (12 Mbps)";
    default:
      return "N/A";
  }
}

String boardClockSource(uint8_t flag) {
  switch (flag) {
    case 0x8:
      return "External oscillator";
    case 0x4:
      return "External crystal";
    case 0x2:
      return "Internal clock";
    default:
      return "N/A";
  }
}