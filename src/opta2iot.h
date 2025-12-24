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

#include <ArduinoJson.h>
#include <Ethernet.h>
#include <WiFi.h>
#include <HttpClient.h>
#include <MQTT.h>
#include "opta_info.h"
#include "KVStore.h"
#include "kvstore_global_api.h"
#include "BlockDevice.h"
#include "MBRBlockDevice.h"
#include "FATFileSystem.h"
#include "wiced_resource.h"
#include <base64.hpp>

#include "certificates.h"
#include "config.h"
#include "html.h"

using namespace opta2iot;

/**
 * Commons
 */

REDIRECT_STDOUT_TO(Serial);

unsigned long loopTime = 0;
unsigned long ntpTime = 0;

unsigned int netMode = NET_NONE;
unsigned int optaMode = OPTA_NONE;

bool configOk = false;
bool formatOk = false;
bool infoOk = false;
bool ledOk = false;
bool mqttOk = false;
bool netOk = false;
bool serialOk = false;
bool webOk = false;

config conf;
EthernetServer ethernetServer(80);
WiFiServer wifiServer(80);
Client* webClient = nullptr;  // required to mix wifi and ethernet client
MQTTClient mqttClient;
EthernetClient ethernetClient;  // for MQTT
WiFiClient wifiClient;          // for MQTT
void mqttPublishDevice();
void mqttPublishInputs();

void printDo() {
  Serial.print("* ");
}
void printIn() {
  Serial.print("?> ");
}
void printKo() {
  Serial.print("!> ");
}
void printOk() {
  Serial.print(" > ");
}
void printProgress(uint32_t offset, uint32_t size, uint32_t threshold, bool reset) {
  static int percent_done = 0;
  if (reset == true) {
    percent_done = 0;
    printOk();
    Serial.println(String(percent_done) + "%");
  } else {
    uint32_t percent_done_new = offset * 100 / size;
    if (percent_done_new >= percent_done + threshold) {
      percent_done = percent_done_new;
      printOk();
      Serial.println(String(percent_done) + "%");
    }
  }
}

String getDeviceName() {
  String message;
  switch (optaMode) {
    case OPTA_LITE: message = "Opta Lite AFX00003"; break;
    case OPTA_WIFI: message = "Opta Wifi AFX00002"; break;
    case OPTA_RS485: message = "Opta RS485 AFX00001"; break;
    default: message = "Unknown"; break;
  }
  return message;
}

IPAddress getLocalIp() {
  return netMode == NET_ETH ? Ethernet.localIP() : WiFi.localIP();
}

IPAddress parseIp(const String& ipaddr) {
  unsigned int ip[4];
  sscanf(ipaddr.c_str(), "%u.%u.%u.%u", &ip[0], &ip[1], &ip[2], &ip[3]);
  IPAddress ret(ip[0], ip[1], ip[2], ip[3]);
  return ret;
}

/**
 * Action
 */

unsigned long actionLoopTime = 0;
unsigned int actionLoopCount = 0;
unsigned int actionLoopRepeat = 0;

void loopAction() {
  if (actionLoopTime > 0) {
    actionLoopCount++;
    if (loopTime - actionLoopTime > 1000) {
      printOk();
      Serial.print(actionLoopCount);
      Serial.println(" loops per second");

      actionLoopTime = actionLoopRepeat < 10 ? loopTime : 0;
      actionLoopCount = 0;
      actionLoopRepeat++;
    }
  }
}

// Start simple loops per second
void actionLoop() {
  printDo();
  Serial.println("Getting loop time");
  actionLoopTime = loopTime;
  actionLoopCount = 0;
  actionLoopRepeat = 0;
}

void actionReboot() {
  printKo();
  Serial.println("Device reboot");
  if (ledOk) {
    for (int i = 0; i < 10; i++) {
      digitalWrite(LEDR, LOW);
      digitalWrite(LED_RESET, HIGH);
      delay(100);
      digitalWrite(LEDR, HIGH);
      digitalWrite(LED_RESET, LOW);
      delay(100);
    }
  } else {
    delay(1000);
  }
  NVIC_SystemReset();
}

void actionInfo() {
  printDo();
  Serial.println("Getting board informations");
  if (optaMode == OPTA_NONE) {
    printKo();
    Serial.println("Failed to find board information");
    return;
  }
  printOk();
  Serial.println("Board type: " + getDeviceName());
  printOk();
  Serial.println("Board has Ethernet");

  if (optaMode == OPTA_WIFI) {
    printOk();
    Serial.println("Board has Wifi");
  }
  if (optaMode != OPTA_LITE) {
    printOk();
    Serial.println("Board has RS485");
  }
}

void actionDhcp() {
  if (!configOk) {
    return;
  }

  bool mode = conf.getNetDhcp() ? false : true;
  printKo();
  Serial.print(mode ? "Enabling" : "Disabling");
  Serial.println(" DHCP mode.");

  conf.setNetDhcp(mode);
  String json = conf.toJson(false);
  kv_set("config", json.c_str(), json.length(), 0);
}

void actionWifi() {
  if (!configOk) {
    return;
  }

  bool mode = conf.getNetWifi() ? false : true;
  printKo();
  Serial.print(mode ? "Enabling" : "Disabling");
  Serial.println(" Wifi.");

  conf.setNetWifi(mode);
  String json = conf.toJson(false);
  kv_set("config", json.c_str(), json.length(), 0);
}

void actionReset() {
  printKo();
  Serial.println("Resetting configuration to default");

  kv_reset("/kv/");
  conf.loadDefaults();
  String def = conf.toJson(false);
  kv_set("config", def.c_str(), def.length(), 0);
}


void actionFormat(bool force) {
  const uint32_t file_size = 421098;
  extern const unsigned char wifi_firmware_image_data[];

  mbed::BlockDevice* root = mbed::BlockDevice::get_default_instance();
  mbed::MBRBlockDevice wifi_data(root, 1);
  mbed::MBRBlockDevice ota_data(root, 2);
  mbed::MBRBlockDevice kvstore_data(root, 3);
  mbed::MBRBlockDevice user_data(root, 4);
  mbed::FATFileSystem wifi_data_fs("wlan");
  mbed::FATFileSystem ota_data_fs("fs");
  mbed::FATFileSystem user_data_fs("user");

  if (root->init() != mbed::BD_ERROR_OK) {
    printKo();
    Serial.println("Error: QSPI init failure");
    return;
  }

  printDo();
  Serial.println("Checking partitions");

  bool perform = false;
  bool perform_user = true;
  if (!wifi_data_fs.mount(&wifi_data)) {
    printOk();
    Serial.println("Wifi partition already exist");
  } else {
    perform = true;
  }
  if (!ota_data_fs.mount(&ota_data)) {
    printOk();
    Serial.println("OTA partition already exist");
  } else {
    perform = true;
  }
  if (!user_data_fs.mount(&user_data)) {
    printOk();
    Serial.println("User partition already exist");
    perform_user = false;
  } else {
    perform = true;
  }
  if (perform) {
    printKo();
    Serial.println("Partition does not exist");
  }
  if (force) {
    printDo();
    Serial.println("Forcing partitions creation");
    perform = true;
  }

  if (!perform) {
    return;
  }

  // erase partitions
  printDo();
  if (force) {
    Serial.println("Full erasing partitions, please wait...");
    root->erase(0x0, root->size());
  } else {
    Serial.println("Erasing partitions, please wait...");
    root->erase(0x0, root->get_erase_size());
  }
  printOk();
  Serial.println("Erase completed");

  mbed::MBRBlockDevice::partition(root, 1, 0x0B, 0, 1 * 1024 * 1024);
  mbed::MBRBlockDevice::partition(root, 2, 0x0B, 1 * 1024 * 1024, 6 * 1024 * 1024);
  mbed::MBRBlockDevice::partition(root, 3, 0x0B, 6 * 1024 * 1024, 7 * 1024 * 1024);
  mbed::MBRBlockDevice::partition(root, 4, 0x0B, 7 * 1024 * 1024, 14 * 1024 * 1024);
  // use space from 15.5MB to 16 MB for another fw, memory mapped

  // format wifi partition
  printDo();
  Serial.println("Formatting Wifi partition");
  if (wifi_data_fs.reformat(&wifi_data)) {  // not used yet
    printKo();
    Serial.println("Error formatting WiFi partition");
    return;
  }

  uint32_t chunk_size = 1024;
  uint32_t byte_count = 0;

  // flash WiFi Firmware And Certificates
  FILE* fp = fopen("/wlan/4343WA1.BIN", "wb");
  printDo();
  Serial.println("Flashing WiFi firmware");
  printProgress(byte_count, file_size, 10, true);
  while (byte_count < file_size) {
    if (byte_count + chunk_size > file_size)
      chunk_size = file_size - byte_count;
    int ret = fwrite(&wifi_firmware_image_data[byte_count], chunk_size, 1, fp);
    if (ret != 1) {
      printKo();
      Serial.println("Error writing firmware data");
      break;
    }
    byte_count += chunk_size;
    printProgress(byte_count, file_size, 10, false);
  }
  fclose(fp);

  fp = fopen("/wlan/cacert.pem", "wb");

  printDo();
  Serial.println("Flashing certificates");
  chunk_size = 128;
  byte_count = 0;
  printProgress(byte_count, cacert_pem_len, 10, true);
  while (byte_count < cacert_pem_len) {
    if (byte_count + chunk_size > cacert_pem_len)
      chunk_size = cacert_pem_len - byte_count;
    int ret = fwrite(&cacert_pem[byte_count], chunk_size, 1, fp);
    if (ret != 1) {
      printKo();
      Serial.println("Error writing certificates");
      break;
    }
    byte_count += chunk_size;
    printProgress(byte_count, cacert_pem_len, 10, false);
  }
  fclose(fp);

  // Flash WiFi Firmware Mapped
  chunk_size = 1024;
  byte_count = 0;
  const uint32_t offset = 15 * 1024 * 1024 + 1024 * 512;

  printDo();
  Serial.println("Flashing memory mapped WiFi firmware");
  printProgress(byte_count, file_size, 10, true);
  while (byte_count < file_size) {
    if (byte_count + chunk_size > file_size)
      chunk_size = file_size - byte_count;
    int ret = root->program(wifi_firmware_image_data, offset + byte_count, chunk_size);
    if (ret != 0) {
      printKo();
      Serial.println("Error writing memory mapped firmware");
      break;
    }
    byte_count += chunk_size;
    printProgress(byte_count, file_size, 10, false);
  }

  // format OTA partition
  printDo();
  Serial.println("Formatting OTA partition");
  if (ota_data_fs.reformat(&ota_data)) {
    printKo();
    Serial.println("Error formatting OTA partition");
    return;
  }

  // format user partition
  if (perform_user) {  // do not erase user partition if it exists !
    user_data_fs.unmount();
    printDo();
    Serial.println("Formatting USER partition");
    if (user_data_fs.reformat(&user_data)) {
      printKo();
      Serial.println("Error formatting user partition");
      return;
    }
  }

  printDo();
  Serial.println("* QSPI Flash formatted");

  formatOk = true;
}

/**
 * Serial
 */

char serialMessage[50];  // max len to 50

void setupSerial() {
  Serial.begin(115200);
  delay(5000);
  Serial.println("");
  Serial.println("+—————————————————————————————————————+");
  Serial.println("| Arduino Opta Industrial IoT gateway |");
  Serial.println("+—————————————————————————————————————+");
  Serial.println("");
  serialOk = true;
}

void loopSerial() {
  static byte index = 0;
  bool end = false;

  // grab serial message
  while (Serial.available() && !end) {
    int c = Serial.read();
    if (c != -1) {
      switch (c) {
        case '\n':
          serialMessage[index] = '\0';
          index = 0;
          end = true;
          break;
        default:
          if (index <= 49) {  // max len to 50 @see serialMessage
            serialMessage[index++] = (char)c;
          }
          break;
      }
    }
  }

  // execute serial message
  if (end) {
    String message = String(serialMessage);
    message.toLowerCase();
    printDo();
    Serial.println("Receiving command '" + message + "'");
    if (message.equals("ip")) {  // print local Ip to terminal
      printDo();
      Serial.println("Getting local IP address");
      printOk();
      Serial.println(getLocalIp());
    }
    if (message.equals("config")) {  // print configuration json to terminal
      printDo();
      Serial.println("Getting user configuration");
      printOk();
      Serial.println(conf.toJson(false));
    }
    if (message.equals("info")) {  // print board info to terminal
      actionInfo();
    }
    if (message.equals("publish")) {  // publish to mqtt device info and inputs state
      mqttPublishDevice();
      mqttPublishInputs();
    }
    if (message.equals("reboot")) {  // reboot device
      actionReboot();
    }
    if (message.equals("format")) {  // format ALL partition
      actionFormat(true);
      if (formatOk) {
        actionReboot();
      }
    }
    if (message.equals("reset")) {  // reset configuration
      actionReset();
      actionReboot();
    }
    if (message.equals("dhcp")) {  // update configuration to toggle DHCP mode
      actionDhcp();
      actionReboot();
    }
    if (message.equals("wifi")) {  // update configuration to toggle WIFI mode
      actionWifi();
      actionReboot();
    }
    if (message.equals("loop")) {  // print 10 times the number of loops per second to terminal
      actionLoop();
    }
  }
}

/**
 * Info
 */

void setupInfo() {
  OptaBoardInfo* info;
  OptaBoardInfo* boardInfo();

  info = boardInfo();
  if (info->magic == 0xB5) {
    if (info->_board_functionalities.ethernet == 1) {
      optaMode = OPTA_LITE;
      //boardMacEthernet = String(info->mac_address[0], HEX) + ":" + String(info->mac_address[1], HEX) + ":" + String(info->mac_address[2], HEX) + ":" + String(info->mac_address[3], HEX) + ":" + String(info->mac_address[4], HEX) + ":" + String(info->mac_address[5], HEX);
    }
    if (info->_board_functionalities.rs485 == 1) {
      optaMode = OPTA_RS485;
    }
    if (info->_board_functionalities.wifi == 1) {
      optaMode = OPTA_WIFI;
      //boardMacWifi = String(info->mac_address_2[0], HEX) + ":" + String(info->mac_address_2[1], HEX) + ":" + String(info->mac_address_2[2], HEX) + ":" + String(info->mac_address_2[3], HEX) + ":" + String(info->mac_address_2[4], HEX) + ":" + String(info->mac_address_2[5], HEX);
    }
  }
  infoOk = true;
  actionInfo();
}

/**
 * Led
 */

void ledFreeze(bool on) {
  // Red and Green to on, Blue to off
  static unsigned int ledRed = LOW;
  static unsigned int ledGreen = LOW;
  static unsigned int ledBlue = LOW;

  if (!ledOk) {
    return;
  }

  if (on) {
    ledRed = digitalRead(LEDR);
    ledGreen = digitalRead(LED_RESET);
    if (optaMode == OPTA_WIFI) {
      ledBlue = digitalRead(LED_USER);
    }

    digitalWrite(LEDR, HIGH);
    digitalWrite(LED_RESET, HIGH);
    if (optaMode == OPTA_WIFI) {
      digitalWrite(LED_USER, LOW);
    }
  } else {
    digitalWrite(LEDR, ledRed);
    digitalWrite(LED_RESET, ledGreen);
    if (optaMode == OPTA_WIFI) {
      digitalWrite(LED_USER, ledBlue);
    }
  }
}

void ledHeartBeat() {
  // short speed blink Red and Green
  static unsigned int ledRed = LOW;
  static unsigned int ledGreen = LOW;
  static unsigned long ledTime = 0;

  if (!ledOk) {
    return;
  }

  if (loopTime - ledTime > 5125) {
    digitalWrite(LEDR, ledRed);
    digitalWrite(LED_RESET, ledGreen);
    ledTime = loopTime;
    return;
  }
  if (loopTime - ledTime > 5100) {
    digitalWrite(LED_RESET, LOW);
    return;
  }
  if (loopTime - ledTime > 5075) {
    digitalWrite(LEDR, LOW);
    return;
  }
  if (loopTime - ledTime > 5050) {
    digitalWrite(LED_RESET, HIGH);
    return;
  }
  if (loopTime - ledTime > 5025) {
    digitalWrite(LEDR, HIGH);
    return;
  }
  if (loopTime - ledTime > 5000) {
    ledRed = digitalRead(LEDR);
    ledGreen = digitalRead(LED_RESET);
    digitalWrite(LEDR, LOW);
    digitalWrite(LED_RESET, LOW);
    return;
  }
}

void setupLed() {
  pinMode(LEDR, OUTPUT);       // RED led for network
  pinMode(LED_RESET, OUTPUT);  // GREEN led for mqtt and config setup
  pinMode(LED_USER, OUTPUT);   // BLUE led for Wifi
  ledOk = true;
}

void loopLed() {
  static unsigned long ledState = LOW;
  static unsigned long ledTime = 0;

  if (loopTime - ledTime > 750) {
    ledTime = loopTime;
    ledState = ledState == LOW ? HIGH : LOW;

    // slow blink red = network ko,
    // slow blink green = mqtt ok
    // slow blink blue = wifi STA mode
    // fix blue = wifi AP mode
    if (netOk && mqttOk) {
      digitalWrite(LED_RESET, ledState);
    } else {
      digitalWrite(LED_RESET, LOW);
    }
    if (!netOk) {
      digitalWrite(LEDR, ledState);
    } else {
      digitalWrite(LEDR, LOW);
    }
    if (netMode == NET_AP) {
      digitalWrite(LED_USER, ledState);
    } else if (netMode == NET_STA) {
      digitalWrite(LED_USER, HIGH);
    }
  }

  ledHeartBeat();
}

/**
 * Format
 */

void setupFormat() {
  actionFormat(false);
  if (formatOk) {
    actionReboot();
  }
}

/**
 * Config
 */

void setupConfig() {
  pinMode(BTN_USER, INPUT);  // init USER button

  // read flash
  printOk();
  Serial.println("Reading configuration from flash");
  char readBuffer[1024];
  kv_get("config", readBuffer, 1024, 0);

  // not found
  bool force = false;
  bool perform = false;
  if (conf.loadFromJson(readBuffer, 1024) < 1) {
    printKo();
    Serial.println("Configuration not found");
    perform = true;
  }

  // wait user
  if (!perform) {

    ledFreeze(true);
    printIn();
    Serial.print("Hold the user button to fully reset device. Waiting ");
    for (int i = CONFIG_RESET_DELAY; i > 0; i--) {
      Serial.print(i);
      delay(500);
      Serial.print(".");
      delay(500);
    }
    Serial.println();

    if (!digitalRead(BTN_USER)) {
      printIn();
      Serial.print("Hold the user button to confirm fully reset device. Waiting ");
      for (int i = CONFIG_RESET_DELAY; i > 0; i--) {
        Serial.print(i);
        delay(500);
        Serial.print(".");
        delay(500);
      }
      Serial.println();

      if (!digitalRead(BTN_USER)) {
        printKo();
        Serial.println("Reset from button");
        perform = true;
        force = true;
      }
    }
    ledFreeze(false);
  }

  // reset config
  if (perform) {
    actionReset();

    printDo();
    Serial.println("Reading configuration");
    kv_get("config", readBuffer, 1024, 0);
    conf.loadFromJson(readBuffer, 1024);

    if (force) {
      actionFormat(true);
      if (formatOk) {
        actionReboot();
      }
    }
  } else {
    printOk();
    Serial.println("Configuration found");
  }

  // configure board IO pins
  printDo();
  Serial.println("Configuring IO board pins");
  conf.initializePins();

  configOk = true;
}

void loopConfig() {
  // if no ethernet cable plugged and User button push: switch DHCP mode in config and reboot
  if (!netOk && !digitalRead(BTN_USER)) {
    actionDhcp();
    actionReboot();
  }

  // poll input
  static unsigned int pinsLastPoll = 0;
  static String prev_di_mask[NUM_INPUTS];

  if (loopTime - pinsLastPoll > PINS_POLL_DELAY) {  // Inputs loop delay
    String cur_di_mask[NUM_INPUTS];
    for (size_t i = 0; i < NUM_INPUTS; i++) {
      if (conf.getInputType(i) == INPUT_ANALOG) {
        float value = analogRead(conf.getInputPin(i)) * (3.249 / ((1 << ADC_BITS) - 1)) / 0.3034;
        char buffer[10];
        snprintf(buffer, sizeof(buffer), "%0.1f", value);

        cur_di_mask[i] = String(buffer).c_str();
      } else {
        int value = digitalRead(conf.getInputPin(i));
        cur_di_mask[i] = String(value).c_str();
      }

      if (!cur_di_mask[i].equals(prev_di_mask[i])) {
        //ts ... && only 1 for pulse
        if (pinsLastPoll > 0 && (conf.getInputType(i) != INPUT_PULSE || cur_di_mask[i].equals(String('1')))) {
          String inTopic = "I" + String(i + 1);
          String rootTopic = conf.getMqttBase() + conf.getDeviceId() + "/";
          if (mqttOk) {
            mqttClient.publish(String(rootTopic + inTopic + "/val").c_str(), String(cur_di_mask[i]).c_str());
            mqttClient.publish(String(rootTopic + inTopic + "/type").c_str(), String(conf.getInputType(i)).c_str());
          }

          printOk();
          Serial.println(String("[" + inTopic + "] " + prev_di_mask[i] + " => " + cur_di_mask[i]).c_str());
        }
        prev_di_mask[i] = cur_di_mask[i];
      }
    }

    pinsLastPoll = loopTime;
  }
}

/**
 * Network
 */

void netEthernetConnect() {
  int ret = 0;

  ledFreeze(true);
  if (conf.getNetDhcp()) {
    printDo();
    Serial.println("Configuring Ethernet using DHCP");
    ret = Ethernet.begin();  // If failed this can take 1 minute long...
  } else {
    printDo();
    Serial.println("Configuring Ethernet using static IP");
    ret = Ethernet.begin(parseIp(conf.getNetIp()));  // If failed this can take 1 minute long...
  }
  ledFreeze(false);

  if (ret == 0) {
    printKo();
    Serial.println("Network connection failed.");
    if (Ethernet.linkStatus() == LinkOFF) {
      printKo();
      Serial.println("Ethernet cable not connected.");
    }
    netOk = false;
  } else {
    printOk();
    Serial.print("Network connected with IP ");
    Serial.println(Ethernet.localIP());
    netOk = true;
  }
}

void netWifiStaConnect() {
  String netApSsid = conf.getNetSsid();
  String netApPass = conf.getNetPassword();
  char ssid[32];
  char pass[32];
  netApSsid.toCharArray(ssid, sizeof(ssid));
  netApPass.toCharArray(pass, sizeof(pass));

  printDo();
  Serial.print("Configuring Wifi with SSID '" + netApSsid + "' and password '" + netApPass + "'");
  if (conf.getNetDhcp()) {
    Serial.println(" and using DHCP");
  } else {
    Serial.print(" and using static IP");
    WiFi.config(parseIp(conf.getNetIp()));
  }

  ledFreeze(true);
  int ret = WiFi.begin(ssid, pass);
  ledFreeze(false);

  if (ret != WL_CONNECTED) {
    printKo();
    Serial.println("Failed to connect Wifi");

    netOk = false;
  } else {
    printOk();
    Serial.println("Wifi connected");
    netOk = true;
  }
}

void setupNetEthernet() {
  printDo();
  Serial.println("Configuring Ethernet network");
  netMode = NET_ETH;

  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    printKo();
    Serial.println("Communication with Ethernet module failed");
    return;
  }

  netEthernetConnect();
}

void setupNetWifiSta() {
  printDo();
  Serial.println("Configuring Wifi standard network");
  netMode = NET_STA;

  if (WiFi.status() == WL_NO_MODULE) {
    printKo();
    Serial.println("Communication with WiFi module failed");
    return;
  }

  netWifiStaConnect();
}

void setupNetWifiAp() {
  printDo();
  Serial.println("Configuring Wifi Access Point network");
  netMode = NET_AP;

  if (WiFi.status() == WL_NO_MODULE) {
    printKo();
    Serial.println("Communication with WiFi module failed");
    return;
  }

  String netApSsid = "opta2iot" + conf.getDeviceId();
  String netApPass = "opta2iot";
  char ssid[32];
  char pass[32];
  netApSsid.toCharArray(ssid, sizeof(ssid));
  netApPass.toCharArray(pass, sizeof(pass));

  printDo();
  Serial.println("Configuraing Wifi using SSID '" + netApSsid + "' and password '" + netApPass + "' and IP " + conf.getNetIp());

  WiFi.config(parseIp(conf.getNetIp()));

  ledFreeze(true);
  int ret = WiFi.beginAP(ssid, pass);
  ledFreeze(false);

  if (ret != WL_AP_LISTENING) {
    printKo();
    Serial.println("Failed to create Wifi Access Point");

    netOk = false;
  } else {
    printOk();
    Serial.println("Wifi access point listening");
    netOk = true;
  }
}

void setupNet() {
  printDo();
  Serial.println("Selecting network mode");

  if (ledOk) {
    digitalWrite(LEDR, HIGH);
  }

  if (optaMode == OPTA_NONE) {
    printKo();
    Serial.println("Failed to find device type");
    return;
  }

  if (!conf.getNetWifi()) {
    setupNetEthernet();
  } else if (optaMode == OPTA_WIFI && conf.getNetWifi() && conf.getNetSsid() != "" && conf.getNetPassword() != "") {
    setupNetWifiSta();
  } else if (optaMode == OPTA_WIFI && conf.getNetWifi()) {
    setupNetWifiAp();
  } else {
    setupNetEthernet();
  }

  if (ledOk) {
    digitalWrite(LEDR, LOW);
  }

  delay(1000);

  if (netOk && configOk && conf.getNetDhcp()) {
    printOk();
    Serial.print("DHCP attributed IP is ");
    Serial.println(getLocalIp());
  }
}

void loopNetEthernet() {
  static unsigned long netLastRetry = 0;
  // mauvaise condition
  if (netLastRetry > 0 && loopTime < netLastRetry + (NET_RETRY_DELAY * 1000)) {
    if (netOk) {
      netLastRetry = loopTime;
      Ethernet.maintain();
    } else {
      netEthernetConnect();
    }
  }
  if (!netOk && Ethernet.linkStatus() == LinkON) {
    printOk();
    Serial.println("Ethernet cable connected");
    netOk = true;
  }
  if (netOk && Ethernet.linkStatus() != LinkON) {
    printOk();
    Serial.println("Ethernet cable disconnected");
    netOk = false;
  }
  if (netOk && Ethernet.linkStatus() == LinkON) {
    netLastRetry = 0;
    netOk = true;
  }
}

void loopNetWifiSta() {
  static unsigned long netLastRetry = 0;
  if (netOk || (netLastRetry > 0 && loopTime > netLastRetry + (NET_RETRY_DELAY * 1000))) {
    return;
  }

  netLastRetry = loopTime;
  netWifiStaConnect();
}

void loopNetWifiAp() {
  static unsigned int firstLoop = 1;
  static int status = WL_IDLE_STATUS;
  if (status != WiFi.status()) {
    status = WiFi.status();

    if (status == WL_AP_CONNECTED) {
      printOk();
      Serial.println("Device connected to AP");
    } else if (firstLoop == 1) {  // do not display message on startup
      firstLoop = 0;
      printOk();
      Serial.println("Device disconnected from AP");
    }
  }
}

void loopNet() {
  switch (netMode) {
    case NET_ETH: loopNetEthernet(); break;
    case NET_STA: loopNetWifiSta(); break;
    case NET_AP: loopNetWifiAp(); break;
    default: break;
  }
}

/**
 * Web Server
 */

void setupWeb() {
  if (!configOk || netMode == NET_NONE) {
    return;
  }

  ledFreeze(true);
  printDo();
  if (netMode == NET_ETH) {
    Serial.println("Configuring Ethernet Web server");
    ethernetServer.begin();
  } else {
    Serial.println("Configuring Wifi Web server");
    wifiServer.begin();
  }
  ledFreeze(false);

  webOk = true;
}

void webSendDevice(Client*& client) {
  client->println("HTTP/1.1 200 OK");
  client->println("Content-Type: text/html");
  client->println("Connection: close");
  client->println();
  client->println(htmlDevice);
}

void webSendHome(Client*& client) {
  client->println("HTTP/1.1 200 OK");
  client->println("Content-Type: text/html");
  client->println("Connection: close");
  client->println();
  client->println(htmlHome);
}

void webSendStyle(Client*& client) {
  client->println("HTTP/1.1 200 OK");
  client->println("Content-Type: text/css");
  client->println("Connection: close");
  client->println();
  client->println(htmlStyle);
}

void webSendConfig(Client*& client) {
  client->println("HTTP/1.1 200 OK");
  client->println("Content-Type: application/json");
  client->println("Connection: close");
  client->println();
  client->println(conf.toJson(true));
}

void webSendData(Client*& client) {
  StaticJsonDocument<512> doc;
  doc["deviceId"] = conf.getDeviceId();
  doc["version"] = SKETCH_VERSION;
  doc["mqttConnected"] = mqttOk;

  // Digital Inputs
  JsonObject inputsObject = doc.createNestedObject("inputs");
  for (int i = 0; i < NUM_INPUTS; i++) {
    String name = "I" + String(i + 1);
    if (conf.getInputType(i) == INPUT_ANALOG) {
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

  client->println("HTTP/1.1 200 OK");
  client->println("Content-Type: application/json");
  client->println("Connection: close");
  client->println();
  client->println(jsonString);
}

void webSendFavicon(Client*& client) {
  client->println("HTTP/1.1 200 OK");
  client->println("Content-Type: image/x-icon");
  client->println("Connnection: close");
  client->println();

  const byte bufferSize = 48;
  uint8_t buffer[bufferSize];
  const size_t n = sizeof htmlFavicon / bufferSize;
  const size_t r = sizeof htmlFavicon % bufferSize;
  for (size_t i = 0; i < sizeof htmlFavicon; i += bufferSize) {
    memcpy_P(buffer, htmlFavicon + i, bufferSize);
    client->write(buffer, bufferSize);
  }
  if (r != 0) {
    memcpy_P(buffer, htmlFavicon + n * bufferSize, r);
    client->write(buffer, r);
  }
}

void webSendAuth(Client*& client) {
  client->println("HTTP/1.1 401 Authorization Required");
  client->println("WWW-Authenticate: Basic realm=\"Secure Area\"");
  client->println("Content-Type: text/html");
  client->println("Connnection: close");
  client->println();
  client->println("<!DOCTYPE HTML>");
  client->println("<HTML><HEAD><TITLE>Error</TITLE></HEAD><BODY><H1>401 Unauthorized.</H1></BODY></HTML>");
}

void webSendError(Client*& client) {
  client->println("HTTP/1.1 404 Not Found");
  client->println("Content-Type: text/html");
  client->println("Connnection: close");
  client->println();
  client->println("<!DOCTYPE HTML>");
  client->println("<HTML><HEAD><TITLE>Error</TITLE></HEAD><BODY><H1>404 Not Found.</H1></BODY></HTML>");
}

void webReceiveConfig(Client*& client) {
  bool isValid = true;
  config oldConf = conf;
  String jsonString = "";

  printDo();
  Serial.println("Parsing received configuration");
  while (client->available()) {
    String line = client->readStringUntil('\n');  // Read line-by-line

    if (line == "\r") {  // Detect the end of headers (an empty line)
      isValid = false;
      break;
    }

    jsonString += line;
  }

  if (!isValid || conf.loadFromJson(jsonString.c_str(), jsonString.length()) < 1) {
    isValid = false;
    printKo();
    Serial.println("Failed to load configuration from response");
  } else {
    if (conf.getDeviceId() == "") {  // device ID must be set
      isValid = false;
      printKo();
      Serial.println("Missing device ID");
    }
    if (conf.getDeviceUser() == "") {  // device user must be set
      isValid = false;
      printKo();
      Serial.println("Missing device user");
    }
    if (conf.getDevicePassword() == "") {  // get old device password if none set
      printOk();
      Serial.println("Get previous device password");
      conf.setDevicePassword(oldConf.getDevicePassword());
    }
    if (conf.getNetPassword() == "" && conf.getNetSsid() != "") {  // get old wifi password if none set
      printOk();
      Serial.println("Get previous Wifi password");
      conf.setNetPassword(oldConf.getNetPassword());
    }
    if (conf.getMqttPassword() == "" && conf.getMqttUser() != "") {  // get old mqtt password if none set
      printOk();
      Serial.println("Get previous MQTT password");
      conf.setMqttPassword(oldConf.getMqttPassword());
    }
  }

  if (isValid) {
    client->println("HTTP/1.1 200 OK");
    client->println("Content-Type: application/json");
    client->println("Connection: close");
    client->println();
    client->println("{\"status\":\"success\",\"message\":\"Configuration updated\"}");
    client->stop();

    printDo();
    Serial.println("Writing new configuration to flash");
    String newJson = conf.toJson(false);
    kv_set("config", newJson.c_str(), newJson.length(), 0);

    actionReboot();
  } else {
    client->println("HTTP/1.1 403 FORBIDDEN");
    client->println("Cache-Control: no-cache");
    client->println("Content-Type: application/json");
    client->println("Connection: close");
    client->println();
    client->println("{\"status\":\"error\",\"message\":\"Configuration not updated\"}");
  }
}

void webReceivePublish(Client*& client) {
  delay(1000);
  mqttPublishDevice();
  mqttPublishInputs();

  client->println("HTTP/1.1 200 OK");
  client->println("Cache-Control: no-cache");
  client->println("Content-Type: application/json");
  client->println("Connection: close");
  client->println();
  client->println("{\"status\":\"success\",\"message\":\"Informations published\"}");
}

void loopWeb() {
  if (netMode == NET_NONE || !netOk || !webOk) {
    return;
  }

  // we need to have both to not crash server
  EthernetClient ethernetClient = ethernetServer.accept();
  WiFiClient wifiClient = wifiServer.accept();
  if (netMode == NET_ETH) {
    webClient = &ethernetClient;
  } else {
    webClient = &wifiClient;
  }

  if (webClient) {  // a client is connected
    // prepare to manage resquest
    String webRequest = "";
    int webCharcount = 0;
    bool webAuthentication = false;
    bool webLineIsBlank = true;
    char webLinebuffer[80];
    memset(webLinebuffer, 0, sizeof(webLinebuffer));

    while (webClient->connected()) {
      if (webClient->available()) {
        // Read client request
        char webChar = webClient->read();
        webLinebuffer[webCharcount] = webChar;
        if (webCharcount < (int)sizeof(webLinebuffer) - 1) {
          webCharcount++;
        }

        if (webChar == '\n' && webLineIsBlank) {
          if (webAuthentication) {
            if (!webRequest) {
              // grab end of request
              webRequest = webClient->readStringUntil('\r');
            }
            webClient->flush();

            if (webRequest.startsWith("GET /style.css")) {
              webSendStyle(webClient);
            } else if (webRequest.startsWith("POST /form")) {
              webReceiveConfig(webClient);
            } else if (webRequest.startsWith("GET /publish ")) {
              webReceivePublish(webClient);
            } else if (webRequest.startsWith("GET /config ")) {
              webSendConfig(webClient);
            } else if (webRequest.startsWith("GET /data ")) {
              webSendData(webClient);
            } else if (webRequest.startsWith("GET /device ")) {
              webSendDevice(webClient);
            } else if (webRequest.startsWith("GET / ")) {
              webSendHome(webClient);
            } else if (webRequest.startsWith("GET /favicon.ico")) {
              webSendFavicon(webClient);
            } else {
              webSendError(webClient);
            }
          } else {
            webSendAuth(webClient);
          }
          webClient->stop();
          break;
        }

        if (webChar == '\n') {
          webLineIsBlank = true;

          // prepare basic auth ! I s*** at this
          String inputString = conf.getDeviceUser() + ":" + conf.getDevicePassword();
          char inputChar[128];
          inputString.toCharArray(inputChar, sizeof(inputChar));
          unsigned char* unsInputChar = (unsigned char*)inputChar;
          int inputLength = strlen((char*)unsInputChar);
          int encodedLength = encode_base64_length(inputLength) + 1;
          unsigned char encodedString[encodedLength];
          encode_base64(unsInputChar, inputLength, encodedString);

          // check basic auth
          if (strstr(webLinebuffer, "Authorization: Basic") && strstr(webLinebuffer, (char*)encodedString)) {
            webAuthentication = true;
          }
          // if web line buffer is the request
          if (strstr(webLinebuffer, "GET /") || strstr(webLinebuffer, "POST /")) {
            webRequest = webLinebuffer;
          }

          memset(webLinebuffer, 0, sizeof(webLinebuffer));
          webCharcount = 0;
        } else if (webChar != '\r') {
          webLineIsBlank = false;
        }
      }
    }
  }
}

/**
 * MQTT
 */

void mqttReceive(String& topic, String& payload) {
  printOk();
  Serial.println("Received " + topic + ": " + payload);

  String match = conf.getMqttBase() + conf.getDeviceId() + "/device/get";
  if (topic == match) {
    mqttPublishDevice();
  }

  for (size_t i = 0; i < NUM_OUTPUTS; i++) {
    String match = conf.getMqttBase() + conf.getDeviceId() + "/O" + String(i + 1);
    if (topic == match) {
      printOk();
      Serial.print("Setting output ");
      Serial.print((i + 1));
      Serial.print(" to ");
      Serial.println(payload.toInt());

      digitalWrite(conf.getOutputPin(i), payload.toInt());
      digitalWrite(conf.getOutputLed(i), payload.toInt());
    }
  }
}

void mqttPublishInputs() {
  if (!mqttOk) {
    return;
  }

  printDo();
  Serial.println("Publishing inputs informations to MQTT");

  String rootTopic = conf.getMqttBase() + conf.getDeviceId() + "/";
  for (size_t i = 0; i < NUM_INPUTS; i++) {
    String inTopic = "I" + String(i + 1) + "/";
    if (conf.getInputType(i) == INPUT_ANALOG) {
      float value = analogRead(conf.getInputPin(i)) * (3.249 / ((1 << ADC_BITS) - 1)) / 0.3034;
      char buffer[10];
      snprintf(buffer, sizeof(buffer), "%0.2f", value);
      mqttClient.publish(String(rootTopic + inTopic + "val").c_str(), buffer);
      mqttClient.publish(String(rootTopic + inTopic + "type").c_str(), String(conf.getInputType(i)).c_str());
    } else {
      mqttClient.publish(String(rootTopic + inTopic + "val").c_str(), String(digitalRead(conf.getInputPin(i))).c_str());
      mqttClient.publish(String(rootTopic + inTopic + "type").c_str(), String(conf.getInputType(i)).c_str());
    }
  }
}

void mqttPublishDevice() {
  if (!mqttOk) {
    return;
  }

  printDo();
  Serial.println("Publishing device informations to MQTT");

  String rootTopic = conf.getMqttBase() + conf.getDeviceId() + "/device/";
  mqttClient.publish(String(rootTopic + "type").c_str(), getDeviceName());
  mqttClient.publish(String(rootTopic + "ip").c_str(), String(getLocalIp()).c_str());
  mqttClient.publish(String(rootTopic + "version").c_str(), String(SKETCH_VERSION).c_str());
}

void mqttConnect() {
  static unsigned long mqttLastRetry = 0;
  if (!netOk) {
    mqttOk = false;
    return;
  }
  if (mqttClient.connected()) {
    mqttOk = true;
    mqttLastRetry = 0;
    return;
  }

  if (mqttLastRetry > 0 && loopTime < mqttLastRetry + (MQTT_RETRY_DELAY * 1000)) {  // retry every x seconds
    return;
  }

  mqttOk = false;
  mqttLastRetry = loopTime;
  printDo();
  Serial.println("Connecting to MQTT broker");

  String clientIdStr = conf.getDeviceId();
  String usernameStr = conf.getMqttUser();
  String passwordStr = conf.getMqttPassword();
  char clientId[32];
  char username[32];
  char password[32];
  clientIdStr.toCharArray(clientId, sizeof(clientId));
  usernameStr.toCharArray(username, sizeof(username));
  passwordStr.toCharArray(password, sizeof(password));

  ledFreeze(true);
  if (!mqttClient.connect(clientId, username, password, false)) {
    printKo();
    Serial.println("Failed to connect to MQTT broker");
    ledFreeze(false);
    return;
  }
  ledFreeze(false);

  printOk();
  Serial.println("MQTT broker found");
  mqttOk = true;

  String topic = conf.getMqttBase() + conf.getDeviceId() + "/device/get";  // command for device information
  mqttClient.subscribe(topic);
  printOk();
  Serial.println("Subcribed to " + topic);

  for (size_t i = 0; i < NUM_OUTPUTS; i++) {
    String topic = conf.getMqttBase() + conf.getDeviceId() + "/O" + String(i + 1);  // command for outputs
    mqttClient.subscribe(topic);
    printOk();
    Serial.println("Subcribed to " + topic);
  }

  mqttPublishDevice();
}

void loopMqtt() {
  if (!netOk) {
    return;
  }

  static unsigned int mqttSetup = 0;
  static unsigned long mqttLastPublish = 0;

  if (mqttSetup == 0) {
    mqttSetup = 1;

    printDo();
    Serial.println("Configuring MQTT on server " + conf.getMqttIp() + ":" + String(conf.getMqttPort()));

    ledFreeze(true);
    if (netMode == NET_ETH) {
      mqttClient.begin(parseIp(conf.getMqttIp()), conf.getMqttPort(), ethernetClient);  // if failed this may take a while (no timeout)
    } else {
      mqttClient.begin(parseIp(conf.getMqttIp()), conf.getMqttPort(), wifiClient);  // if failed this may take a while (no timeout)
    }
    ledFreeze(false);

    mqttClient.onMessage(mqttReceive);
  }

  mqttConnect();

  if (!mqttOk) {
    return;
  }

  if (!digitalRead(BTN_USER)) {  // publish to MQTT on button USER push
    mqttPublishDevice();
    mqttPublishInputs();
    delay(500);
  }

  if (mqttOk && conf.getMqttInterval() > 0 && loopTime - mqttLastPublish > (unsigned int)(conf.getMqttInterval() * 1000)) {
    mqttLastPublish = loopTime;

    mqttPublishDevice();
    mqttPublishInputs();
  }

  mqttClient.loop();
}

/**
 * GO!
 */

void setup() {
  setupLed();
  setupSerial();
  setupInfo();
  setupLed();
  setupFormat();
  setupConfig();
  setupNet();
  setupWeb();
}

void loop() {
  loopTime = millis();

  loopLed();
  loopAction();
  loopSerial();
  loopConfig();
  loopNet();
  loopWeb();
  loopMqtt();
}