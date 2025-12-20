/*
 * opta2iot
 *
 * Arduino Opta Lite IIOT gateway
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

#if !defined(CONFIG_H)
#define CONFIG_H

#include "Arduino.h"

#define DEFAULT_DEVICE_ID "99999"
#define DEFAULT_DEVICE_USER "admin"
#define DEFAULT_DEVICE_PASSWORD "admin"

#define DEFAULT_MQTT_IP "192.168.1.100"
#define DEFAULT_MQTT_PORT 1883
#define DEFAULT_MQTT_USER "mqtt_user"
#define DEFAULT_MQTT_PASSWORD "mqtt_password"
#define DEFAULT_MQTT_BASE "/opta/"
#define DEFAULT_MQTT_INTERVAL 0

#define DEFAULT_NET_DHCP false
#define DEFAULT_NET_IP "192.168.1.231"

namespace opta2iot {

constexpr int VERSION = 2025121900;
constexpr int NUM_INPUTS = 8;
constexpr int NUM_OUTPUTS = 4;
constexpr int PULSE = 2;
constexpr int DIGITAL = 1;
constexpr int ANALOG = 0;
constexpr int ADC_BITS = 16;
constexpr int CONFIG_RESET_DELAY = 3; // in second
constexpr int IO_POLL_DELAY = 40; // in millisecond
constexpr int NET_RETRY_DELAY = 30; // in second
constexpr int MQTT_RETRY_DELAY = 60; // in second

class config {
private:
  String _deviceId;
  String _deviceUser;
  String _devicePassword;

  bool _netDhcp;
  String _netIp;

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
};
}

#endif  // CONFIG_H