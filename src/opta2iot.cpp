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
#include <drivers/Watchdog.h>
#include "KVStore.h"
#include "kvstore_global_api.h"
#include <Ethernet.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <mbed_mktime.h>
#include <base64.hpp>
#include "define.h"
#include "opta2iot.h"
#include "label.h"
#include "html.h"
#include "certificates.h"

// Opta board info
#include "opta_info.h"  //mbed
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

uint32_t Opta::now(bool now) {
  if (now) {
    _now = millis();
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
         && watchdogSetup()
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
         && watchdogLoop()
         && endLoop();
}

bool Opta::startSetup() {
  now(true);

  return true;
}

bool Opta::endSetup() {
  float last = floor((millis() - now()) / 1000);
  serialLine(label_setup_end + String((int)last) + " secondes");

  return true;
}

bool Opta::startLoop() {
  now(true);

  return running();
}

bool Opta::endLoop() {
  odd(true);

  return true;
}

bool Opta::stop(String reason) {
  return stop(reason.c_str());
}

bool Opta::stop(const char *reason) {
  serialWarn(reason);
  _stop = true;
  ledSetGreen(true);
  ledSetRed(false);
  ledSetRed(false);

  return false;
}

bool Opta::running() {
  return !_stop;
}

bool Opta::odd(bool change) {
  if (change) {
    _odd = !_odd;
  }

  return _odd;
}

void Opta::print(String str) {
  print(str.c_str());
}
void Opta::print(const char *str) {
  Serial.print(str);
}

void Opta::format() {
  flashFormat(true);
}

void Opta::reset() {
  serialInfo(label_main_reset);

  kv_reset("/kv/");
  delay(10);
  configReadFromDefault();
  delay(10);
  configWriteToFile();
  delay(10);
}

void Opta::reboot() {
  serialLine(label_main_reboot);

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
  if (!_threadStarted) {
    serialLine(label_main_thread);

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

uint32_t Opta::connectTimeout() {
  uint32_t timeout = 10;
  if ((ConnectTimeout > 0) && (ConnectTimeout < 120)) {
    timeout = ConnectTimeout;
  }

  return timeout * 1000;
}

/*
 * Watchdog
 */

bool Opta::watchdogSetup() {
  serialLine(label_watchdog_start);

  _watchdogStarted = true;
  watchdogMin();

  return true;
}

bool Opta::watchdogLoop() {
  watchdogPing();

  return true;
}

bool Opta::watchdogStarted() {
  return _watchdogStarted;
}

void Opta::watchdogMin() {
  if (watchdogStarted()) {
    uint32_t timeout = 100;
    if ((WatchdogTimeout > 0) && (WatchdogTimeout < mbed::Watchdog::get_instance().get_max_timeout())) {
      timeout = WatchdogTimeout;
    }
    mbed::Watchdog::get_instance().start(timeout);
  }
}

void Opta::watchdogMax() {
  if (watchdogStarted()) {
    mbed::Watchdog::get_instance().start(mbed::Watchdog::get_instance().get_max_timeout());
  }
}

void Opta::watchdogPing() {
  mbed::Watchdog::get_instance().kick();
}

uint32_t Opta::watchdogTimeout() {
  if (watchdogStarted()) {
    return mbed::Watchdog::get_instance().get_timeout();
  }

  return 0;
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

  print(label_serial_setup);

  return true;
}

bool Opta::serialLoop() {
  if (serialIncoming()) {
    String message = serialReceived();
    if (message.equals("loop")) {
      timeLoop(true);
    }
    if (message.equals("ip")) {
      serialLine(label_serial_cmd_ip);
      serialInfo(networkLocalIp().toString());
    }
    if (message.equals("config")) {
      serialInfo(configWriteToJson(false));
    }
    if (message.equals("time")) {
      serialLine(label_serial_cmd_time);
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
      serialWarn(label_serial_reboot);
    }
    if (message.equals("reboot")) {
      reboot();
    }
    if (message.equals("dhcp")) {
      configSetNetworkDhcp(configGetNetworkDhcp() ? false : true);
      configWriteToFile();
      serialWarn(label_serial_reboot);
    }
    if (message.equals("wifi")) {
      configSetNetworkWifi(configGetNetworkWifi() ? false : true);
      configWriteToFile();
      serialWarn(label_serial_reboot);
    }
    if (message.equals("publish")) {
      mqttPublishDevice();
      mqttPublishInputs();
    }
  }

  return true;
}

void Opta::serialLine(String str) {
  print(label_serial_line + str + "\n");
}
void Opta::serialLine(const char *str) {
  serialLine(String(str));
}

void Opta::serialInfo(String str) {
  if (SerialVerbose) {
    print(label_serial_info + str + "\n");
  }
}
void Opta::serialInfo(const char *str) {
  serialInfo(String(str));
}

void Opta::serialWarn(String str) {
  print(label_serial_warn + str + "\n");
}
void Opta::serialWarn(const char *str) {
  serialWarn(String(str));
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
          serialLine(label_serial_receive + String(_serialIncoming));
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
  serialLine(label_board_setup);

  OptaBoardInfo *info = boardInfo();
  if (info->magic == 0xB5) {
    if (info->_board_functionalities.ethernet == 1) {
      boardSetType(BoardType::BoardLite);
      //MacEthernet = String(info->mac_address[0], HEX) + ":" + String(info->mac_address[1], HEX) + ":" + String(info->mac_address[2], HEX) + ":" + String(info->mac_address[3], HEX) + ":" + String(info->mac_address[4], HEX) + ":" + String(info->mac_address[5], HEX);
    }
    if (info->_board_functionalities.rs485 == 1) {
      boardSetType(BoardType::BoardRs485);
    }
    if (info->_board_functionalities.wifi == 1) {
      boardSetType(BoardType::BoardWifi);
      //MacWifi = String(info->mac_address_2[0], HEX) + ":" + String(info->mac_address_2[1], HEX) + ":" + String(info->mac_address_2[2], HEX) + ":" + String(info->mac_address_2[3], HEX) + ":" + String(info->mac_address_2[4], HEX) + ":" + String(info->mac_address_2[5], HEX);
    }
  }

  if (boardIsNone()) {
    // Ko
    return stop(label_board_error);
  }
  serialInfo(label_board_name + boardGetName());

  return true;
}

bool Opta::boardSetType(BoardType type) {
  _boardType = type;

  return true;
}

bool Opta::boardIsNone() {
  return _boardType == BoardType::BoardNone;
}

bool Opta::boardIsLite() {
  return _boardType == BoardType::BoardLite;
}

bool Opta::boardIsRs485() {
  return _boardType == BoardType::BoardRs485;
}

bool Opta::boardIsWifi() {
  return _boardType == BoardType::BoardWifi;
}

String Opta::boardGetName() {
  String name;
  switch(_boardType) {
    case BoardType::BoardLite:
      name = label_board_name_lite;
      break;
    case BoardType::BoardRs485:
      name = label_board_name_rs485;
      break;
    case BoardType::BoardWifi:
      name = label_board_name_wifi;
      break;
    default:
      name = label_board_name_none;
    }

    return name;
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
  serialLine(label_flash_setup);

  _flashRoot = mbed::BlockDevice::get_default_instance();
  if (_flashRoot->init() != mbed::BD_ERROR_OK) {
    serialWarn(label_flash_init_error);
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
  serialInfo((noWifi ? label_flash_missing : label_flash_existing) + String("Wifi"));

  mbed::MBRBlockDevice ota_data(_flashRoot, 2);
  mbed::FATFileSystem ota_data_fs("fs");
  bool noOta = ota_data_fs.mount(&ota_data) != 0;
  serialInfo((noOta ? label_flash_missing : label_flash_existing) + String("OTA"));

  mbed::MBRBlockDevice kvstore_data(_flashRoot, 3);
  // do not touch this one

  mbed::MBRBlockDevice user_data(_flashRoot, 4);
  mbed::FATFileSystem user_data_fs("fs");
  bool noUser = user_data_fs.mount(&user_data) != 0;
  serialInfo((noUser ? label_flash_missing : label_flash_existing) + String("User"));

  bool perform = force || noWifi || noOta || noUser;

  if (perform) {
    serialLine(label_flash_erase_wait);
    _flashRoot->erase(0x0, _flashRoot->size());
    serialInfo(label_flash_erase_done);
  }

  mbed::MBRBlockDevice::partition(_flashRoot, 1, 0x0B, 0, 1 * 1024 * 1024);
  mbed::MBRBlockDevice::partition(_flashRoot, 2, 0x0B, 1 * 1024 * 1024, 6 * 1024 * 1024);
  mbed::MBRBlockDevice::partition(_flashRoot, 3, 0x0B, 6 * 1024 * 1024, 7 * 1024 * 1024);
  mbed::MBRBlockDevice::partition(_flashRoot, 4, 0x0B, 7 * 1024 * 1024, 14 * 1024 * 1024);
  // use space from 15.5MB to 16 MB for another fw, memory mapped

  if (force || noWifi) {
    serialLine(label_flash_format + String("Wifi"));

    wifi_data_fs.unmount();
    if (wifi_data_fs.reformat(&wifi_data) != 0) {  // not used yet
      serialWarn(label_flash_format_error);
      return false;
    }
    watchdogPing();

    if (!flashWiFiFirmwareAndCertificates() || !flashWiFiFirmwareMapped()) {
      return false;
    }
  }

  if (force || noOta) {
    serialLine(label_flash_format + String("OTA"));

    ota_data_fs.unmount();
    if (ota_data_fs.reformat(&ota_data) != 0) {
      serialWarn(label_flash_format_error);
      return false;
    }
    watchdogPing();
  }

  if (force || noUser) {
    serialLine(label_flash_format + String("User"));

    user_data_fs.unmount();
    if (user_data_fs.reformat(&user_data) != 0) {
      serialWarn(label_flash_format_error);
      return false;
    }
    watchdogPing();
  }

  return true;
}

bool Opta::flashWiFiFirmwareAndCertificates() {
  FILE *fp = fopen("/wlan/4343WA1.BIN", "wb");
  uint32_t chunk_size = 1024;
  uint32_t byte_count = 0;

  serialLine(label_flash_firmware);
  serialProgress(byte_count, file_size, 10, true);
  while (byte_count < file_size) {
    if (byte_count + chunk_size > file_size)
      chunk_size = file_size - byte_count;
    int ret = fwrite(&wifi_firmware_image_data[byte_count], chunk_size, 1, fp);
    if (ret != 1) {
      serialWarn(label_flash_firmware_error);

      return false;
    }
    byte_count += chunk_size;
    serialProgress(byte_count, file_size, 10, false);
    watchdogPing();
  }
  fclose(fp);

  fp = fopen("/wlan/cacert.pem", "wb");
  chunk_size = 128;
  byte_count = 0;

  serialLine(label_flash_certificate);
  serialProgress(byte_count, cacert_pem_len, 10, true);

  while (byte_count < cacert_pem_len) {
    if (byte_count + chunk_size > cacert_pem_len)
      chunk_size = cacert_pem_len - byte_count;
    int ret = fwrite(&cacert_pem[byte_count], chunk_size, 1, fp);
    if (ret != 1) {
      serialWarn(label_flash_certificate_error);

      return false;
    }
    byte_count += chunk_size;
    serialProgress(byte_count, cacert_pem_len, 10, false);
    watchdogPing();
  }

  return true;
}

bool Opta::flashWiFiFirmwareMapped() {
  uint32_t chunk_size = 1024;
  uint32_t byte_count = 0;
  const uint32_t offset = 15 * 1024 * 1024 + 1024 * 512;

  serialLine(label_flash_mapped);
  serialProgress(byte_count, file_size, 10, true);

  while (byte_count < file_size) {
    if (byte_count + chunk_size > file_size)
      chunk_size = file_size - byte_count;
    int ret = _flashRoot->program(wifi_firmware_image_data, offset + byte_count, chunk_size);
    if (ret != 0) {
      serialWarn(label_flash_mapped_error);

      return false;
    }
    byte_count += chunk_size;
    serialProgress(byte_count, file_size, 10, false);
    watchdogPing();
  }

  return true;
}

/*
 * User LEDs
 */

bool Opta::ledSetup() {
  serialLine(label_led_setup);

  serialInfo(label_led_green + String(BoardUserLeds[0]));
  pinMode(BoardUserLeds[0], OUTPUT);
  serialInfo(label_led_red + String(BoardUserLeds[1]));
  pinMode(BoardUserLeds[1], OUTPUT);
  serialInfo(label_led_blue + String(BoardUserLeds[2]));
  pinMode(BoardUserLeds[2], OUTPUT);

  ledSetGreen(false);
  ledSetRed(false);
  ledSetBlue(false);

  return true;
}

bool Opta::ledLoop() {
  if (now() - _ledConnectionStart > 750) {
    // connections
    _ledConnectionState = !_ledConnectionState;
    _ledConnectionStart = now();

    ledSetRed(networkIsConnected() ? false : _ledConnectionState);
    ledSetGreen(networkIsConnected() && mqttIsConnected() ? _ledConnectionState : false);
    if (networkIsAccessPoint()) {
      ledSetBlue(_ledConnectionState);
    } else if (networkIsStandard()) {
      ledSetBlue(true);
    }
  }

  if (now() - _ledHeartbeatStart > 10000) {
    if (_ledHeartBeatStep == 0) {
      _ledHeartBeatStep = 1;
      _ledGreen = ledGetGreen();
      _ledRed = ledGetRed();
      ledSetGreen(false);
      ledSetRed(false);
    }
    if (_ledHeartBeatStep == 1 && now() - _ledHeartbeatStart > 10150) {
      _ledHeartBeatStep = 2;
      ledSetGreen(true);
      ledSetRed(true);
    }
    if (_ledHeartBeatStep == 2 && now() - _ledHeartbeatStart > 10200) {
      _ledHeartBeatStep = 3;
      ledSetGreen(false);
      ledSetRed(false);
    }
    if (_ledHeartBeatStep == 3 && now() - _ledHeartbeatStart > 10350) {
      _ledHeartBeatStep = 0;
      _ledHeartbeatStart = now();
      serialInfo(label_led_heartbeat + timeGet());
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
    watchdogMax();
    watchdogPing();

    _ledGreen = ledGetGreen();
    _ledRed = ledGetRed();
    //_ledBlue = ledGetBlue();

    ledSetGreen(true);
    ledSetRed(true);
    //ledSetBlue(false);
  } else {
    watchdogMin();
    watchdogPing();

    ledSetGreen(_ledGreen);
    ledSetRed(_ledRed);
    //ledSetBlue(_ledBlue);
  }
}

/**
 * User buttons
 */

bool Opta::buttonSetup() {
  serialLine(label_button_setup);

  serialInfo(label_button_user + String(BoardUserButtons[0]));
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
    if ((!networkIsConnected() || networkIsAccessPoint()) && (duration < 1000)) {
      configSetNetworkDhcp(configGetNetworkDhcp() ? false : true);
      configWriteToFile();
      reboot();
    }

    // if network not connected or as access point and button push bettwen 1s and 3s: switch WIFI mode in config and reboot
    if ((!networkIsConnected() || networkIsAccessPoint()) && (duration > 1000) && (duration < 3000)) {
      configSetNetworkWifi(configGetNetworkWifi() ? false : true);
      configWriteToFile();
      reboot();
    }

    // if network connected and mqtt connected and button push less than 1s : publish MQTT info
    if (networkIsConnected() && !networkIsAccessPoint() && mqttIsConnected() && (duration < 1000)) {
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
      _buttonStart = now();
    }
    _buttonDuration = now() - _buttonStart;

    return 0;
  } else if (_buttonStart > 0 && _buttonDuration > 0) {
    _buttonStart = 0;
    serialInfo(label_button_duration + String(_buttonDuration) + " milliseconds");

    return _buttonDuration;
  }

  return 0;
}

/*
 *Config
 */

bool Opta::configSetup() {
  serialLine(label_config_setup);

  if (configReadFromFile()) {
    serialWarn(label_config_hold);

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

void Opta::configSetTimeOffset(const int offset) {
  _configTimeOffset = offset;
}

String Opta::configGetNetworkIp() const {
  return _configNetworkIp;
}

void Opta::configSetNetworkIp(const String &ip) {
  _configNetworkIp = ip;
}

String Opta::configGetNetworkGateway() const {
  return _configNetworkGateway;
}

void Opta::configSetNetworkGateway(const String &ip) {
  _configNetworkGateway = ip;
}

String Opta::configGetNetworkSubnet() const {
  return _configNetworkSubnet;
}

void Opta::configSetNetworkSubnet(const String &ip) {
  _configNetworkSubnet = ip;
}

String Opta::configGetNetworkDns() const {
  return _configNetworkDns;
}

void Opta::configSetNetworkDns(const String &ip) {
  _configNetworkDns = ip;
}

bool Opta::configGetNetworkDhcp() const {
  return _configNetworkDhcp;
}

void Opta::configSetNetworkDhcp(const bool on) {
  serialInfo((on ? label_config_mode_enable : label_config_mode_disable) + String("DHCP"));
  _configNetworkDhcp = on;
}

bool Opta::configGetNetworkWifi() const {
  return _configNetworkWifi;
}

void Opta::configSetNetworkWifi(const bool on) {
  serialInfo((on ? label_config_mode_enable : label_config_mode_disable) + String("Wifi"));
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
  return IoType::IoAnalog;
}

bool Opta::configSetInputType(size_t index, byte type) {
  if (index < boardGetInputsNum() && (type == IoType::IoPulse || type == IoType::IoDigital || type == IoType::IoAnalog)) {
    _configInputs[index] = type;
    return true;
  }
  return false;
}

bool Opta::configReadFromJson(const char *buffer, size_t length) {
  serialInfo(label_config_json_read);

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, buffer, length);

  if (error) {
    serialWarn(label_config_json_read_error);

    return false;
  }

  if (doc["deviceId"].isNull()
      || doc["deviceUser"].isNull()
      || doc["devicePassword"].isNull()
      || doc["timeOffset"].isNull()
      || doc["netIp"].isNull()
      || doc["netGateway"].isNull()
      || doc["netSubnet"].isNull()
      || doc["netDns"].isNull()
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
    serialWarn(label_config_json_uncomplete);

    return false;
  }

  configSetDeviceId(doc["deviceId"].as<String>());
  configSetDeviceUser(doc["deviceUser"].as<String>());
  configSetDevicePassword(doc["devicePassword"].as<String>());
  configSetTimeOffset(doc["timeOffset"].as<int>());
  configSetNetworkIp(doc["netIp"].as<String>());
  configSetNetworkGateway(doc["netGateway"].as<String>());
  configSetNetworkSubnet(doc["netSubnet"].as<String>());
  configSetNetworkDns(doc["netDns"].as<String>());
  configSetNetworkDhcp(doc["netDhcp"].as<bool>());
  configSetNetworkWifi(doc["netWifi"].as<bool>());
  configSetNetworkSsid(doc["netSsid"].as<String>());
  configSetNetworkPassword(doc["netPassword"].as<String>());
  configSetMqttIp(doc["mqttIp"].as<String>());
  configSetMqttPort(doc["mqttPort"].as<int>());
  configSetMqttUser(doc["mqttUser"].as<String>());
  configSetMqttPassword(doc["mqttPassword"].as<String>());
  configSetMqttBase(doc["mqttBase"].as<String>());
  configSetMqttInterval(doc["mqttInterval"].as<int>() < 0 ? 0 : doc["mqttInterval"].as<int>());

  for (size_t i = 0; i < boardGetInputsNum(); ++i) {
    String pinName = "I" + String(i + 1);
    int pinType = doc["inputs"][pinName].as<int>();
    configSetInputType(i, (pinType == IoType::IoAnalog || pinType == IoType::IoDigital || pinType == IoType::IoPulse) ? pinType : IoType::IoAnalog);
  }

  return true;
}

String Opta::configWriteToJson(const bool nopass) {
  //serialInfo("Writing configuration to JSON");

  JsonDocument doc;

  doc["version"] = version();
  doc["deviceId"] = configGetDeviceId();
  doc["deviceUser"] = configGetDeviceUser();
  doc["devicePassword"] = nopass ? "" : configGetDevicePassword();
  doc["timeOffset"] = configGetTimeOffset();
  doc["netIp"] = configGetNetworkIp();
  doc["netGateway"] = configGetNetworkGateway();
  doc["netSubnet"] = configGetNetworkSubnet();
  doc["netDns"] = configGetNetworkDns();
  doc["netDhcp"] = configGetNetworkDhcp();
  doc["netWifi"] = configGetNetworkWifi();
  doc["netSsid"] = configGetNetworkSsid();
  doc["netPassword"] = nopass ? "" : configGetNetworkPassword();
  doc["mqttIp"] = configGetMqttIp();
  doc["mqttPort"] = configGetMqttPort();
  doc["mqttUser"] = configGetMqttUser();
  doc["mqttPassword"] = nopass ? "" : configGetMqttPassword();
  doc["mqttBase"] = configGetMqttBase();
  doc["mqttInterval"] = configGetMqttInterval();

  for (size_t i = 0; i < boardGetInputsNum(); ++i) {
    String pinName = "I" + String(i + 1);
    doc["inputs"][pinName] = configGetInputType(i);
  }

  String jsonString;
  serializeJson(doc, jsonString);

  return jsonString;
}

void Opta::configReadFromDefault() {
  serialInfo(label_config_default_read);

  configSetDeviceId(OPTA2IOT_DEVICE_ID);
  configSetDeviceUser(OPTA2IOT_DEVICE_USER);
  configSetDevicePassword(OPTA2IOT_DEVICE_PASSWORD);
  configSetTimeOffset(OPTA2IOT_TIME_OFFSET);

  configSetNetworkIp(OPTA2IOT_NET_IP);
  configSetNetworkGateway(OPTA2IOT_NET_GATEWAY);
  configSetNetworkSubnet(OPTA2IOT_NET_SUBNET);
  configSetNetworkDns(OPTA2IOT_NET_DNS);
  configSetNetworkDhcp(OPTA2IOT_NET_DHCP);
  configSetNetworkWifi(OPTA2IOT_NET_WIFI);
  configSetNetworkSsid(OPTA2IOT_NET_SSID);
  configSetNetworkPassword(OPTA2IOT_NET_PASSWORD);

  configSetMqttIp(OPTA2IOT_MQTT_IP);
  configSetMqttPort(OPTA2IOT_MQTT_PORT);
  configSetMqttUser(OPTA2IOT_MQTT_USER);
  configSetMqttPassword(OPTA2IOT_MQTT_PASSWORD);
  configSetMqttBase(OPTA2IOT_MQTT_BASE);
  configSetMqttInterval(OPTA2IOT_MQTT_INTERVAL);

  for (size_t i = 0; i < boardGetInputsNum(); i++) {
    configSetInputType(i, IoType::IoDigital);
  }
}

bool Opta::configWriteToFile() {
  String str = configWriteToJson(false);

  serialInfo(label_config_file_write);
  kv_set("config", str.c_str(), str.length(), 0);

  return true;
}

bool Opta::configReadFromFile() {
  serialInfo(label_config_file_read);

  bool ret = true;
  char readBuffer[1024];
  kv_get("config", readBuffer, 1024, 0);
  if (configReadFromJson(readBuffer, 1024) < 1) {
    serialWarn(label_config_file_error);
    reset();
    ret = false;
  }

  return ret;
}

/*
 * IO
 */

bool Opta::ioSetup() {
  serialLine(label_io_setup);

  serialInfo(label_io_resolution + String(IoResolution));
  analogReadResolution(IoResolution);

  for (size_t i = 0; i < boardGetInputsNum(); ++i) {
    serialInfo("Set input " + String(i + 1) + " of type " + String(configGetInputType(i)) + " on pin " + String(BoardInputs[i]));
    if (configGetInputType(i) == IoType::IoDigital || configGetInputType(i) == IoType::IoPulse) {
      pinMode(BoardInputs[i], INPUT);
    }
  }
  for (size_t i = 0; i < boardGetOutputsNum(); ++i) {
    serialInfo("Set output " + String(i + 1) + " on pin " + String(BoardInputs[i]) + " with LED on pin " + BoardOutputsLeds[i]);
    pinMode(BoardOutputs[i], OUTPUT);
    pinMode(BoardOutputsLeds[i], OUTPUT);

    ioSetDigitalOuput(i, false);
  }

  return true;
}

bool Opta::ioLoop() {
  if (networkIsConnected() && mqttIsConnected()) {
    if (now() - _ioLastPoll > IoPollDelay) {  // Inputs loop delay
      String inputsCurrent[IoMaxInputs];
      for (size_t i = 0; i < boardGetInputsNum(); i++) {
        if (configGetInputType(i) == IoType::IoAnalog) {
          inputsCurrent[i] = String(ioGetAnalogInputString(i)).c_str();
        } else {
          inputsCurrent[i] = String(ioGetDigitalInput(i)).c_str();
        }

        if (!inputsCurrent[i].equals(_ioPreviousState[i])) {
          //ts ... && only 1 for pulse
          if (_ioLastPoll > 0 && (configGetInputType(i) != IoType::IoPulse || inputsCurrent[i].equals(String('1')))) {
            String inTopic = "I" + String(i + 1);
            String rootTopic = configGetMqttBase() + configGetDeviceId() + "/";

            mqttPublish(String(rootTopic + inTopic + "/val").c_str(), String(inputsCurrent[i]).c_str());
            mqttPublish(String(rootTopic + inTopic + "/type").c_str(), String(configGetInputType(i)).c_str());

            serialInfo(String("[" + inTopic + "] " + _ioPreviousState[i] + " => " + inputsCurrent[i]).c_str());
          }
          _ioPreviousState[i] = inputsCurrent[i];
        }
      }
      _ioLastPoll = now();
    }
  }

  return true;
}

bool Opta::ioGetDigitalInput(size_t index) {
  if (index < boardGetInputsNum() && (configGetInputType(index) != IoType::IoAnalog)) {
    return digitalRead(BoardInputs[index]) == 1;
  }

  return false;
}

float Opta::ioGetAnalogInput(size_t index) {
  if (index < boardGetInputsNum() && (configGetInputType(index) == IoType::IoAnalog)) {
    return analogRead(BoardInputs[index]) * (3.249 / ((1 << IoResolution) - 1)) / 0.3034;
  }

  return 0;
}

String Opta::ioGetAnalogInputString(size_t index) {
  float value = ioGetAnalogInput(index);
  char buffer[10];
  snprintf(buffer, sizeof(buffer), "%0.1f", value);

  return String(buffer);
}

void Opta::ioSetDigitalOuput(size_t index, bool on) {
  if (index < boardGetOutputsNum()) {
    digitalWrite(BoardOutputs[index], on ? 1 : 0);
    digitalWrite(BoardOutputsLeds[index], on ? 1 : 0);
  }
}

/**
 * Network
 */

bool Opta::networkSetup() {
  serialLine(label_network_setup);

  if (boardIsWifi() && configGetNetworkWifi() && configGetNetworkSsid() != "" && configGetNetworkPassword() != "") {
    serialInfo(label_network_mode + String("Wifi standard network"));
    networkSetType(NetworkType::NetworkStandard);

    if (WiFi.status() == WL_NO_MODULE) {
      // Ko
      return stop(label_network_fail);
    }

    networkConnectStandard();
  } else if (boardIsWifi() && configGetNetworkWifi()) {
    serialInfo(label_network_mode + String("Wifi Access Point network"));
    networkSetType(NetworkType::NetworkAccessPoint);

    if (WiFi.status() == WL_NO_MODULE) {
      // Ok
      return stop(label_network_fail);
    }

    String netApSsid = "opta2iot" + configGetDeviceId();
    String netApPass = "opta2iot";
    char ssid[32];
    char pass[32];
    netApSsid.toCharArray(ssid, sizeof(ssid));
    netApPass.toCharArray(pass, sizeof(pass));

    serialInfo(label_network_ssid + netApSsid + " / " + netApPass);
    serialInfo(label_network_static_ip + configGetNetworkIp());

    WiFi.config(networkParseIp(configGetNetworkIp()));

    ledSetFreeze(true);
    int ret = WiFi.beginAP(ssid, pass);

    if (ret != WL_AP_LISTENING) {
      // Ko
      return stop(label_network_ap_fail);
    } else {
      serialInfo(label_network_ap_success);
      networkSetConnected(true);
    }
    ledSetFreeze(false);
  } else {
    serialInfo(label_network_mode + String("Ethernet network"));
    networkSetType(NetworkType::NetworkEthernet);

    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      // Ko
      return stop(label_network_fail);
    }
    
    networkConnectEthernet();
  }

  delay(1000);

  if (networkIsConnected() && configGetNetworkDhcp()) {
    serialInfo(label_network_dhcp_ip + networkLocalIp().toString());
  }

  return true;
}

bool Opta::networkLoop() {
  if (networkIsEthernet()) {
    // mauvaise condition
    if (!networkIsConnected() && (_networkLastRetry == 0 || now() - _networkLastRetry > (NetworkRetryDelay * 1000))) {
      _networkLastRetry = now();
      if (networkIsConnected()) {
        Ethernet.maintain();
      } else {
        networkConnectEthernet();
      }
    }
    if (!networkIsConnected() && Ethernet.linkStatus() == LinkON) {
      serialInfo(label_network_eth_plug);
      networkSetConnected(true);
    }
    if (networkIsConnected() && Ethernet.linkStatus() != LinkON) {
      serialWarn(label_network_eth_unplug);
      networkSetConnected(false);
    }
    if (networkIsConnected() && Ethernet.linkStatus() == LinkON) {
      _networkLastRetry = 0;
      networkSetConnected(true);
    }
  } else if (networkIsStandard()) {
    if (!networkIsConnected() && (_networkLastRetry == 0 || now() - _networkLastRetry > (NetworkRetryDelay * 1000))) {
      _networkLastRetry = now();
      networkConnectStandard();
    }
  } else if (networkIsAccessPoint()) {
    if (_networkAccessPointStatus != WiFi.status()) {
      _networkAccessPointStatus = WiFi.status();

      if (_networkAccessPointStatus == WL_AP_CONNECTED) {
        serialInfo(label_network_ap_plug);
      } else if (_networkAccessPointFirstLoop) {  // do not display message on startup
        _networkAccessPointFirstLoop = false;
      } else {
        serialWarn(label_network_ap_unplug);
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
  return networkIsEthernet() ? Ethernet.localIP() : WiFi.localIP();
}

bool Opta::networkSetConnected(bool connected) {
  _networkConnected = connected;

  return true;
}

bool Opta::networkIsConnected() {
  return _networkConnected;
}

bool Opta::networkSetType(NetworkType type) {
  _networkType = type;

  return true;
}

bool Opta::networkIsAccessPoint() {
  return _networkType == NetworkType::NetworkAccessPoint;
}

bool Opta::networkIsStandard() {
  return _networkType == NetworkType::NetworkStandard;
}

bool Opta::networkIsEthernet() {
  return _networkType == NetworkType::NetworkEthernet;
}

void Opta::networkConnectEthernet() {
  serialLine(label_network_eth);

  int ret = 0;
  ledSetFreeze(true);
  if (configGetNetworkDhcp()) {
    serialInfo(label_network_mode + String("DHCP"));
    ret = Ethernet.begin(nullptr, connectTimeout(), 4000);
  } else {
    serialInfo(label_network_mode + String("Static IP"));
    ret = Ethernet.begin(nullptr, networkParseIp(configGetNetworkIp()), networkParseIp(configGetNetworkDns()), networkParseIp(configGetNetworkGateway()), networkParseIp(configGetNetworkSubnet()), connectTimeout(), 4000);
  }
  ledSetFreeze(false);

  if (ret == 0) {
    networkSetConnected(false);
    serialWarn(label_network_eth_fail);
    if (Ethernet.linkStatus() == LinkOFF) {
      serialWarn(label_network_eth_unplug);
    }
  } else {
    networkSetConnected(true);
    serialInfo(label_network_eth_success + networkLocalIp().toString());
  }
}

void Opta::networkConnectStandard() {
  serialLine(label_network_sta);

  String netApSsid = configGetNetworkSsid();
  String netApPass = configGetNetworkPassword();
  char ssid[32];
  char pass[32];
  netApSsid.toCharArray(ssid, sizeof(ssid));
  netApPass.toCharArray(pass, sizeof(pass));

  serialInfo(label_network_ssid + netApSsid + " / " + netApPass);
  if (configGetNetworkDhcp()) {
    serialInfo(label_network_mode + String("DHCP"));
  } else {
    serialInfo(label_network_mode + String("Static IP"));
    WiFi.config(networkParseIp(configGetNetworkIp()), networkParseIp(configGetNetworkDns()), networkParseIp(configGetNetworkGateway()), networkParseIp(configGetNetworkSubnet()));
  }

  ledSetFreeze(true);
  WiFi.setTimeout(connectTimeout());
  int ret = WiFi.begin(ssid, pass);
  ledSetFreeze(false);

  if (ret != WL_CONNECTED) {
    serialWarn(label_network_sta_fail);
    networkSetConnected(false);
  } else {
    serialInfo(label_network_sta_success);
    networkSetConnected(true);
  }
}

/**
 * Time
 */

bool Opta::timeSetup() {
  serialLine(label_time_setup);

  timeUpdate();

  return true;
}

bool Opta::timeLoop(bool startBenchmark) {
  // If time update failed during setup, try every hour until success
  if (!_timeUpdated && ((now() - 3600000) < _timeLastUpdate)) {
    _timeLastUpdate = now();
    timeUpdate();
  }

  // Loop benchmark
  if (startBenchmark) {
    serialLine(label_time_loop_start);

    _timeBenchmarkTime = now();
    _timeBenchmarkCount = _timeBenchmarkRepeat = _timeBenchmarkSum = 0;
  } else if (_timeBenchmarkTime > 0) {
    _timeBenchmarkCount++;
    if (now() - _timeBenchmarkTime > 1000) {
      serialInfo(label_time_loop_line + String(_timeBenchmarkCount));

      if (_timeBenchmarkRepeat < 10) {
        _timeBenchmarkTime = now();
        _timeBenchmarkSum += _timeBenchmarkCount;
        _timeBenchmarkCount = 0;
        _timeBenchmarkRepeat++;
      } else {
        _timeBenchmarkTime = 0;

        serialInfo(label_time_loop_average + String(_timeBenchmarkSum / 10));
      }
    }
  }

  return true;
}

void Opta::timeUpdate() {
  if (networkIsConnected() && !networkIsAccessPoint()) {
    serialLine(label_time_update);

    ledSetFreeze(true);
    if (networkIsStandard()) {
      WiFiUDP wifiUdpClient;
      NTPClient timeClient(wifiUdpClient, TimeServer, configGetTimeOffset() * 3600, 0);
      timeClient.begin();
      if (!timeClient.update() || !timeClient.isTimeSet()) {
        serialWarn(label_time_update_fail);
      } else {
        const unsigned long epoch = timeClient.getEpochTime();
        set_time(epoch);
        serialInfo(label_time_update_success + timeClient.getFormattedTime());
        _timeUpdated = true;
      }
    } else if (networkIsEthernet()) {
      EthernetUDP ethernetUdpClient;
      NTPClient timeClient(ethernetUdpClient, TimeServer, configGetTimeOffset() * 3600, 0);
      timeClient.begin();
      if (!timeClient.update()) {
        serialWarn(label_time_update_fail);
      } else {
        const unsigned long epoch = timeClient.getEpochTime();
        set_time(epoch);
        serialInfo(label_time_update_success + timeClient.getFormattedTime());
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
  serialLine(label_mqtt_setup);

  serialInfo(label_mqtt_server + configGetMqttIp() + ":" + String(configGetMqttPort()));

  ledSetFreeze(true);
  if (networkIsEthernet()) {
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
  if (mqttIsConnected()) {
    int rspSize = mqttClient.parseMessage();
    if (rspSize) {
      String rspTopic = mqttClient.messageTopic();
      String rspPayload = "";

      for (int index = 0; index < rspSize; index++) {
        rspPayload += (char)mqttClient.read();
      }

      mqttReceive(rspTopic, rspPayload);
    }
  }

  return true;
}

bool Opta::mqttSetConnected(bool connected) {
  _mqttConnected = connected;

  return true;
}

bool Opta::mqttIsConnected() {
  return _mqttConnected;
}

void Opta::mqttConnect() {
  if (!networkIsConnected() || networkIsAccessPoint()) {
    return;
  }
  if (mqttClient.connected()) {
    mqttSetConnected(true);
    _mqttLastRetry = 0;
    return;
  }

  if (_mqttLastRetry > 0 && ((now() - _mqttLastRetry) < (NetworkRetryDelay * 1000))) {  // retry every x seconds
    return;
  }

  mqttSetConnected(false);
  _mqttLastRetry = millis();
  serialLine(label_mqtt_broker);

  ledSetFreeze(true);
  mqttClient.setId(configGetDeviceId());
  mqttClient.setUsernamePassword(configGetMqttUser(), configGetMqttPassword());
  mqttClient.setConnectionTimeout(connectTimeout());
  if (!mqttClient.connect(configGetMqttIp().c_str(), configGetMqttPort())) {
    serialWarn(label_mqtt_broker_fail);
    ledSetFreeze(false);
    return;
  }
  ledSetFreeze(false);

  serialInfo(label_mqtt_broker_success);
  mqttSetConnected(true);

  String topic = configGetMqttBase() + configGetDeviceId() + "/device/get";  // command for device information
  mqttClient.subscribe(topic);
  serialInfo(label_mqtt_subscribe + topic);

  for (size_t i = 0; i < boardGetOutputsNum(); i++) {
    String topic = configGetMqttBase() + configGetDeviceId() + "/O" + String(i + 1);  // command for outputs
    mqttClient.subscribe(topic);
    serialInfo(label_mqtt_subscribe + topic);
  }

  mqttPublishDevice();
}

bool Opta::mqttSubscribe(String topic) {
  if(mqttIsConnected()) {
    mqttClient.subscribe(topic);

    return true;
  }

  return false;
}

bool Opta::mqttPublish(String topic, String message) {
  if(mqttIsConnected()) {
    mqttClient.beginMessage(topic);
    mqttClient.print(message);
    mqttClient.endMessage();

    return true;
  }

  return false;
}

void Opta::mqttReceive(String &topic, String &payload) {
  serialLine(label_mqtt_receive + topic + " = " + payload);

  String match = configGetMqttBase() + configGetDeviceId() + "/device/get";
  if (topic == match) {
    mqttPublishDevice();
  }

  for (size_t i = 0; i < boardGetInputsNum(); i++) {
    String match = configGetMqttBase() + configGetDeviceId() + "/O" + String(i + 1);
    if (topic == match) {
      serialInfo("Setting output " + String(i + 1) + " to " + payload);

      ioSetDigitalOuput(i, (bool)payload.toInt());
    }
  }
}

void Opta::mqttPublishDevice() {
  if (networkIsConnected() && mqttIsConnected()) {
    serialLine(label_mqtt_publish_device);

    String rootTopic = configGetMqttBase() + configGetDeviceId();

    mqttPublish(String(rootTopic + "/device/type").c_str(), boardGetName().c_str());
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
  if (networkIsConnected() && mqttIsConnected()) {
    serialLine(label_mqtt_publish_inputs);

    String rootTopic = configGetMqttBase() + configGetDeviceId() + "/";
    for (size_t i = 0; i < boardGetInputsNum(); i++) {
      String inTopic = "I" + String(i + 1) + "/";
      if (configGetInputType(i) == IoType::IoAnalog) {
        mqttPublish(String(rootTopic + inTopic + "val").c_str(), ioGetAnalogInputString(i).c_str());
        mqttPublish(String(rootTopic + inTopic + "type").c_str(), String(configGetInputType(i)).c_str());
      } else {
        mqttPublish(String(rootTopic + inTopic + "val").c_str(), String(ioGetDigitalInput(i)).c_str());
        mqttPublish(String(rootTopic + inTopic + "type").c_str(), String(configGetInputType(i)).c_str());
      }
    }
  }
}

/*
 * Web
 */

bool Opta::webSetup() {
  serialLine(label_web_setup);

  ledSetFreeze(true);
  webEthernetServer = EthernetServer(80);
  webWifiServer = WiFiServer(80);
  if (networkIsEthernet()) {
    serialInfo(label_web_ethernet);
    webEthernetServer.begin();
  } else {
    serialInfo(label_web_wifi);
    webWifiServer.begin();
  }
  ledSetFreeze(false);

  return true;
}

bool Opta::webLoop() {
  if (networkIsConnected() && odd()) {  // _odd: leave place for other things
    Client *webClient = nullptr;
    if (networkIsEthernet()) {
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
        String inputString = configGetDeviceUser() + ":" + configGetDevicePassword();
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
  const size_t n = sizeof web_favicon_hex / bufferSize;
  const size_t r = sizeof web_favicon_hex % bufferSize;
  for (size_t i = 0; i < sizeof web_favicon_hex; i += bufferSize) {
    memcpy_P(buffer, web_favicon_hex + i, bufferSize);
    client->write(buffer, bufferSize);
  }
  if (r != 0) {
    memcpy_P(buffer, web_favicon_hex + n * bufferSize, r);
    client->write(buffer, r);
  }
}

void Opta::webSendStyle(Client *&client) {
  client->println("HTTP/1.1 200 OK");
  client->println("Content-Type: text/css");
  client->println("Connection: close");
  client->println();
  client->println(web_style_css);
}

void Opta::webSendAuth(Client *&client) {
  client->println("HTTP/1.1 401 Authorization Required");
  client->println("WWW-Authenticate: Basic realm=\"Secure Area\"");
  client->println("Content-Type: text/html");
  client->println("Connnection: close");
  client->println();
  client->println(web_auth_html);
}

void Opta::webSendError(Client *&client) {
  client->println("HTTP/1.1 404 Not Found");
  client->println("Content-Type: text/html");
  client->println("Connnection: close");
  client->println();
  client->println(web_error_html);
}

void Opta::webSendHome(Client *&client) {
  client->println("HTTP/1.1 200 OK");
  client->println("Content-Type: text/html");
  client->println("Connection: close");
  client->println();
  client->println(web_home_html);
}

void Opta::webSendDevice(Client *&client) {
  client->println("HTTP/1.1 200 OK");
  client->println("Content-Type: text/html");
  client->println("Connection: close");
  client->println();
  client->println(web_device_html);
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
  doc["deviceId"] = configGetDeviceId();
  doc["version"] = version();
  doc["mqttConnected"] = mqttIsConnected();
  doc["time"] = timeGet();
  doc["gmt"] = configGetTimeOffset();

  JsonObject inputsObject = doc["inputs"].to<JsonObject>();
  for (size_t i = 0; i < boardGetInputsNum(); i++) {
    String name = "I" + String(i + 1);
    JsonObject obj = inputsObject[name].to<JsonObject>();
    obj["type"] = configGetInputType(i);
    if (configGetInputType(i) == IoType::IoAnalog) {
      obj["value"] = ioGetAnalogInput(i);
    } else {
      obj["value"] = ioGetDigitalInput(i);
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
  serialLine(label_web_config);

  bool isValid = true;
  String jsonString = "";

  String oldDevicePassword = configGetDevicePassword();
  String oldNetPassword = configGetNetworkPassword();
  String oldMqttPassword = configGetMqttPassword();

  watchdogMax();
  while (client->available()) {
    String line = client->readStringUntil('\n');  // Read line-by-line

    if (line == "\r") {  // Detect the end of headers (an empty line)
      isValid = false;
      break;
    }

    jsonString += line;
  }
  watchdogMin();

  if (!isValid || configReadFromJson(jsonString.c_str(), jsonString.length()) < 1) {
    serialWarn(label_web_config_fail);
    isValid = false;
  } else {
    if (configGetDeviceId() == "") {  // device ID must be set
      serialWarn(label_web_config_fail_id);
      isValid = false;
    }
    if (configGetDeviceUser() == "") {  // device user must be set
      serialWarn(label_web_config_fail_user);
      isValid = false;
    }
    if (configGetDevicePassword() == "") {  // get old device password if none set
      serialInfo(label_web_config_keep_device);
      configSetDevicePassword(oldDevicePassword);
    }
    int offset = configGetTimeOffset();
    if (offset > 24 || offset < -24) {  // time offset must be in this day
      configSetTimeOffset(0);
    }
    if (configGetNetworkPassword() == "" && configGetNetworkSsid() != "") {  // get old wifi password if none set
      serialInfo(label_web_config_keep_wifi);
      configSetNetworkPassword(oldNetPassword);
    }
    if (configGetMqttPassword() == "" && configGetMqttUser() != "") {  // get old mqtt password if none set
      serialInfo(label_web_config_keep_mqtt);
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