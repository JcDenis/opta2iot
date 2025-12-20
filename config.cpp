/*
 * sopta2iot by Jean-Christian Paul Denis
 *
 * Based on "Remoto" at https://github.com/albydnc/remoto by 
 * Author: Alberto Perro
 * Date: 27-12-2024
 * License: CERN-OHL-P
 *
 * Arduino sketch for the Arduino Opta Lite
 *
 * see README.md file
 */

#include <ArduinoJson.h>
#include <Arduino.h>

#include "config.h"

namespace opta2iot {

config::config() {
  loadDefaults();
}

String config::getDeviceId() const {
  return _deviceId;
}

void config::setDeviceId(const String &id) {
  _deviceId = id;
}

String config::getDeviceUser() const {
  return _deviceUser;
}

void config::setDeviceUser(const String &user) {
  _deviceUser = user;
}

String config::getDevicePassword() const {
  return _devicePassword;
}

void config::setDevicePassword(const String &pass) {
  _devicePassword = pass;
}

String config::getNetIp() const {
  return _netIp;
}

void config::setNetIp(const String &ip) {
  _netIp = ip;
}

bool config::getNetDhcp() const {
  return _netDhcp;
}

void config::setNetDhcp(const bool val) {
  _netDhcp = val;
}

String config::getMqttIp() const {
  return _mqttIp;
}

void config::setMqttIp(const String &ip) {
  _mqttIp = ip;
}

int config::getMqttPort() const {
  return _mqttPort;
}

void config::setMqttPort(const int port) {
  _mqttPort = port;
}

String config::getMqttUser() const {
  return _mqttUser;
}

void config::setMqttUser(const String &user) {
  _mqttUser = user;
}

String config::getMqttPassword() const {
  return _mqttPassword;
}

void config::setMqttPassword(const String &password) {
  _mqttPassword = password;
}

String config::getMqttBase() const {
  return _mqttBase;
}

void config::setMqttBase(const String &base) {
  _mqttBase = base;
}

int config::getMqttInterval() const {
  return _mqttInterval;
}

void config::setMqttInterval(int interval) {
  _mqttInterval = interval;
}

int config::getInputType(int index) const {
  if (index >= 0 && index < NUM_INPUTS) {
    return _inputs[index][1];
  }
  return -1;
}

int config::setInputType(int index, int type) {
  if (index >= 0 && index < NUM_INPUTS && (type == PULSE || type == DIGITAL || type == ANALOG)) {
    _inputs[index][1] = type;
    return 0;
  }
  return -1;
}

int config::getInputPin(int index) const {
  if (index >= 0 && index < NUM_INPUTS) {
    return _inputs[index][0];
  }
  return -1;
}

int config::getOutputPin(int index) const {
  if (index >= 0 && index < NUM_OUTPUTS) {
    return _outputs[index];
  }
  return -1;
}

int config::getOutputLed(int index) const {
  if (index >= 0 && index < NUM_OUTPUTS) {
    return _outputsLed[index];
  }
  return -1;
}

void config::initializePins() {
  analogReadResolution(ADC_BITS);

  for (int i = 0; i < NUM_INPUTS; ++i) {
    if (_inputs[i][1] == DIGITAL || _inputs[i][1] == PULSE) {
      pinMode(_inputs[i][0], INPUT);  // Set pin to digital input

      Serial.print(" > Set input ");
      Serial.print(i+1);
      Serial.print(" on pin ");
      Serial.print(_inputs[i][0]);
      Serial.print(" with type ");
      Serial.println(_inputs[i][1]);
    }
  }
  for (int i = 0; i < NUM_OUTPUTS; ++i) {
    pinMode(_outputs[i], OUTPUT);
    pinMode(_outputsLed[i], OUTPUT);
    digitalWrite(_outputs[i], LOW);
    digitalWrite(_outputsLed[i], LOW);

    Serial.print(" > Set output ");
    Serial.print(i+1);
    Serial.print(" on pin ");
    Serial.print(_outputs[i]);
    Serial.print(" with led pin ");
    Serial.println(_outputsLed[i]);
  }
}

int config::loadFromJson(const char *buffer, size_t length) {
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, buffer, length);

  if (error) {
    Serial.println("Failed to parse JSON");
    return -1;
  }

  if (!doc.containsKey("deviceId")
      || !doc.containsKey("deviceUser")
      || !doc.containsKey("devicePassword")
      || !doc.containsKey("netIp")
      || !doc.containsKey("netDhcp")
      || !doc.containsKey("mqttIp")
      || !doc.containsKey("mqttPort")
      || !doc.containsKey("mqttUser")
      || !doc.containsKey("mqttPassword")
      || !doc.containsKey("mqttBase")
      || !doc.containsKey("mqttInterval")
      || !doc.containsKey("inputs")) {
    Serial.println("Missing required keys in JSON");
    return -1;
  }

  _deviceId = doc["deviceId"].as<String>();
  _deviceUser = doc["deviceUser"].as<String>();
  _devicePassword = doc["devicePassword"].as<String>();
  _netIp = doc["netIp"].as<String>();
  _netDhcp = doc["netDhcp"].as<bool>();
  _mqttIp = doc["mqttIp"].as<String>();
  _mqttPort = doc["mqttPort"].as<int>();
  _mqttUser = doc["mqttUser"].as<String>();
  _mqttPassword = doc["mqttPassword"].as<String>();
  _mqttBase = doc["mqttBase"].as<String>();
  _mqttInterval = doc["mqttInterval"].as<int>();

  for (int i = 0; i < NUM_INPUTS; ++i) {
    String pinName = "I" + String(i + 1);
    _inputs[i][1] = doc["inputs"][pinName].as<int>();
  }

  return 1;
}

String config::toJson(const bool nopass) const {
  DynamicJsonDocument doc(2048);

  doc["version"] = VERSION;
  doc["deviceId"] = _deviceId;
  doc["deviceUser"] = _deviceUser;
  doc["devicePassword"] = nopass ? "" : _devicePassword;
  doc["netIp"] = _netIp;
  doc["netDhcp"] = _netDhcp;
  doc["mqttIp"] = _mqttIp;
  doc["mqttPort"] = _mqttPort;
  doc["mqttUser"] = _mqttUser;
  doc["mqttPassword"] = nopass ? "" : _mqttPassword;
  doc["mqttBase"] = _mqttBase;
  doc["mqttInterval"] = _mqttInterval;

  for (int i = 0; i < NUM_INPUTS; ++i) {
    String pinName = "I" + String(i + 1);
    doc["inputs"][pinName] = _inputs[i][1];
  }

  String jsonString;
  serializeJson(doc, jsonString);

  return jsonString;
}

void config::loadDefaults() {
  _deviceId = DEFAULT_DEVICE_ID;
  _deviceUser = DEFAULT_DEVICE_USER;
  _devicePassword = DEFAULT_DEVICE_PASSWORD;

  _netIp = DEFAULT_NET_IP;
  _netDhcp = DEFAULT_NET_DHCP;

  _mqttIp = DEFAULT_MQTT_IP;
  _mqttPort = DEFAULT_MQTT_PORT;
  _mqttUser = DEFAULT_MQTT_USER;
  _mqttPassword = DEFAULT_MQTT_PASSWORD;
  _mqttBase = DEFAULT_MQTT_BASE;
  _mqttInterval = DEFAULT_MQTT_INTERVAL;

  _inputs[0][0] = A0;
  _inputs[0][1] = DIGITAL;
  _inputs[1][0] = A1;
  _inputs[1][1] = DIGITAL;
  _inputs[2][0] = A2;
  _inputs[2][1] = DIGITAL;
  _inputs[3][0] = A3;
  _inputs[3][1] = DIGITAL;
  _inputs[4][0] = A4;
  _inputs[4][1] = DIGITAL;
  _inputs[5][0] = A5;
  _inputs[5][1] = DIGITAL;
  _inputs[6][0] = A6;
  _inputs[6][1] = DIGITAL;
  _inputs[7][0] = A7;
  _inputs[7][1] = DIGITAL;
}
}