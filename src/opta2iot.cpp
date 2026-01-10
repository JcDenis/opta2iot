/* opta2iot
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

// Required librairies
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoMqttClient.h>
#include "KVStore.h"
#include "kvstore_global_api.h"
#include <Ethernet.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <mbed_mktime.h>
#include <base64.hpp>
#include "define.h"
#include "opta2iot.h"
#include "html.h"
#include "certificates.h"

// Opta board info
#include "opta_info.h"  //mbed
OptaBoardInfo *info;
OptaBoardInfo *boardInfo();

// Flash
#include "BlockDevice.h"
#include "MBRBlockDevice.h"
#include "FATFileSystem.h"
const uint32_t file_size = 421098;
extern const unsigned char wifi_firmware_image_data[];

// Set namespace
namespace opta2iot {

Opta::Opta() {
  // create static instance
  instance = this;  // required by MQTT.onMessage() and Thread.start()
}

Opta *Opta::instance = nullptr;

char *Opta::version() {
  if (!_version[0]) {
    // parse to human readable version
    size_t part[4];
    char ver[16];
    itoa(Revision, ver, 10);
    sscanf(ver, "%04d%02d%02d%02d", &part[0], &part[1], &part[2], &part[3]);
    sprintf(_version, "%04d.%02d.%02d (r%02d)", part[0], part[1], part[2], part[3]);
  }

  return _version;
}

uint32_t Opta::now(uint32_t now) {
  if (now > 0) {
    _now = now;
  }

  return _now;
}

bool Opta::setup() {
  return startSetup()
         && serialSetup()
         && boardSetup()
         && flashSetup()
         && ledSetup()
         && buttonSetup()
         && configSetup()
         && ioSetup()
         && networkSetup()
         && timeSetup()
         && mqttSetup()
         && webSetup()
         && endSetup();
}

bool Opta::loop() {
  return startLoop()
         && serialLoop()
         && ledLoop()
         && buttonLoop()
         && ioLoop()
         && networkLoop()
         && timeLoop()
         && mqttLoop()
         && webLoop()
         && endLoop();
}

bool Opta::startSetup() {
  _now = millis();

  return true;
}

bool Opta::endSetup() {
  //thread(); // It is better to do it in .ino file

  float last = floor((millis() - _now) / 1000);
  serialLine("Setup completed in " + String((int)last) + " secondes");

  return true;
}

bool Opta::startLoop() {
  _now = millis();

  return !_stop;
}

bool Opta::endLoop() {
  _odd = !_odd;

  return true;
}

bool Opta::stop(String reason) {
  return stop(reason.c_str());
}

bool Opta::stop(const char *reason) {
  serialWarn(reason);
  _stop = true;
  digitalWrite(BoardUserLeds[0], HIGH);
  digitalWrite(BoardUserLeds[1], LOW);
  digitalWrite(BoardUserLeds[2], LOW);

  return false;
}

bool Opta::running() {
  return !_stop;
}

void Opta::print(String str) {
  Serial.print(str);
}
void Opta::print(const char *str) {
  Serial.print(str);
}

void Opta::format() {
  flashFormat(true);
}

void Opta::reset() {
  serialInfo(F("Resetting device"));

  kv_reset("/kv/");
  delay(10);
  configReadFromDefault();
  delay(10);
  configWriteToFile();
  delay(10);
}

void Opta::reboot() {
  serialLine(F("Rebooting device"));

  bool on = true;
  for (int i = 0; i < 10; i++) {
    on = on ? false : true;
    ledSetRed(on);
    ledSetGreen(!on);
    delay(100);
  }

  NVIC_SystemReset();
}

void Opta::thread() {
  serialLine("Starting threaded loop");

  if (!_threadStarted) {
    _threadStarted = true;

    static rtos::Thread thread;
    thread.start([]() {
      // rtos::Thread.start() requires a static callback
      if (instance) {
        while(instance->loop()){}
      }
    });
  }
}

/*
 * Serial
 */

bool Opta::serialSetup() {
  Serial.begin(115200);
  for (size_t i = 0; i < 5000; i++) {
    delay(1);
    if (Serial) {
      break;
    }
  }

  print(F("\n"));
  print(F("+—————————————————————————————————————+\n"));
  print(F("| Arduino Opta Industrial IoT gateway |\n"));
  print(F("+—————————————————————————————————————+\n"));
  print(F("\n"));

  return true;
}

bool Opta::serialLoop() {
  if (serialIncoming()) {
    String message = serialReceived();
    if (message.equals("loop")) {
      timeLoop(true);
    }
    if (message.equals("ip")) {
      serialLine(F("Getting local IP address"));
      serialInfo(networkLocalIp().toString());
    }
    if (message.equals("config")) {
      serialInfo(configWriteToJson(false));
    }
    if (message.equals("time")) {
      serialLine(F("Getting local time"));
      serialInfo(timeGet());
    }
    if (message.equals("update time")) {
      timeUpdate();
    }
    if (message.equals("version")) {
      serialInfo(version());
    }
    if (message.equals("format")) {
      //reset();
      if (flashFormat(true)) {
        reboot();
      }
    }
    if (message.equals("reset")) {
      reset();
      serialWarn(F("You should reboot device"));
    }
    if (message.equals("reboot")) {
      reboot();
    }
    if (message.equals("dhcp")) {
      configSetNetworkDhcp(configGetNetworkDhcp() ? false : true);
      configWriteToFile();
      serialWarn(F("You should reboot device"));
    }
    if (message.equals("wifi")) {
      configSetNetworkWifi(configGetNetworkWifi() ? false : true);
      configWriteToFile();
      serialWarn(F("You should reboot device"));
    }
    if (message.equals("publish")) {
      mqttPublishDevice();
      mqttPublishInputs();
    }
  }

  return true;
}

void Opta::serialLine(String str) {
  print("* " + str + "\n");
}
void Opta::serialLine(const char *str) {
  print("* " + String(str) + "\n");
}

void Opta::serialInfo(String str) {
  if (SerialVerbose) {
    print(" > " + str + "\n");
  }
}
void Opta::serialInfo(const char *str) {
  if (SerialVerbose) {
    print(" > " + String(str) + "\n");
  }
}

void Opta::serialWarn(String str) {
  print("!> " + str + "\n");
}
void Opta::serialWarn(const char *str) {
  print("!> " + String(str) + "\n");
}

void Opta::serialProgress(uint32_t offset, uint32_t size, uint32_t threshold, bool reset) {
  if (SerialVerbose) {
    if (reset == true) {
      _serialProgress = 0;
      serialInfo(String(_serialProgress) + "%");
    } else {
      byte percent_done_new = offset * 100 / size;
      if (percent_done_new >= _serialProgress + threshold) {
        _serialProgress = percent_done_new;
        serialInfo(String(_serialProgress) + "%");
      }
    }
  }
}

bool Opta::serialIncoming() {
  byte index = 0;
  bool ended = false;

  while (Serial.available() && !ended) {
    int c = Serial.read();
    if (c != -1) {
      switch (c) {
        case '\n':
          _serialIncoming[index] = '\0';
          serialLine("Receiveing serial message: " + String(_serialIncoming));
          index = 0;
          ended = true;
          break;
        default:
          if (index <= 49) {  // max len to 50
            _serialIncoming[index++] = (char)c;
          }
          break;
      }
    }
  }

  return ended;
}

String Opta::serialReceived() {
  String msg = String(_serialIncoming);
  msg.toLowerCase();

  return msg;
}

/*
 * Board
 */

bool Opta::boardSetup() {
  serialLine(F("Configuring board"));

  info = boardInfo();
  if (info->magic == 0xB5) {
    if (info->_board_functionalities.ethernet == 1) {
      _boardType = BoardType::Lite;
      _boardName = "Opta Lite AFX00003 ";
      //MacEthernet = String(info->mac_address[0], HEX) + ":" + String(info->mac_address[1], HEX) + ":" + String(info->mac_address[2], HEX) + ":" + String(info->mac_address[3], HEX) + ":" + String(info->mac_address[4], HEX) + ":" + String(info->mac_address[5], HEX);
    }
    if (info->_board_functionalities.rs485 == 1) {
      _boardType = BoardType::Rs485;
      _boardName = "Opta RS485 AFX00001";
    }
    if (info->_board_functionalities.wifi == 1) {
      _boardType = BoardType::Wifi;
      _boardName = "Opta Wifi AFX00002 ";
      //MacWifi = String(info->mac_address_2[0], HEX) + ":" + String(info->mac_address_2[1], HEX) + ":" + String(info->mac_address_2[2], HEX) + ":" + String(info->mac_address_2[3], HEX) + ":" + String(info->mac_address_2[4], HEX) + ":" + String(info->mac_address_2[5], HEX);
    }
  }

  if (_boardType == BoardType::Undefined) {
    // Ko
    return stop(F("Failed to find board type"));
  }
  serialInfo("Board is " + String(_boardName));

  return true;
}

size_t Opta::boardGetInputsNum() {
  return sizeof(BoardInputs) / sizeof(BoardInputs[0]);
}
size_t Opta::boardGetOutputsNum() {
  return sizeof(BoardOutputs) / sizeof(BoardOutputs[0]);
}

/*
 * Flash
 */

bool Opta::flashSetup() {
  serialLine(F("Configuring flash memory"));

  _flashRoot = mbed::BlockDevice::get_default_instance();
  if (_flashRoot->init() != mbed::BD_ERROR_OK) {
    serialWarn(F("QSPI initialization failed"));
    return false;
  }

  return flashFormat();
}

bool Opta::flashHasWifi() {
  mbed::MBRBlockDevice wifi_data(_flashRoot, 1);
  mbed::FATFileSystem wifi_data_fs("wlan");

  return wifi_data_fs.mount(&wifi_data) == 0;
}

bool Opta::flashHasOta() {
  mbed::MBRBlockDevice ota_data(_flashRoot, 2);
  mbed::FATFileSystem ota_data_fs("fs");

  return ota_data_fs.mount(&ota_data) == 0;
}

bool Opta::flashHasUser() {
  mbed::MBRBlockDevice user_data(_flashRoot, 2);
  mbed::FATFileSystem user_data_fs("fs");

  return user_data_fs.mount(&user_data) == 0;
}

bool Opta::flashFormat(bool force) {
  mbed::MBRBlockDevice wifi_data(_flashRoot, 1);
  mbed::FATFileSystem wifi_data_fs("wlan");
  bool noWifi = wifi_data_fs.mount(&wifi_data) != 0;
  serialInfo(String(noWifi ? "Missing" : "Existing") + " Wifi partition");

  mbed::MBRBlockDevice ota_data(_flashRoot, 2);
  mbed::FATFileSystem ota_data_fs("fs");
  bool noOta = ota_data_fs.mount(&ota_data) != 0;
  serialInfo(String(noOta ? "Missing" : "Existing") + " OTA partition");

  mbed::MBRBlockDevice kvstore_data(_flashRoot, 3);
  // do not touch this one

  mbed::MBRBlockDevice user_data(_flashRoot, 4);
  mbed::FATFileSystem user_data_fs("fs");
  bool noUser = user_data_fs.mount(&user_data) != 0;
  serialInfo(String(noUser ? "Missing" : "Existing") + " User partition");

  bool perform = force || noWifi || noOta || noUser;

  if (perform) {
    serialLine(F("Erasing partitions, please wait..."));
    _flashRoot->erase(0x0, _flashRoot->size());
    serialInfo(F("Erase completed"));
  }

  mbed::MBRBlockDevice::partition(_flashRoot, 1, 0x0B, 0, 1 * 1024 * 1024);
  mbed::MBRBlockDevice::partition(_flashRoot, 2, 0x0B, 1 * 1024 * 1024, 6 * 1024 * 1024);
  mbed::MBRBlockDevice::partition(_flashRoot, 3, 0x0B, 6 * 1024 * 1024, 7 * 1024 * 1024);
  mbed::MBRBlockDevice::partition(_flashRoot, 4, 0x0B, 7 * 1024 * 1024, 14 * 1024 * 1024);
  // use space from 15.5MB to 16 MB for another fw, memory mapped

  if (force || noWifi) {
    serialLine(F("Formatting Wifi partition"));

    wifi_data_fs.unmount();
    if (wifi_data_fs.reformat(&wifi_data) != 0) {  // not used yet
      serialWarn(F("Error formatting WiFi partition"));
      return false;
    }

    if (!flashWiFiFirmwareAndCertificates() || !flashWiFiFirmwareMapped()) {
      return false;
    }
  }

  if (force || noOta) {
    serialLine(F("Formatting OTA partition"));

    ota_data_fs.unmount();
    if (ota_data_fs.reformat(&ota_data) != 0) {
      serialWarn(F("Error formatting OTA partition"));
      return false;
    }
  }

  if (force || noUser) {
    serialLine(F("Formatting User partition"));

    user_data_fs.unmount();
    if (user_data_fs.reformat(&user_data) != 0) {
      serialWarn(F("Error formatting user partition"));
      return false;
    }
  }

  return true;
}

bool Opta::flashWiFiFirmwareAndCertificates() {
  FILE *fp = fopen("/wlan/4343WA1.BIN", "wb");
  uint32_t chunk_size = 1024;
  uint32_t byte_count = 0;

  serialLine(F("Flashing WiFi firmware"));
  serialProgress(byte_count, file_size, 10, true);
  while (byte_count < file_size) {
    if (byte_count + chunk_size > file_size)
      chunk_size = file_size - byte_count;
    int ret = fwrite(&wifi_firmware_image_data[byte_count], chunk_size, 1, fp);
    if (ret != 1) {
      serialWarn(F("Error writing firmware data"));

      return false;
    }
    byte_count += chunk_size;
    serialProgress(byte_count, file_size, 10, false);
  }
  fclose(fp);

  fp = fopen("/wlan/cacert.pem", "wb");
  chunk_size = 128;
  byte_count = 0;

  serialLine(F("Flashing certificates"));
  serialProgress(byte_count, cacert_pem_len, 10, true);

  while (byte_count < cacert_pem_len) {
    if (byte_count + chunk_size > cacert_pem_len)
      chunk_size = cacert_pem_len - byte_count;
    int ret = fwrite(&cacert_pem[byte_count], chunk_size, 1, fp);
    if (ret != 1) {
      serialWarn(F("Error writing certificates"));

      return false;
    }
    byte_count += chunk_size;
    serialProgress(byte_count, cacert_pem_len, 10, false);
  }

  return true;
}

bool Opta::flashWiFiFirmwareMapped() {
  uint32_t chunk_size = 1024;
  uint32_t byte_count = 0;
  const uint32_t offset = 15 * 1024 * 1024 + 1024 * 512;

  serialLine(F("Flashing memory mapped WiFi firmware"));
  serialProgress(byte_count, file_size, 10, true);

  while (byte_count < file_size) {
    if (byte_count + chunk_size > file_size)
      chunk_size = file_size - byte_count;
    int ret = _flashRoot->program(wifi_firmware_image_data, offset + byte_count, chunk_size);
    if (ret != 0) {
      serialWarn(F("Error writing memory mapped firmware"));

      return false;
    }
    byte_count += chunk_size;
    serialProgress(byte_count, file_size, 10, false);
  }

  return true;
}

/*
 * User LEDs
 */

bool Opta::ledSetup() {
  serialLine(F("Configuring User LEDs"));

  serialInfo("Set Green LED on pin " + String(BoardUserLeds[0]));
  pinMode(BoardUserLeds[0], OUTPUT);
  serialInfo("Set Red LED on pin " + String(BoardUserLeds[1]));
  pinMode(BoardUserLeds[1], OUTPUT);
  serialInfo("Set Blue LED on pin " + String(BoardUserLeds[2]));
  pinMode(BoardUserLeds[2], OUTPUT);

  digitalWrite(BoardUserLeds[0], LOW);
  digitalWrite(BoardUserLeds[1], LOW);
  digitalWrite(BoardUserLeds[2], LOW);

  return true;
}

bool Opta::ledLoop() {
  if (_now - _ledConnectionStart > 750) {
    // connections
    _ledConnectionState = !_ledConnectionState;
    _ledConnectionStart = _now;

    ledSetRed(_networkConnected ? false : _ledConnectionState);
    ledSetGreen(_networkConnected && _mqttConnected ? _ledConnectionState : false);
    if (_networkSelected == NetworkType::AccessPoint) {
      ledSetBlue(_ledConnectionState);
    } else if (_networkSelected == NetworkType::Standard) {
      ledSetBlue(true);
    }
  }

  if (_now - _ledHeartbeatStart > 10000) {
    if (_ledHeartBeatStep == 0) {
      _ledHeartBeatStep = 1;
      _ledGreen = ledGetGreen();
      _ledRed = ledGetRed();
      ledSetGreen(false);
      ledSetRed(false);
    }
    if (_ledHeartBeatStep == 1 && _now - _ledHeartbeatStart > 10150) {
      _ledHeartBeatStep = 2;
      ledSetGreen(true);
      ledSetRed(true);
    }
    if (_ledHeartBeatStep == 2 && _now - _ledHeartbeatStart > 10200) {
      _ledHeartBeatStep = 3;
      ledSetGreen(false);
      ledSetRed(false);
    }
    if (_ledHeartBeatStep == 3 && _now - _ledHeartbeatStart > 10350) {
      _ledHeartBeatStep = 0;
      _ledHeartbeatStart = _now;
      serialInfo("I'm alive at " + timeGet());
      ledSetGreen(_ledGreen);
      ledSetRed(_ledRed);
    }
  }

  return true;
}

bool Opta::ledGetGreen() {
  return digitalRead(BoardUserLeds[0]) == HIGH ? true : false;
}

void Opta::ledSetGreen(bool on) {
  //serialInfo("Light " + String(on ? "on" : "off") + " green LED");
  digitalWrite(BoardUserLeds[0], on ? HIGH : LOW);
}

bool Opta::ledGetRed() {
  return digitalRead(BoardUserLeds[1]) == HIGH ? true : false;
}

void Opta::ledSetRed(bool on) {
  //serialInfo("Light " + String(on ? "on" : "off") + " red LED");
  digitalWrite(BoardUserLeds[1], on ? HIGH : LOW);
}

bool Opta::ledGetBlue() {
  return digitalRead(BoardUserLeds[2]) == HIGH ? true : false;
}

void Opta::ledSetBlue(bool on) {
  //serialInfo("Light " + String(on ? "on" : "off") + " blue LED");
  digitalWrite(BoardUserLeds[2], on ? HIGH : LOW);
}

void Opta::ledSetFreeze(bool on) {
  if (on) {
    _ledGreen = ledGetGreen();
    _ledRed = ledGetRed();
    _ledBlue = ledGetBlue();

    ledSetGreen(true);
    ledSetRed(true);
    ledSetBlue(false);
  } else {
    ledSetGreen(_ledGreen);
    ledSetRed(_ledRed);
    ledSetBlue(_ledBlue);
  }
}

/**
 * User buttons
 */

bool Opta::buttonSetup() {
  serialLine(F("Configuring buttons"));

  serialInfo("Set user button on pin " + String(BoardUserButtons[0]));
  pinMode(BoardUserButtons[0], INPUT);

  return true;
}

bool Opta::buttonLoop() {
  unsigned long duration = buttonDuration();
  if (duration > 0) {

    // if button press > 5s: reset config and reboot
    if (duration > 5000) {
      reset();
      reboot();
    }

    // if network not connected or as access point and button push less than 1s: switch DHCP mode in config and reboot
    if ((!_networkConnected || (_networkSelected == NetworkType::AccessPoint)) && (duration < 1000)) {
      configSetNetworkDhcp(configGetNetworkDhcp() ? false : true);
      configWriteToFile();
      reboot();
    }

    // if network not connected or as access point and button push bettwen 1s and 3s: switch WIFI mode in config and reboot
    if ((!_networkConnected || (_networkSelected == NetworkType::AccessPoint)) && (duration > 1000) && (duration < 3000)) {
      configSetNetworkWifi(configGetNetworkWifi() ? false : true);
      configWriteToFile();
      reboot();
    }

    // if network connected and mqtt connected and button push less than 1s : publish MQTT info
    if (_networkConnected && !(_networkSelected == NetworkType::AccessPoint) && _mqttConnected && (duration < 1000)) {
      mqttPublishDevice();
      mqttPublishInputs();
    }
  }

  return true;
}

bool Opta::buttonGet() {
  return digitalRead(BoardUserButtons[0]) == LOW;  // press = LOW = true
}

unsigned long Opta::buttonDuration() {
  if (buttonGet()) {
    if (_buttonStart == 0) {
      delay(1);
      _buttonStart = _now;
    }
    _buttonDuration = _now - _buttonStart;

    return 0;
  } else if (_buttonStart > 0 && _buttonDuration > 0) {
    _buttonStart = 0;
    serialInfo("Button was activated " + String(_buttonDuration) + " milliseconds");

    return _buttonDuration;
  }

  return 0;
}

/*
 *Config
 */

bool Opta::configSetup() {
  serialLine(F("Configuring parameters"));

  if (configReadFromFile()) {
    serialWarn(F("Hold for 5 seconds the user button to fully reset device. Waiting "));

    unsigned long resetPushStart = 0;
    bool resetLedState = false;
    for (size_t i = 4; i-- > 0;) {  // boot delay of 3 seconds
      for (size_t j = 0; j < 20; j++) {
        delay(50);
        resetLedState = !resetLedState;
        ledSetRed(resetLedState);

        resetPushStart = millis();
        while (buttonGet()) {
          ledSetRed(true);
          if (resetPushStart + 5000 < millis()) {
            reset();
            reboot();
            i = 0;
            break;
          }
        }
      }
      if (i > 0) {
        serialInfo(String(i));
      }
    }
    ledSetRed(false);
  }

  return true;
}

String Opta::configGetDeviceId() const {
  return _configDeviceId;
}

void Opta::configSetDeviceId(const String &id) {
  _configDeviceId = id;
}

String Opta::configGetDeviceUser() const {
  return _configDeviceUser;
}

void Opta::configSetDeviceUser(const String &user) {
  _configDeviceUser = user;
}

String Opta::configGetDevicePassword() const {
  return _configDevicePassword;
}

void Opta::configSetDevicePassword(const String &pass) {
  _configDevicePassword = pass;
}

int Opta::configGetTimeOffset() const {
  return _configTimeOffset;
}

void Opta::setTimeOffset(const int offset) {
  _configTimeOffset = offset;
}

String Opta::configGetNetworkIp() const {
  return _configNetworkIp;
}

void Opta::configSetNetworkIp(const String &ip) {
  _configNetworkIp = ip;
}

bool Opta::configGetNetworkDhcp() const {
  return _configNetworkDhcp;
}

void Opta::configSetNetworkDhcp(const bool on) {
  serialLine(String(on ? "Enabling" : "Disabling") + " DHCP");
  _configNetworkDhcp = on;
}

bool Opta::configGetNetworkWifi() const {
  return _configNetworkWifi;
}

void Opta::configSetNetworkWifi(const bool on) {
  serialLine(String(on ? "Enabling" : "Disabling") + " Wifi");
  _configNetworkWifi = on;
}

String Opta::configGetNetworkSsid() const {
  return _configNetworkSsid;
}

void Opta::configSetNetworkSsid(const String &id) {
  _configNetworkSsid = id;
}

String Opta::configGetNetworkPassword() const {
  return _configNetworkPassword;
}

void Opta::configSetNetworkPassword(const String &pass) {
  _configNetworkPassword = pass;
}

String Opta::configGetMqttIp() const {
  return _configMqttIp;
}

void Opta::configSetMqttIp(const String &ip) {
  _configMqttIp = ip;
}

int Opta::configGetMqttPort() const {
  return _configMqttPort;
}

void Opta::configSetMqttPort(const int port) {
  _configMqttPort = port;
}

String Opta::configGetMqttUser() const {
  return _configMqttUser;
}

void Opta::configSetMqttUser(const String &user) {
  _configMqttUser = user;
}

String Opta::configGetMqttPassword() const {
  return _configMqttPassword;
}

void Opta::configSetMqttPassword(const String &password) {
  _configMqttPassword = password;
}

String Opta::configGetMqttBase() const {
  return _configMqttBase;
}

void Opta::configSetMqttBase(const String &base) {
  _configMqttBase = base;
}

int Opta::configGetMqttInterval() const {
  return _configMqttInterval;
}

void Opta::configSetMqttInterval(int interval) {
  _configMqttInterval = interval;
}

byte Opta::configGetInputType(size_t index) {
  if (index < boardGetInputsNum()) {
    return _configInputs[index];
  }
  return IoType::Analog;
}

bool Opta::configSetInputType(size_t index, byte type) {
  if (index < boardGetInputsNum() && (type == IoType::Pulse || type == IoType::Digital || type == IoType::Analog)) {
    _configInputs[index] = type;
    return true;
  }
  return false;
}

bool Opta::configReadFromJson(const char *buffer, size_t length) {
  serialInfo(F("Reading configuration from JSON"));

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, buffer, length);

  if (error) {
    serialWarn(F("Failed to parse JSON"));

    return false;
  }

  if (doc["deviceId"].isNull()
      || doc["deviceUser"].isNull()
      || doc["devicePassword"].isNull()
      || doc["timeOffset"].isNull()
      || doc["netIp"].isNull()
      || doc["netDhcp"].isNull()
      || doc["netWifi"].isNull()
      || doc["netSsid"].isNull()
      || doc["netPassword"].isNull()
      || doc["mqttIp"].isNull()
      || doc["mqttPort"].isNull()
      || doc["mqttUser"].isNull()
      || doc["mqttPassword"].isNull()
      || doc["mqttBase"].isNull()
      || doc["mqttInterval"].isNull()
      || doc["inputs"].isNull()) {
    serialWarn(F("Missing required keys in JSON"));

    return false;
  }

  _configDeviceId = doc["deviceId"].as<String>();
  _configDeviceUser = doc["deviceUser"].as<String>();
  _configDevicePassword = doc["devicePassword"].as<String>();
  _configTimeOffset = doc["timeOffset"].as<int>();
  _configNetworkIp = doc["netIp"].as<String>();
  _configNetworkDhcp = doc["netDhcp"].as<bool>();
  _configNetworkWifi = doc["netWifi"].as<bool>();
  _configNetworkSsid = doc["netSsid"].as<String>();
  _configNetworkPassword = doc["netPassword"].as<String>();
  _configMqttIp = doc["mqttIp"].as<String>();
  _configMqttPort = doc["mqttPort"].as<int>();
  _configMqttUser = doc["mqttUser"].as<String>();
  _configMqttPassword = doc["mqttPassword"].as<String>();
  _configMqttBase = doc["mqttBase"].as<String>();
  _configMqttInterval = doc["mqttInterval"].as<int>() < 0 ? 0 : doc["mqttInterval"].as<int>();

  for (size_t i = 0; i < boardGetInputsNum(); ++i) {
    String pinName = "I" + String(i + 1);
    int pinType = doc["inputs"][pinName].as<int>();
    _configInputs[i] = pinType == IoType::Analog || pinType == IoType::Digital || pinType == IoType::Pulse ? pinType : IoType::Analog;
  }

  return true;
}

String Opta::configWriteToJson(const bool nopass) {
  //serialInfo("Writing configuration to JSON");

  JsonDocument doc;

  doc["version"] = version();
  doc["deviceId"] = _configDeviceId;
  doc["deviceUser"] = _configDeviceUser;
  doc["devicePassword"] = nopass ? "" : _configDevicePassword;
  doc["timeOffset"] = _configTimeOffset;
  doc["netIp"] = _configNetworkIp;
  doc["netDhcp"] = _configNetworkDhcp;
  doc["netWifi"] = _configNetworkWifi;
  doc["netSsid"] = _configNetworkSsid;
  doc["netPassword"] = nopass ? "" : _configNetworkPassword;
  doc["mqttIp"] = _configMqttIp;
  doc["mqttPort"] = _configMqttPort;
  doc["mqttUser"] = _configMqttUser;
  doc["mqttPassword"] = nopass ? "" : _configMqttPassword;
  doc["mqttBase"] = _configMqttBase;
  doc["mqttInterval"] = _configMqttInterval;

  for (size_t i = 0; i < boardGetInputsNum(); ++i) {
    String pinName = "I" + String(i + 1);
    doc["inputs"][pinName] = _configInputs[i];
  }

  String jsonString;
  serializeJson(doc, jsonString);

  return jsonString;
}

void Opta::configReadFromDefault() {
  serialInfo(F("Loading default configuration"));

  _configDeviceId = OPTA2IOT_DEVICE_ID;
  _configDeviceUser = OPTA2IOT_DEVICE_USER;
  _configDevicePassword = OPTA2IOT_DEVICE_PASSWORD;
  _configTimeOffset = OPTA2IOT_TIME_OFFSET;

  _configNetworkIp = OPTA2IOT_NET_IP;
  _configNetworkDhcp = OPTA2IOT_NET_DHCP;
  _configNetworkWifi = OPTA2IOT_NET_WIFI;
  _configNetworkSsid = OPTA2IOT_NET_SSID;
  _configNetworkPassword = OPTA2IOT_NET_PASSWORD;

  _configMqttIp = OPTA2IOT_MQTT_IP;
  _configMqttPort = OPTA2IOT_MQTT_PORT;
  _configMqttUser = OPTA2IOT_MQTT_USER;
  _configMqttPassword = OPTA2IOT_MQTT_PASSWORD;
  _configMqttBase = OPTA2IOT_MQTT_BASE;
  _configMqttInterval = OPTA2IOT_MQTT_INTERVAL;

  for (size_t i = 0; i < boardGetInputsNum(); i++) {
    _configInputs[i] = IoType::Digital;
  }
}

bool Opta::configWriteToFile() {
  String str = configWriteToJson(false);

  serialInfo(F("Writing configuration to flash memory"));
  kv_set("config", str.c_str(), str.length(), 0);

  return true;
}

bool Opta::configReadFromFile() {
  serialInfo(F("Reading configuration from flash memory"));

  bool ret = true;
  char readBuffer[1024];
  kv_get("config", readBuffer, 1024, 0);
  if (configReadFromJson(readBuffer, 1024) < 1) {
    serialWarn(F("Configuration file not found"));
    reset();
    ret = false;
  }

  return ret;
}

/*
 * IO
 */

bool Opta::ioSetup() {
  serialLine(F("Configuring IO"));

  serialInfo("Set IO resolution to " + String(IoResolution));
  analogReadResolution(IoResolution);

  for (size_t i = 0; i < boardGetInputsNum(); ++i) {
    serialInfo("Set input " + String(i + 1) + " of type " + String(_configInputs[i]) + " on pin " + String(BoardInputs[i]));
    if (_configInputs[i] == IoType::Digital || _configInputs[i] == IoType::Pulse) {
      pinMode(BoardInputs[i], INPUT);
    }
  }
  for (size_t i = 0; i < boardGetOutputsNum(); ++i) {
    serialInfo("Set output " + String(i + 1) + " on pin " + String(BoardInputs[i]) + " with LED on pin " + BoardOutputsLeds[i]);
    pinMode(BoardOutputs[i], OUTPUT);
    pinMode(BoardOutputsLeds[i], OUTPUT);

    digitalWrite(BoardOutputs[i], LOW);
    digitalWrite(BoardOutputsLeds[i], LOW);
  }

  return true;
}

bool Opta::ioLoop() {
  if (_networkConnected && _mqttConnected) {
    if (_now - _ioLastPoll > IoPollDelay) {  // Inputs loop delay
      String inputsCurrent[IoMaxInputs];
      for (size_t i = 0; i < boardGetInputsNum(); i++) {
        if (_configInputs[i] == IoType::Analog) {
          float value = analogRead(BoardInputs[i]) * (3.249 / ((1 << IoResolution) - 1)) / 0.3034;
          char buffer[10];
          snprintf(buffer, sizeof(buffer), "%0.1f", value);

          inputsCurrent[i] = String(buffer).c_str();
        } else {
          int value = digitalRead(BoardInputs[i]);
          inputsCurrent[i] = String(value).c_str();
        }

        if (!inputsCurrent[i].equals(_ioPreviousState[i])) {
          //ts ... && only 1 for pulse
          if (_ioLastPoll > 0 && (_configInputs[i] != IoType::Pulse || inputsCurrent[i].equals(String('1')))) {
            String inTopic = "I" + String(i + 1);
            String rootTopic = _configMqttBase + _configDeviceId + "/";

            mqttPublish(String(rootTopic + inTopic + "/val").c_str(), String(inputsCurrent[i]).c_str());
            mqttPublish(String(rootTopic + inTopic + "/type").c_str(), String(_configInputs[i]).c_str());

            serialInfo(String("[" + inTopic + "] " + _ioPreviousState[i] + " => " + inputsCurrent[i]).c_str());
          }
          _ioPreviousState[i] = inputsCurrent[i];
        }
      }
      _ioLastPoll = _now;
    }
  }

  return true;
}

bool Opta::ioGetDigitalInput(size_t index) {
  if (index < boardGetInputsNum() && (_configInputs[index] != IoType::Analog)) {
    return digitalRead(BoardInputs[index]) == 1;
  }

  return false;
}

float Opta::ioGetAnalogInput(size_t index) {
  if (index < boardGetInputsNum() && (_configInputs[index] != IoType::Analog)) {
    return analogRead(BoardInputs[index]) * (3.249 / ((1 << IoResolution) - 1)) / 0.3034;
  }

  return 0;
}

void Opta::ioSetDigitalOuput(size_t index, bool on) {
  if (index < boardGetOutputsNum()) {
    digitalWrite(BoardOutputs[index], on ? 1 : 0);
  }
}

/**
 * Network
 */

bool Opta::networkSetup() {
  serialLine(F("Configuring network"));

  if (_boardType == BoardType::Wifi && _configNetworkWifi && _configNetworkSsid != "" && _configNetworkPassword != "") {
    serialInfo(F("as Wifi standard network"));
    _networkSelected = NetworkType::Standard;

    if (WiFi.status() == WL_NO_MODULE) {
      // Ko
      return stop(F("Communication with WiFi module failed"));
    }

    networkConnectStandard();
  } else if (_boardType == BoardType::Wifi && _configNetworkWifi) {
    serialInfo(F("as Wifi Access Point network"));
    _networkSelected = NetworkType::AccessPoint;

    if (WiFi.status() == WL_NO_MODULE) {
      // Ok
      return stop(F("Communication with WiFi module failed"));
    }

    String netApSsid = "opta2iot" + _configDeviceId;
    String netApPass = "opta2iot";
    char ssid[32];
    char pass[32];
    netApSsid.toCharArray(ssid, sizeof(ssid));
    netApPass.toCharArray(pass, sizeof(pass));

    serialInfo("using SSID '" + netApSsid + "' and password '" + netApPass + "' and IP " + _configNetworkIp);

    WiFi.config(networkParseIp(_configNetworkIp));

    ledSetFreeze(true);
    int ret = WiFi.beginAP(ssid, pass);

    if (ret != WL_AP_LISTENING) {
      // Ko
      return stop(F("Failed to create Wifi Access Point"));
    } else {
      serialInfo(F("Wifi access point listening"));
      _networkConnected = true;
    }
    ledSetFreeze(false);
  } else {
    serialInfo(F("as Ethernet network"));
    _networkSelected = NetworkType::Rj45;

    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      // Ko
      return stop(F("Communication with Ethernet module failed"));
    }

    networkConnectRj45();
  }

  delay(1000);

  if (_networkConnected && _configNetworkDhcp) {
    serialInfo("DHCP attributed IP is " + networkLocalIp().toString());
  }

  return true;
}

bool Opta::networkLoop() {
  if (_networkSelected == NetworkType::Rj45) {
    // mauvaise condition
    if (!_networkConnected && (_networkLastRetry == 0 || _now - _networkLastRetry > (NetworkRetryDelay * 1000))) {
      if (_networkConnected) {
        _networkLastRetry = _now;
        Ethernet.maintain();
      } else if (Ethernet.linkStatus() == LinkON) {
        networkConnectRj45();
      }
    }
    if (!_networkConnected && Ethernet.linkStatus() == LinkON) {
      serialInfo(F("Ethernet cable connected"));
      _networkConnected = true;
    }
    if (_networkConnected && Ethernet.linkStatus() != LinkON) {
      serialWarn(F("Ethernet cable disconnected"));
      _networkConnected = false;
    }
    if (_networkConnected && Ethernet.linkStatus() == LinkON) {
      _networkLastRetry = 0;
      _networkConnected = true;
    }
  } else if (_networkSelected == NetworkType::Standard) {
    if (!_networkConnected && (_networkLastRetry == 0 || _now - _networkLastRetry > (NetworkRetryDelay * 1000))) {
      _networkLastRetry = _now;
      networkConnectStandard();
    }
  } else if (_networkSelected == NetworkType::AccessPoint) {
    if (_networkAccessPointStatus != WiFi.status()) {
      _networkAccessPointStatus = WiFi.status();

      if (_networkAccessPointStatus == WL_AP_CONNECTED) {
        serialInfo(F("Device connected to Access Point"));
      } else if (_networkAccessPointFirstLoop) {  // do not display message on startup
        _networkAccessPointFirstLoop = false;
      } else {
        serialWarn(F("Device disconnected from Access Point"));
      }
    }
  }

  return true;
}

IPAddress Opta::networkParseIp(const String &ip) {
  unsigned int res[4];
  sscanf(ip.c_str(), "%u.%u.%u.%u", &res[0], &res[1], &res[2], &res[3]);
  IPAddress ret(res[0], res[1], res[2], res[3]);
  return ret;
}

IPAddress Opta::networkLocalIp() {
  return _networkSelected == NetworkType::Rj45 ? Ethernet.localIP() : WiFi.localIP();
}

bool Opta::networkIsConnected() {
  return _networkConnected;
}
bool Opta::networkIsAccessPoint() {
  return _networkSelected == NetworkType::AccessPoint;
}
bool Opta::networkIsStandard() {
  return _networkSelected == NetworkType::Standard;
}
bool Opta::networkIsRj45() {
  return _networkSelected == NetworkType::Rj45;
}

void Opta::networkConnectRj45() {
  serialLine(F("Connecting Ethernet network"));

  int ret = 0;
  ledSetFreeze(true);
  if (_configNetworkDhcp) {
    serialInfo(F("using DHCP"));
    ret = Ethernet.begin();  // If failed this can take 1 minute long...
  } else {
    serialInfo(F("using static IP"));
    ret = Ethernet.begin(networkParseIp(_configNetworkIp));  // If failed this can take 1 minute long...
  }
  ledSetFreeze(false);

  if (ret == 0) {
    _networkConnected = false;
    serialWarn(F("Network connection failed."));
    if (Ethernet.linkStatus() == LinkOFF) {
      serialWarn(F("Ethernet cable not connected."));
    }
  } else {
    _networkConnected = true;
    serialInfo("Network connected with IP " + networkLocalIp().toString());
  }
}

void Opta::networkConnectStandard() {
  serialLine(F("Connecting Wifi Standard network"));

  String netApSsid = _configNetworkSsid;
  String netApPass = _configNetworkPassword;
  char ssid[32];
  char pass[32];
  netApSsid.toCharArray(ssid, sizeof(ssid));
  netApPass.toCharArray(pass, sizeof(pass));

  serialInfo("using SSID '" + netApSsid + "' and password '" + netApPass + "'");
  if (_configNetworkDhcp) {
    serialInfo(F("using DHCP"));
  } else {
    serialInfo(F("using static IP"));
    WiFi.config(networkParseIp(_configNetworkIp));
  }

  ledSetFreeze(true);
  int ret = WiFi.begin(ssid, pass);
  ledSetFreeze(false);

  if (ret != WL_CONNECTED) {
    serialWarn(F("Failed to connect Wifi"));
    _networkConnected = false;
  } else {
    serialInfo(F("Wifi connected"));
    _networkConnected = true;
  }
}

/**
 * Time
 */

bool Opta::timeSetup() {
  serialLine(F("Configuring time"));

  timeUpdate();

  return true;
}

bool Opta::timeLoop(bool startBenchmark) {
  // If time update failed during setup, try every hour until success
  if (!_timeUpdated && ((_now - 3600000) < _timeLastUpdate)) {
    _timeLastUpdate = _now;
    timeUpdate();
  }

  // Loop benchmark
  if (startBenchmark) {
    serialLine(F("Getting loop time"));

    _timeBenchmarkTime = _now;
    _timeBenchmarkCount = _timeBenchmarkRepeat = _timeBenchmarkSum = 0;
  } else if (_timeBenchmarkTime > 0) {
    _timeBenchmarkCount++;
    if (_now - _timeBenchmarkTime > 1000) {
      serialInfo(String(_timeBenchmarkCount) + " loops per second");

      if (_timeBenchmarkRepeat < 10) {
        _timeBenchmarkTime = _now;
        _timeBenchmarkSum += _timeBenchmarkCount;
        _timeBenchmarkCount = 0;
        _timeBenchmarkRepeat++;
      } else {
        _timeBenchmarkTime = 0;

        serialInfo("Average of " + String(_timeBenchmarkSum / 10) + " loops per second");
      }
    }
  }

  return true;
}

void Opta::timeUpdate() {
  if (_networkConnected && (_networkSelected != NetworkType::AccessPoint)) {
    serialLine(F("Updating local time"));

    ledSetFreeze(true);
    if (_networkSelected == NetworkType::Standard) {
      WiFiUDP wifiUdpClient;
      NTPClient timeClient(wifiUdpClient, TimeServer, _configTimeOffset * 3600, 0);
      timeClient.begin();
      if (!timeClient.update()) {
        serialWarn(F("Failed to update local time"));
      } else {
        const unsigned long epoch = timeClient.getEpochTime();
        set_time(epoch);
        serialInfo("Time set to " + timeClient.getFormattedTime());
        _timeUpdated = true;
      }
    } else if (_networkSelected == NetworkType::Rj45) {
      EthernetUDP ethernetUdpClient;
      NTPClient timeClient(ethernetUdpClient, TimeServer, _configTimeOffset * 3600, 0);
      timeClient.begin();
      if (!timeClient.update()) {
        serialWarn(F("Failed to update local time"));
      } else {
        const unsigned long epoch = timeClient.getEpochTime();
        set_time(epoch);
        serialInfo("Time set to " + timeClient.getFormattedTime());
        _timeUpdated = true;
      }
    }
    ledSetFreeze(false);
  }
}

String Opta::timeGet() {
  char buffer[32];
  tm t;
  _rtc_localtime(time(NULL), &t, RTC_FULL_LEAP_YEAR_SUPPORT);
  strftime(buffer, 32, "%k:%M:%S", &t);
  return String(buffer);
}

/*
 * MQTT
 */

bool Opta::mqttSetup() {
  serialLine(F("Configuring MQTT client"));

  serialInfo("using server " + _configMqttIp + ":" + String(_configMqttPort));

  ledSetFreeze(true);
  if (_networkSelected == NetworkType::Rj45) {
    MqttClient tempMqttClient(mqttEthernetClient);
    mqttClient = tempMqttClient;
  } else {
    MqttClient tempMqttClient(mqttWifiClient);
    mqttClient = tempMqttClient;
  }
  ledSetFreeze(false);

  mqttConnect();

  return true;
}

bool Opta::mqttLoop() {
  mqttConnect();
  if (_mqttConnected && mqttClient.parseMessage()) {
    String mqttTopic = mqttClient.messageTopic();
    char mqttBuffer[20]; // limit received message size
    int mqttLength = 0;
    while (mqttClient.available()) {
      mqttBuffer[mqttLength] = (char)mqttClient.read();
      if (mqttLength < (int)sizeof(mqttBuffer) - 1) {
        mqttLength++;
      }
    }
    String mqttString = String(mqttBuffer);
    mqttReceive(mqttTopic, mqttString);
  }

  return true;
}

bool Opta::mqttIsConnected() {
  return _mqttConnected;
}

void Opta::mqttConnect() {
  if (!_networkConnected || (_networkSelected == NetworkType::AccessPoint)) {
    return;
  }
  if (mqttClient.connected()) {
    _mqttConnected = true;
    _mqttLastRetry = 0;
    return;
  }

  if (_mqttLastRetry > 0 && ((_now - _mqttLastRetry) < (NetworkRetryDelay * 1000))) {  // retry every x seconds
    return;
  }

  _mqttConnected = false;
  _mqttLastRetry = millis();
  serialLine(F("Connecting to MQTT broker"));

  ledSetFreeze(true);
  mqttClient.setId(_configDeviceId);
  mqttClient.setUsernamePassword(_configMqttUser, _configMqttPassword);
  if (!mqttClient.connect(_configMqttIp.c_str(), _configMqttPort)) {
    serialWarn(F("Failed to connect to MQTT broker"));
    ledSetFreeze(false);
    return;
  }
  ledSetFreeze(false);

  serialInfo(F("MQTT broker found"));
  _mqttConnected = true;

  String topic = _configMqttBase + _configDeviceId + "/device/get";  // command for device information
  mqttClient.subscribe(topic);
  serialInfo("Subcribed to " + topic);

  for (size_t i = 0; i < boardGetOutputsNum(); i++) {
    String topic = _configMqttBase + _configDeviceId + "/O" + String(i + 1);  // command for outputs
    mqttClient.subscribe(topic);
    serialInfo("Subcribed to " + topic);
  }

  mqttPublishDevice();
}

bool Opta::mqttSubscribe(String topic) {
  if(_mqttConnected) {
    mqttClient.subscribe(topic);

    return true;
  }

  return false;
}

bool Opta::mqttPublish(String topic, String message) {
  if(_mqttConnected) {
    mqttClient.beginMessage(topic);
    mqttClient.print(message);
    mqttClient.endMessage();

    return true;
  }

  return false;
}

void Opta::mqttReceive(String &topic, String &payload) {
  serialLine("Receiving MQTT command " + topic + ": " + payload);

  String match = _configMqttBase + _configDeviceId + "/device/get";
  if (topic == match) {
    mqttPublishDevice();
  }

  for (size_t i = 0; i < boardGetInputsNum(); i++) {
    String match = _configMqttBase + _configDeviceId + "/O" + String(i + 1);
    if (topic == match) {
      serialInfo("Setting output " + String(i + 1) + " to " + payload);

      digitalWrite(BoardOutputs[i], payload.toInt());
      digitalWrite(BoardOutputsLeds[i], payload.toInt());
    }
  }
}

void Opta::mqttPublishDevice() {
  if (_networkConnected && _mqttConnected) {
    serialLine(F("Publishing device informations to MQTT"));

    String rootTopic = _configMqttBase + _configDeviceId;

    mqttPublish(String(rootTopic + "/device/type").c_str(), String(_boardName).c_str());
    mqttPublish(String(rootTopic + "/device/ip").c_str(), networkLocalIp().toString());
    mqttPublish(String(rootTopic + "/device/revision").c_str(), String(Revision).c_str());

    /*
    for (size_t i = 0; i < boardGetOutputsNum(); i++) {
      // for now all output are digital
      mqttClient.publish(String(rootTopic + "/O" + (i + 1)).c_str(), String(digitalRead(BoardOutputs[i])).c_str());
    }
    //*/
  }
}

void Opta::mqttPublishInputs() {
  if (_networkConnected && _mqttConnected) {
    serialLine(F("Publishing inputs informations to MQTT"));

    String rootTopic = _configMqttBase + _configDeviceId + "/";
    for (size_t i = 0; i < boardGetInputsNum(); i++) {
      String inTopic = "I" + String(i + 1) + "/";
      if (_configInputs[i] == IoType::Analog) {
        float value = analogRead(BoardInputs[i]) * (3.249 / ((1 << IoResolution) - 1)) / 0.3034;
        char buffer[10];
        snprintf(buffer, sizeof(buffer), "%0.2f", value);
        mqttPublish(String(rootTopic + inTopic + "val").c_str(), buffer);
        mqttPublish(String(rootTopic + inTopic + "type").c_str(), String(_configInputs[i]).c_str());
      } else {
        mqttPublish(String(rootTopic + inTopic + "val").c_str(), String(digitalRead(BoardInputs[i])).c_str());
        mqttPublish(String(rootTopic + inTopic + "type").c_str(), String(_configInputs[i]).c_str());
      }
    }
  }
}

/*
 * Web
 */

bool Opta::webSetup() {
  serialLine(F("Configuring web server"));

  ledSetFreeze(true);
  webEthernetServer = EthernetServer(80);
  webWifiServer = WiFiServer(80);
  if (_networkSelected == NetworkType::Rj45) {
    serialInfo("as Ethernet Web server");
    webEthernetServer.begin();
  } else {
    serialInfo("as Wifi Web server");
    webWifiServer.begin();
  }
  ledSetFreeze(false);

  return true;
}

bool Opta::webLoop() {
  if (_networkConnected && _odd) {  // _odd: leave place for other things
    Client *webClient = nullptr;
    if (_networkSelected == NetworkType::Rj45) {
      EthernetClient webEthernetClient = webEthernetServer.accept();
      webClient = &webEthernetClient;
      if (webClient) {
        webConnect(webClient);
      }
    } else {
      WiFiClient webWifiClient = webWifiServer.accept();
      webClient = &webWifiClient;
      if (webClient) {
        webConnect(webClient);
      }
    }
  }

  return true;
}

void Opta::webConnect(Client *&client) {
  client->setTimeout(5000);
  String webConnectRequest = "";
  int webConnectLength = 0;
  bool webConnectAuth = false;
  bool webConnectBlank = true;
  char webConnectBuffer[100];
  //memset(webConnectBuffer, 0, sizeof(webConnectBuffer));

  while (client->connected()) {
    // NOTE: Try to kill Wifi waiting client
    if (!client->available()) {
      for (size_t i = 0; i < 250; i++) {
        delay(1);
        if (client->available()) {
          break;
        }
      }
      if (!client->available()) {
        client->stop();
        break;
      }
    }

    if (client->available()) {  // NOTE: Wifi server client "available" sometimes freeze too long for the loop.
      // Read client request
      char webChar = client->read();
      webConnectBuffer[webConnectLength] = webChar;
      if (webConnectLength < (int)sizeof(webConnectBuffer) - 1) {
        webConnectLength++;
      }

      if (webChar == '\n' && webConnectBlank) {
        if (webConnectAuth) {
          if (!webConnectRequest) {
            // grab end of request
            webConnectRequest = client->readStringUntil('\r');
            //webConnectRequest = client->readString();
          }

          client->flush();  // nothing more to read

          if (webConnectRequest.startsWith("GET /style.css")) {
            webSendStyle(client);
          } else if (webConnectRequest.startsWith("POST /form")) {
            webReceiveConfig(client);
          } else if (webConnectRequest.startsWith("GET /publish ")) {
            webReceivePublish(client);
          } else if (webConnectRequest.startsWith("GET /config ")) {
            webSendConfig(client);
          } else if (webConnectRequest.startsWith("GET /data ")) {
            webSendData(client);
          } else if (webConnectRequest.startsWith("GET /device ")) {
            webSendDevice(client);
          } else if (webConnectRequest.startsWith("GET / ")) {
            webSendHome(client);
          } else if (webConnectRequest.startsWith("GET /favicon.ico")) {
            webSendFavicon(client);
          } else {
            webSendError(client);
          }
        } else {
          webSendAuth(client);
        }
        client->stop();
        break;
      }

      if (webChar == '\n') {
        webConnectBlank = true;

        // prepare basic auth ! I s*** at this
        String inputString = _configDeviceUser + ":" + _configDevicePassword;
        char inputChar[32];
        inputString.toCharArray(inputChar, sizeof(inputChar));
        unsigned char *unsInputChar = (unsigned char *)inputChar;
        int inputLength = strlen((char *)unsInputChar);
        //int encodedLength = encode_base64_length(inputLength) + 1;
        //unsigned char encodedString[encodedLength];
        unsigned char encodedString[32];
        encode_base64(unsInputChar, inputLength, encodedString);

        // check basic auth
        if (strstr(webConnectBuffer, "Authorization: Basic ") && strstr(webConnectBuffer, (char *)encodedString)) {
          webConnectAuth = true;
        }

        // if web line buffer is the request
        if (strstr(webConnectBuffer, "GET /") || strstr(webConnectBuffer, "POST /")) {
          webConnectRequest = webConnectBuffer;
        }

        //memset(webConnectBuffer, 0, sizeof(webConnectBuffer));
        webConnectLength = 0;
      } else if (webChar != '\r') {
        webConnectBlank = false;
      }
    }
  }
  client->stop();
}

void Opta::webSendFavicon(Client *&client) {
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

void Opta::webSendStyle(Client *&client) {
  client->println("HTTP/1.1 200 OK");
  client->println("Content-Type: text/css");
  client->println("Connection: close");
  client->println();
  client->println(htmlStyle);
}

void Opta::webSendAuth(Client *&client) {
  client->println("HTTP/1.1 401 Authorization Required");
  client->println("WWW-Authenticate: Basic realm=\"Secure Area\"");
  client->println("Content-Type: text/html");
  client->println("Connnection: close");
  client->println();
  client->println(htmlAuth);
}

void Opta::webSendError(Client *&client) {
  client->println("HTTP/1.1 404 Not Found");
  client->println("Content-Type: text/html");
  client->println("Connnection: close");
  client->println();
  client->println(htmlError);
}

void Opta::webSendHome(Client *&client) {
  client->println("HTTP/1.1 200 OK");
  client->println("Content-Type: text/html");
  client->println("Connection: close");
  client->println();
  client->println(htmlHome);
}

void Opta::webSendDevice(Client *&client) {
  client->println("HTTP/1.1 200 OK");
  client->println("Content-Type: text/html");
  client->println("Connection: close");
  client->println();
  client->println(htmlDevice);
}

void Opta::webSendConfig(Client *&client) {
  client->println("HTTP/1.1 200 OK");
  client->println("Content-Type: application/json");
  client->println("Connection: close");
  client->println();
  client->println(configWriteToJson(true));
}

void Opta::webSendData(Client *&client) {
  JsonDocument doc;
  doc["deviceId"] = _configDeviceId;
  doc["version"] = version();
  doc["mqttConnected"] = _mqttConnected;
  doc["time"] = timeGet();
  doc["gmt"] = _configTimeOffset;

  // Digital Inputs
  JsonObject inputsObject = doc["inputs"].to<JsonObject>();
  for (size_t i = 0; i < boardGetInputsNum(); i++) {
    String name = "I" + String(i + 1);
    JsonObject obj = inputsObject[name].to<JsonObject>();
    obj["type"] = _configInputs[i];
    if (_configInputs[i] == IoType::Analog) {
      obj["value"] = analogRead(BoardInputs[i]) * (3.249 / ((1 << IoResolution) - 1)) / 0.3034;
    } else {
      obj["value"] = digitalRead(BoardInputs[i]);
    }
  }
  JsonObject outputsObj = doc["outputs"].to<JsonObject>();
  for (size_t i = 0; i < boardGetOutputsNum(); i++) {
    String name = "O" + String(i + 1);
    outputsObj[name] = digitalRead(BoardOutputs[i]);
  }
  String jsonString;
  serializeJson(doc, jsonString);

  client->println("HTTP/1.1 200 OK");
  client->println("Content-Type: application/json");
  client->println("Connection: close");
  client->println();
  client->println(jsonString);
}

void Opta::webReceiveConfig(Client *&client) {
  serialLine(F("Parsing received configuration"));

  bool isValid = true;
  String jsonString = "";

  String oldDevicePassword = _configDevicePassword;
  String oldNetPassword = _configNetworkPassword;
  String oldMqttPassword = _configMqttPassword;

  while (client->available()) {
    String line = client->readStringUntil('\n');  // Read line-by-line

    if (line == "\r") {  // Detect the end of headers (an empty line)
      isValid = false;
      break;
    }

    jsonString += line;
  }

  if (!isValid || configReadFromJson(jsonString.c_str(), jsonString.length()) < 1) {
    serialWarn("Failed to load configuration from response");
    isValid = false;
  } else {
    if (_configDeviceId == "") {  // device ID must be set
      serialWarn("Missing device ID");
      isValid = false;
    }
    if (_configDeviceUser == "") {  // device user must be set
      serialWarn("Missing device user");
      isValid = false;
    }
    if (_configDevicePassword == "") {  // get old device password if none set
      serialInfo("Get previous device password");
      configSetDevicePassword(oldDevicePassword);
    }
    int offset = _configTimeOffset;
    if (offset > 24 || offset < -24) {  // time offset must be in this day
      setTimeOffset(0);
    }
    if (_configNetworkPassword == "" && _configNetworkSsid != "") {  // get old wifi password if none set
      serialInfo("Get previous Wifi password");
      configSetNetworkPassword(oldNetPassword);
    }
    if (_configMqttPassword == "" && _configMqttUser != "") {  // get old mqtt password if none set
      serialInfo("Get previous MQTT password");
      configSetMqttPassword(oldMqttPassword);
    }
  }

  if (isValid) {
    client->println("HTTP/1.1 200 OK");
    client->println("Content-Type: application/json");
    client->println("Connection: close");
    client->println();
    client->println("{\"status\":\"success\",\"message\":\"Configuration updated\"}");
    client->stop();

    configWriteToFile();
    delay(1000);
    reboot();
  } else {
    client->println("HTTP/1.1 403 FORBIDDEN");
    client->println("Content-Type: application/json");
    client->println("Connection: close");
    client->println();
    client->println("{\"status\":\"error\",\"message\":\"Configuration not updated\"}");
  }
}

void Opta::webReceivePublish(Client *&client) {
  mqttPublishDevice();
  mqttPublishInputs();

  client->println("HTTP/1.1 200 OK");
  client->println("Content-Type: application/json");
  client->println("Connection: close");
  client->println();
  client->println("{\"status\":\"success\",\"message\":\"Informations published\"}");
}


}  // namespace