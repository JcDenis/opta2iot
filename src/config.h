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

#include "default.h"

namespace opta2iot {

constexpr int SKETCH_VERSION = 2025122300;

constexpr int OPTA_NONE = 0;
constexpr int OPTA_RS485 = 1;
constexpr int OPTA_WIFI = 2;
constexpr int OPTA_LITE = 3;

constexpr int NET_NONE = 0;
constexpr int NET_ETH = 1;
constexpr int NET_STA = 2;
constexpr int NET_AP = 3;

constexpr int INPUT_PULSE = 2;
constexpr int INPUT_DIGITAL = 1;
constexpr int INPUT_ANALOG = 0;

constexpr int NUM_INPUTS = 8;
constexpr int NUM_OUTPUTS = 4;
constexpr int ADC_BITS = 16;

constexpr int CONFIG_RESET_DELAY = 3; // in second
constexpr int PINS_POLL_DELAY = 50; // in millisecond
constexpr int NET_RETRY_DELAY = 30; // in second
constexpr int MQTT_RETRY_DELAY = 60; // in second

constexpr int DO = 1;
constexpr int OK = 2;
constexpr int KO = 3;
constexpr int IN = 4;

class config {
private:
  String _deviceId;
  String _deviceUser;
  String _devicePassword;

  bool _netDhcp;
  bool _netWifi;
  String _netIp;
  String _netSsid;
  String _netPassword;

  String _mqttIp;
  String _mqttUser;
  String _mqttPassword;
  unsigned int _mqttPort;
  String _mqttBase;
  int _mqttInterval;

  int _inputs[NUM_INPUTS][2];
  const int _outputs[NUM_OUTPUTS] = {D0, D1, D2, D3};
  const int _outputsLed[NUM_OUTPUTS] = {LED_D0, LED_D1, LED_D2, LED_D3};

public:
  config();
  ~config() = default;

  void loadDefaults();

  String getDeviceId() const;
  void setDeviceId(const String &id);

  String getDeviceUser() const;
  void setDeviceUser(const String &user);

  String getDevicePassword() const;
  void setDevicePassword(const String &pass);

  String getNetIp() const;
  void setNetIp(const String &ip);

  bool getNetDhcp() const;
  void setNetDhcp(const bool val);

  bool getNetWifi() const;
  void setNetWifi(const bool val);

  String getNetSsid() const;
  void setNetSsid(const String &id);

  String getNetPassword() const;
  void setNetPassword(const String &pass);

  String getMqttIp() const;
  void setMqttIp(const String &ip);

  int getMqttPort() const;
  void setMqttPort(const int port);

  String getMqttUser() const;
  void setMqttUser(const String &user);

  String getMqttPassword() const;
  void setMqttPassword(const String &password);

  String getMqttBase() const;
  void setMqttBase(const String &base);

  int getMqttInterval() const;
  void setMqttInterval(int interval);

  int getInputType(int index) const;
  int setInputType(int index, int type);

  int getInputPin(int index) const;
  int getOutputPin(int index) const;
  int getOutputLed(int index) const;

  void initializePins();

  int loadFromJson(const char *buffer, size_t length);

  String toJson(const bool val) const;
}; // class

} // namespace