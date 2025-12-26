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

#pragma once

#include <ArduinoJson.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <WiFi.h>
#include <HttpClient.h>
#include <NTPClient.h>
#include <mbed_mktime.h>
#include <MQTT.h>
#include "opta_info.h"
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

bool buttonOk = false;
bool configOk = false;
bool formatOk = false;
bool infoOk = false;
bool ledOk = false;
bool mqttOk = false;
bool netOk = false;
bool ntpOk = false;
bool serialOk = false;
bool webOk = false;

config conf;
EthernetServer ethernetServer(80);
WiFiServer wifiServer(80);
Client* webClient = nullptr;  // required to mix wifi and ethernet client
MQTTClient mqttClient;
EthernetClient ethernetClient;  // for MQTT
WiFiClient wifiClient;          // for MQTT

/**
 * Various
 */

void print(int action, String str) {
  String prefix;
  switch (action) {
    case DO: prefix = "* "; break;
    case OK: prefix = " > "; break;
    case KO: prefix = "!> "; break;
    case IN: prefix = "?> "; break;
    default: prefix = "";
  }

  Serial.println(prefix + str);
}

void printProgress(uint32_t offset, uint32_t size, uint32_t threshold, bool reset) {
  static int percent_done = 0;

  if (reset == true) {
    percent_done = 0;
    print(OK, String(percent_done) + "%");
  } else {
    uint32_t percent_done_new = offset * 100 / size;
    if (percent_done_new >= percent_done + threshold) {
      percent_done = percent_done_new;
      print(OK, String(percent_done) + "%");
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

String getLocalTime() {
  char buffer[32];
  tm t;
  _rtc_localtime(time(NULL), &t, RTC_FULL_LEAP_YEAR_SUPPORT);
  strftime(buffer, 32, "%k:%M:%S", &t);
  return String(buffer);
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
      print(OK, String(actionLoopCount) + " loops per second");

      actionLoopTime = actionLoopRepeat < 10 ? loopTime : 0;
      actionLoopCount = 0;
      actionLoopRepeat++;
    }
  }
}

// Start simple loops per second
void actionLoop() {
  print(DO, "Getting loop time");
  actionLoopTime = loopTime;
  actionLoopCount = 0;
  actionLoopRepeat = 0;
}

void actionReboot() {
  print(KO, "Device reboot");
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
  print(DO, "Getting board informations");
  if (optaMode == OPTA_NONE) {
    print(KO, "Failed to find board information");
    return;
  }
  print(OK, "Board type: " + getDeviceName());
  print(OK, "Board has Ethernet");

  if (optaMode == OPTA_WIFI) {
    print(OK, "Board has Wifi");
  }
  if (optaMode != OPTA_LITE) {
    print(OK, "Board has RS485");
  }
}

void actionDhcp() {
  if (!configOk) {
    return;
  }

  bool mode = conf.getNetDhcp() ? false : true;
  print(KO, String(mode ? "Enabling" : "Disabling") + " DHCP mode.");

  conf.setNetDhcp(mode);
  conf.toFile();
}

void actionWifi() {
  if (!configOk) {
    return;
  }

  bool mode = conf.getNetWifi() ? false : true;
  print(KO, String(mode ? "Enabling" : "Disabling") + " Wifi.");

  conf.setNetWifi(mode);
  conf.toFile();
}

void actionReset() {
  print(KO, "Resetting configuration to default");

  conf.resetFile();
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
    print(KO, "Error: QSPI init failure");
    return;
  }

  print(DO, "Checking partitions");

  bool perform = false;
  bool perform_user = true;
  if (!wifi_data_fs.mount(&wifi_data)) {
    print(OK, "Wifi partition already exist");
  } else {
    perform = true;
  }
  if (!ota_data_fs.mount(&ota_data)) {
    print(OK, "OTA partition already exist");
  } else {
    perform = true;
  }
  if (!user_data_fs.mount(&user_data)) {
    print(OK, "User partition already exist");
    perform_user = false;
  } else {
    perform = true;
  }
  if (perform) {
    print(OK, "Partition does not exist");
  }
  if (force) {
    print(DO, "Forcing partitions creation");
    perform = true;
  }

  if (!perform) {
    return;
  }

  // erase partitions
  if (force) {
    print(DO, "Full erasing partitions, please wait...");
    root->erase(0x0, root->size());
  } else {
    print(DO, "Erasing partitions, please wait...");
    root->erase(0x0, root->get_erase_size());
  }
  print(OK, "Erase completed");

  mbed::MBRBlockDevice::partition(root, 1, 0x0B, 0, 1 * 1024 * 1024);
  mbed::MBRBlockDevice::partition(root, 2, 0x0B, 1 * 1024 * 1024, 6 * 1024 * 1024);
  mbed::MBRBlockDevice::partition(root, 3, 0x0B, 6 * 1024 * 1024, 7 * 1024 * 1024);
  mbed::MBRBlockDevice::partition(root, 4, 0x0B, 7 * 1024 * 1024, 14 * 1024 * 1024);
  // use space from 15.5MB to 16 MB for another fw, memory mapped

  // format wifi partition
  print(DO, "Formatting Wifi partition");
  if (wifi_data_fs.reformat(&wifi_data)) {  // not used yet
    print(KO, "Error formatting WiFi partition");
    return;
  }

  uint32_t chunk_size = 1024;
  uint32_t byte_count = 0;

  // flash WiFi Firmware And Certificates
  FILE* fp = fopen("/wlan/4343WA1.BIN", "wb");
  print(DO, "Flashing WiFi firmware");
  printProgress(byte_count, file_size, 10, true);
  while (byte_count < file_size) {
    if (byte_count + chunk_size > file_size)
      chunk_size = file_size - byte_count;
    int ret = fwrite(&wifi_firmware_image_data[byte_count], chunk_size, 1, fp);
    if (ret != 1) {
      print(KO, "Error writing firmware data");
      break;
    }
    byte_count += chunk_size;
    printProgress(byte_count, file_size, 10, false);
  }
  fclose(fp);

  fp = fopen("/wlan/cacert.pem", "wb");

  print(DO, "Flashing certificates");
  chunk_size = 128;
  byte_count = 0;
  printProgress(byte_count, cacert_pem_len, 10, true);
  while (byte_count < cacert_pem_len) {
    if (byte_count + chunk_size > cacert_pem_len)
      chunk_size = cacert_pem_len - byte_count;
    int ret = fwrite(&cacert_pem[byte_count], chunk_size, 1, fp);
    if (ret != 1) {
      print(KO, "Error writing certificates");
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

  print(DO, "Flashing memory mapped WiFi firmware");
  printProgress(byte_count, file_size, 10, true);
  while (byte_count < file_size) {
    if (byte_count + chunk_size > file_size)
      chunk_size = file_size - byte_count;
    int ret = root->program(wifi_firmware_image_data, offset + byte_count, chunk_size);
    if (ret != 0) {
      print(KO, "Error writing memory mapped firmware");
      break;
    }
    byte_count += chunk_size;
    printProgress(byte_count, file_size, 10, false);
  }

  // format OTA partition
  print(DO, "Formatting OTA partition");
  if (ota_data_fs.reformat(&ota_data)) {
    print(KO, "Error formatting OTA partition");
    return;
  }

  // format user partition
  if (perform_user) {  // do not erase user partition if it exists !
    user_data_fs.unmount();
    print(DO, "Formatting USER partition");
    if (user_data_fs.reformat(&user_data)) {
      print(KO, "Error formatting user partition");
      return;
    }
  }

  print(DO, "QSPI Flash formatted");

  formatOk = true;
}

void actionPublishInputs() {
  if (!mqttOk) {
    return;
  }

  print(DO, "Publishing inputs informations to MQTT");

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

void actionPublishDevice() {
  if (!mqttOk) {
    return;
  }

  print(DO, "Publishing device informations to MQTT");

  String rootTopic = conf.getMqttBase() + conf.getDeviceId() + "/device/";
  mqttClient.publish(String(rootTopic + "type").c_str(), getDeviceName());
  mqttClient.publish(String(rootTopic + "ip").c_str(), getLocalIp().toString());
  mqttClient.publish(String(rootTopic + "version").c_str(), String(SKETCH_VERSION).c_str());
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
    print(DO, "Receiving command '" + message + "'");
    if (message.equals("ip")) {  // print local Ip to terminal
      print(DO, "Getting local IP address");
      print(OK, getLocalIp().toString());
    }
    if (message.equals("config")) {  // print configuration json to terminal
      print(DO, "Getting user configuration");
      print(OK, conf.toJson(false));
    }
    if (message.equals("info")) {  // print board info to terminal
      actionInfo();
    }
    if (message.equals("time")) {  // print local time to terminal
      print(DO, "Getting local time");
      if (ntpOk) {
        print(OK, "Time set to " + getLocalTime());
      } else {
        print(KO, "Time not updated " + getLocalTime());
      }
    }
    if (message.equals("loop")) {  // print 10 times the number of loops per second to terminal
      actionLoop();
    }
    if (message.equals("publish")) {  // publish to mqtt device info and inputs state
      actionPublishDevice();
      actionPublishInputs();
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
      print(KO, "You should reboot device");
    }
    if (message.equals("dhcp")) {  // update configuration to toggle DHCP mode
      actionDhcp();
      print(KO, "You should reboot device");
    }
    if (message.equals("wifi")) {  // update configuration to toggle WIFI mode
      actionWifi();
      print(KO, "You should reboot device");
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
 * Button
 */

void setupButton() {
  pinMode(BTN_USER, INPUT);  // init USER button
  buttonOk = true;
}

void loopButton() {
  static unsigned long btnPressStart = 0;
  static unsigned long btnPressLength = 0;

  if (!digitalRead(BTN_USER)) {
    if (btnPressStart == 0) {
      btnPressStart = loopTime;
    }
    btnPressLength = loopTime - btnPressStart;
  } else if (btnPressStart > 0) {
    btnPressStart = 0;
  }

  // wait button release
  if (btnPressStart > 0 || btnPressLength == 0) {
    return;
  }

  print(OK, "Button pressed " + String(btnPressLength) + " milliseconds");

  // if button press > 5s: reset config and reboot
  if (btnPressLength > 5000) {
    actionReset();
    actionReboot();
  }

  // if no network and button push more than 1s: switch WIFI mode in config and reboot
  if (!netOk && btnPressLength > 1000 && btnPressLength < 3000) {
    actionWifi();
    actionReboot();
  }

  // if no network and button push less than 1s: switch DHCP mode in config and reboot
  if (!netOk && btnPressLength < 1000) {
    actionDhcp();
    actionReboot();
  }
  // if network ok, mqtt connected, and button push less than 1s : publish MQTT info
  if (netOk && mqttOk && btnPressLength < 1000) {
    actionPublishDevice();
    actionPublishInputs();
  }

  btnPressLength = 0;
}

/**
 * Config
 */

void setupConfig() {
  // read flash
  print(OK, "Reading configuration from flash");

  bool ret = conf.fromFile();

  // configuartion file not found
  if (!ret) {
    print(OK, "Configuration not found");

    // wait user button press
  } else if (buttonOk) {
    print(OK, "Configuration found");
    print(IN, "Hold the user button to fully reset device. Waiting ");

    ledFreeze(true);
    unsigned long start = 0;
    for (int i = CONFIG_RESET_DELAY; i >= 0; i--) {
      start = millis();
      while (!digitalRead(BTN_USER)) {
        if (start + 3000 < millis()) {
          actionReset();
          i = -1;
          break;
        }
      }
      if (i >= 0) {
        delay(1000);
        print(OK, String(i));
      }
    }
    ledFreeze(false);
  }

  // configure board IO pins
  print(DO, "Configuring IO board pins");
  conf.initializePins();

  configOk = true;
}

void loopConfig() {
  static unsigned int pinsLastPoll = 0;
  static String prev_di_mask[NUM_INPUTS];

  // poll input
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

          print(OK, String("[" + inTopic + "] " + prev_di_mask[i] + " => " + cur_di_mask[i]).c_str());
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
  print(DO, "Configuring Ethernet");
  if (conf.getNetDhcp()) {
    print(OK, "using DHCP");
    ret = Ethernet.begin();  // If failed this can take 1 minute long...
  } else {
    print(OK, "using static IP");
    ret = Ethernet.begin(parseIp(conf.getNetIp()));  // If failed this can take 1 minute long...
  }
  ledFreeze(false);

  if (ret == 0) {
    print(KO, "Network connection failed.");
    if (Ethernet.linkStatus() == LinkOFF) {
      print(KO, "Ethernet cable not connected.");
    }
    netOk = false;
  } else {
    print(KO, "Network connected with IP ");
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

  print(OK, "using SSID '" + netApSsid + "' and password '" + netApPass + "'");
  if (conf.getNetDhcp()) {
    print(OK, "using DHCP");
  } else {
    print(OK, "using static IP");
    WiFi.config(parseIp(conf.getNetIp()));
  }

  ledFreeze(true);
  int ret = WiFi.begin(ssid, pass);
  ledFreeze(false);

  if (ret != WL_CONNECTED) {
    print(KO, "Failed to connect Wifi");

    netOk = false;
  } else {
    print(OK, "Wifi connected");
    netOk = true;
  }
}

void setupNetEthernet() {
  print(DO, "Configuring Ethernet network");
  netMode = NET_ETH;

  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    print(KO, "Communication with Ethernet module failed");
    return;
  }

  netEthernetConnect();
}

void setupNetWifiSta() {
  print(DO, "Configuring Wifi standard network");
  netMode = NET_STA;

  if (WiFi.status() == WL_NO_MODULE) {
    print(KO, "Communication with WiFi module failed");
    return;
  }

  netWifiStaConnect();
}

void setupNetWifiAp() {
  print(DO, "Configuring Wifi Access Point network");
  netMode = NET_AP;

  if (WiFi.status() == WL_NO_MODULE) {
    print(KO, "Communication with WiFi module failed");
    return;
  }

  String netApSsid = "opta2iot" + conf.getDeviceId();
  String netApPass = "opta2iot";
  char ssid[32];
  char pass[32];
  netApSsid.toCharArray(ssid, sizeof(ssid));
  netApPass.toCharArray(pass, sizeof(pass));

  print(OK, "using SSID '" + netApSsid + "' and password '" + netApPass + "' and IP " + conf.getNetIp());

  WiFi.config(parseIp(conf.getNetIp()));

  ledFreeze(true);
  int ret = WiFi.beginAP(ssid, pass);
  ledFreeze(false);

  if (ret != WL_AP_LISTENING) {
    print(KO, "Failed to create Wifi Access Point");

    netOk = false;
  } else {
    print(OK, "Wifi access point listening");
    netOk = true;
  }
}

void setupNet() {
  print(DO, "Selecting network mode");

  if (ledOk) {
    digitalWrite(LEDR, HIGH);
  }

  if (optaMode == OPTA_NONE) {
    print(KO, "Failed to find device type");
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
    print(OK, "DHCP attributed IP is " + getLocalIp().toString());
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
    print(OK, "Ethernet cable connected");
    netOk = true;
  }
  if (netOk && Ethernet.linkStatus() != LinkON) {
    print(KO, "Ethernet cable disconnected");
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
  static bool firstLoop = true;
  static int status = WL_IDLE_STATUS;

  if (status != WiFi.status()) {
    status = WiFi.status();

    if (status == WL_AP_CONNECTED) {
      print(OK, "Device connected to AP");
    } else if (firstLoop) {  // do not display message on startup
      firstLoop = false;
    } else {
      print(KO, "Device disconnected from AP");
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
 * NTP
 */

void ntpUpdate(UDP& udp) {
  print(DO, "Configuring local time from time server " + String(TIME_SERVER));

  ledFreeze(true);
  NTPClient timeClient(udp, TIME_SERVER, conf.getTimeOffset() * 3600, 0);
  timeClient.begin();
  timeClient.update();
  const unsigned long epoch = timeClient.getEpochTime();
  set_time(epoch);
  print(OK, "Time set to " + timeClient.getFormattedTime());
  ledFreeze(false);
}

void loopNtp() {
  if (!netOk || ntpOk || netMode == NET_AP) {
    return;
  }

  if (netMode == NET_ETH) {
    EthernetUDP ethernetUdpClient;
    ntpUpdate(ethernetUdpClient);
  } else {
    WiFiUDP wifiUdpClient;
    ntpUpdate(wifiUdpClient);
  }

  ntpOk = true;
}

/**
 * Web Server
 */

void setupWeb() {
  if (!configOk || netMode == NET_NONE) {
    return;
  }

  ledFreeze(true);
  if (netMode == NET_ETH) {
    print(DO, "Configuring Ethernet Web server");
    ethernetServer.begin();
  } else {
    print(DO, "Configuring Wifi Web server");
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
  doc["time"] = getLocalTime();
  doc["gmt"] = conf.getTimeOffset();

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

  print(DO, "Parsing received configuration");
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
    print(KO, "Failed to load configuration from response");
  } else {
    if (conf.getDeviceId() == "") {  // device ID must be set
      isValid = false;
      print(KO, "Missing device ID");
    }
    if (conf.getDeviceUser() == "") {  // device user must be set
      isValid = false;
      print(KO, "Missing device user");
    }
    if (conf.getDevicePassword() == "") {  // get old device password if none set
      print(OK, "Get previous device password");
      conf.setDevicePassword(oldConf.getDevicePassword());
    }
    int offset = conf.getTimeOffset();
    if (offset > 24 || offset < -24) {  // time offset must be in this day
      conf.setTimeOffset(0);
    }
    if (conf.getNetPassword() == "" && conf.getNetSsid() != "") {  // get old wifi password if none set
      print(OK, "Get previous Wifi password");
      conf.setNetPassword(oldConf.getNetPassword());
    }
    if (conf.getMqttPassword() == "" && conf.getMqttUser() != "") {  // get old mqtt password if none set
      print(OK, "Get previous MQTT password");
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

    print(DO, "Writing new configuration to flash");
    conf.toFile();

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
  actionPublishDevice();
  actionPublishInputs();

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
  print(OK, "Received " + topic + ": " + payload);

  String match = conf.getMqttBase() + conf.getDeviceId() + "/device/get";
  if (topic == match) {
    actionPublishDevice();
  }

  for (size_t i = 0; i < NUM_OUTPUTS; i++) {
    String match = conf.getMqttBase() + conf.getDeviceId() + "/O" + String(i + 1);
    if (topic == match) {
      print(OK, "Setting output " + String(i + 1) + " to " + payload);

      digitalWrite(conf.getOutputPin(i), payload.toInt());
      digitalWrite(conf.getOutputLed(i), payload.toInt());
    }
  }
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
  print(DO, "Connecting to MQTT broker");

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
    print(KO, "Failed to connect to MQTT broker");
    ledFreeze(false);
    return;
  }
  ledFreeze(false);

  print(OK, "MQTT broker found");
  mqttOk = true;

  String topic = conf.getMqttBase() + conf.getDeviceId() + "/device/get";  // command for device information
  mqttClient.subscribe(topic);
  print(OK, "Subcribed to " + topic);

  for (size_t i = 0; i < NUM_OUTPUTS; i++) {
    String topic = conf.getMqttBase() + conf.getDeviceId() + "/O" + String(i + 1);  // command for outputs
    mqttClient.subscribe(topic);
    print(OK, "Subcribed to " + topic);
  }

  actionPublishDevice();
}

void loopMqtt() {
  static bool mqttSetup = false;
  static unsigned long mqttLastPublish = 0;

  if (!netOk || netMode == NET_AP) {
    return;
  }

  if (!mqttSetup) {
    print(DO, "Configuring MQTT on server " + conf.getMqttIp() + ":" + String(conf.getMqttPort()));

    ledFreeze(true);
    if (netMode == NET_ETH) {
      mqttClient.begin(parseIp(conf.getMqttIp()), conf.getMqttPort(), ethernetClient);  // if failed this may take a while (no timeout)
    } else {
      mqttClient.begin(parseIp(conf.getMqttIp()), conf.getMqttPort(), wifiClient);  // if failed this may take a while (no timeout)
    }
    ledFreeze(false);

    mqttClient.onMessage(mqttReceive);
    mqttSetup = true;
  }

  mqttConnect();

  if (!mqttOk) {
    return;
  }

  if ((conf.getMqttInterval() > 0 && loopTime - mqttLastPublish > (unsigned int)(conf.getMqttInterval() * 1000))) {
    mqttLastPublish = loopTime;

    actionPublishDevice();
    actionPublishInputs();
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
  setupButton();
  setupFormat();
  setupConfig();
  setupNet();
  setupWeb();
}

void loop() {
  loopTime = millis();

  loopLed();
  loopButton();
  loopAction();
  loopSerial();
  loopConfig();
  loopNet();
  loopNtp();
  loopWeb();
  loopMqtt();
}