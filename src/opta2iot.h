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

#ifndef OPTA2IOT_H
#define OPTA2IOT_H

#include <Arduino.h>
#include <WiFi.h>
#include <Ethernet.h>
#include <MQTT.h>
#include "BlockDevice.h"
#include "define.h"

#ifndef CORE_CM7
#error "opta2iot must run on M7 Main Core"
#endif

namespace opta2iot {

class Opta {

private:

  // Main

  static Opta *instance;
  bool _stop = false;
  bool _odd = false;
  uint32_t _now = 0;
  char _version[17]; // Human readable version
  bool _threadStarted = false;

  // Serial

  byte _serialProgress = 0;
  char _serialIncoming[30];  // limit command message length

  // Board

  const unsigned int BoardInputs[8] = { A0, A1, A2, A3, A4, A5, A6, A7 };       // I1, I2, I3, I4, I5, I6, I7, I8
  const unsigned int BoardOutputs[4] = { D0, D1, D2, D3 };                      // O1, O2, O3, O4
  const unsigned int BoardOutputsLeds[4] = { LED_D0, LED_D1, LED_D2, LED_D3 };  // O1, O2, O3, O4
  const unsigned int BoardUserLeds[3] = { LED_RESET, LEDR, LED_USER };          // Green, Red, Blue
  const unsigned int BoardUserButtons[1] = { BTN_USER };                        // Button
  enum BoardType {
    Undefined = 0,
    Rs485,
    Wifi,
    Lite
  };
  byte _boardType = BoardType::Undefined;
  const char *_boardName = "Unknown board name ";

  // Flash

  mbed::BlockDevice *_flashRoot;
  bool flashFormat(bool force = false);
  bool flashWiFiFirmwareAndCertificates();
  bool flashWiFiFirmwareMapped();

  // User LEDs

  bool _ledGreen = false;
  bool _ledRed = false;
  bool _ledBlue = false;
  byte _ledHeartBeatStep = 0;
  uint32_t _ledHeartbeatStart = 0;
  uint32_t _ledConnectionStart = 0;
  bool _ledConnectionState = false;

  // User buttons

  uint32_t _buttonStart = 0;
  uint32_t _buttonDuration = 0;

  // Config

  String _configDeviceId = OPTA2IOT_DEVICE_ID;
  String _configDeviceUser = OPTA2IOT_DEVICE_USER;
  String _configDevicePassword = OPTA2IOT_DEVICE_PASSWORD;

  byte _configTimeOffset = OPTA2IOT_TIME_OFFSET;

  bool _configNetworkDhcp = OPTA2IOT_NET_DHCP;
  bool _configNetworkWifi = OPTA2IOT_NET_WIFI;
  String _configNetworkIp = OPTA2IOT_NET_IP;
  String _configNetworkSsid = OPTA2IOT_NET_SSID;
  String _configNetworkPassword = OPTA2IOT_NET_PASSWORD;

  String _configMqttIp = OPTA2IOT_MQTT_IP;
  uint16_t _configMqttPort = OPTA2IOT_MQTT_PORT;
  String _configMqttUser = OPTA2IOT_MQTT_USER;
  String _configMqttPassword = OPTA2IOT_MQTT_PASSWORD;
  String _configMqttBase = OPTA2IOT_MQTT_BASE;
  uint16_t _configMqttInterval = OPTA2IOT_MQTT_INTERVAL;
  byte _configInputs[OPTA2IOT_MAX_INPUTS];

  // IO
  uint32_t _ioLastPoll = 0;
  String _ioPreviousState[OPTA2IOT_MAX_INPUTS];

  // Network

  byte _networkSelected = NetworkType::None;
  bool _networkConnected = false;
  uint32_t _networkLastRetry = 0;
  bool _networkAccessPointFirstLoop = true;
  int _networkAccessPointStatus = WL_IDLE_STATUS;
  void networkConnectRj45();
  void networkConnectStandard();

  // Time

  uint32_t _timeLastUpdate = 0;
  bool _timeUpdated = false;
  uint32_t _timeBenchmarkTime = 0;
  uint32_t _timeBenchmarkCount = 0;
  byte _timeBenchmarkRepeat = 0;
  uint32_t _timeBenchmarkSum = 0;

  // MQTT

  uint32_t _mqttLastRetry = 0;
  bool _mqttConnected = false;
  MQTTClient mqttClient;
  EthernetClient mqttEthernetClient;
  WiFiClient mqttWifiClient;
  void mqttConnect();
  void mqttReceive(String &topic, String &payload);

  // Web

  bool _webConnected = false;
  EthernetServer webEthernetServer;
  WiFiServer webWifiServer;
  void webConnect(Client *&client);
  void webSendAuth(Client *&client);
  void webSendError(Client *&client);
  void webSendFavicon(Client *&client);
  void webSendStyle(Client *&client);
  void webSendHome(Client *&client);
  void webSendDevice(Client *&client);
  void webSendConfig(Client *&client);
  void webSendData(Client *&client);
  void webReceiveConfig(Client *&client);
  void webReceivePublish(Client *&client);

public:

  // Main

  Opta();
  static const uint32_t Revision = 2026010800;
  char *version();
  uint32_t now(uint32_t now = 0);
  bool setup();
  bool loop();
  bool stop(String reason);
  bool stop(const char *reason);
  bool running();
  void print(String str);
  void print(const char *str);
  void format();
  void reset();
  void reboot();
  void thread();

  bool startSetup();
  bool endSetup();
  bool startLoop();
  bool endLoop();

  // Serial

  static const bool SerialVerbose = OPTA2IOT_SERIAL_VERBOSE;

  bool serialSetup();
  bool serialLoop();
  void serialLine(String str);                                                          // print to serial a message
  void serialLine(const char *str);                                                     // print to serial a message
  void serialInfo(String str);                                                          // print to serial a information
  void serialInfo(const char *str);                                                     // print to serial a information
  void serialWarn(String str);                                                          // print to serial a warning
  void serialWarn(const char *str);                                                     // print to serial a warning
  void serialProgress(uint32_t offset, uint32_t size, uint32_t threshold, bool reset);  // print to serial a progress
  bool serialIncoming();                                                                // check if a serial message was received
  String serialReceived();                                                              // get received serial message

  // Board

  bool boardSetup();
  size_t boardGetInputsNum();
  size_t boardGetOutputsNum();

  // Flash

  bool flashSetup();
  bool flashHasWifi();
  bool flashHasOta();
  bool flashHasUser();

  // User LEDs

  bool ledSetup();
  bool ledLoop();
  bool ledGetGreen();
  void ledSetGreen(bool on = true);
  bool ledGetRed();
  void ledSetRed(bool on = true);
  bool ledGetBlue();
  void ledSetBlue(bool on = true);
  void ledSetFreeze(bool on = true);

  // User button

  bool buttonSetup();
  bool buttonLoop();
  bool buttonGet();
  uint32_t buttonDuration();

  // Config

  bool configSetup();

  String configGetDeviceId() const;
  void configSetDeviceId(const String &id);
  String configGetDeviceUser() const;
  void configSetDeviceUser(const String &user);
  String configGetDevicePassword() const;
  void configSetDevicePassword(const String &pass);

  int configGetTimeOffset() const;
  void setTimeOffset(const int offset);

  String configGetNetworkIp() const;
  void configSetNetworkIp(const String &ip);
  bool configGetNetworkDhcp() const;
  void configSetNetworkDhcp(const bool val);
  bool configGetNetworkWifi() const;
  void configSetNetworkWifi(const bool val);
  String configGetNetworkSsid() const;
  void configSetNetworkSsid(const String &id);
  String configGetNetworkPassword() const;
  void configSetNetworkPassword(const String &pass);

  String configGetMqttIp() const;
  void configSetMqttIp(const String &ip);
  int configGetMqttPort() const;
  void configSetMqttPort(const int port);
  String configGetMqttUser() const;
  void configSetMqttUser(const String &user);
  String configGetMqttPassword() const;
  void configSetMqttPassword(const String &password);
  String configGetMqttBase() const;
  void configSetMqttBase(const String &base);
  int configGetMqttInterval() const;
  void configSetMqttInterval(int interval);

  byte configGetInputType(size_t index);
  bool configSetInputType(size_t index, byte type);

  bool configReadFromJson(const char *buffer, size_t length);
  String configWriteToJson(const bool nopass = true);
  bool configReadFromFile();
  bool configWriteToFile();
  void configReadFromDefault();

  // IO

  enum IoType {
    Analog = 0,
    Digital,
    Pulse
  };
  static const byte IoResolution = OPTA2IOT_IO_RESOLUTION;
  static const byte IoMaxInputs = OPTA2IOT_MAX_INPUTS;
  static const byte IoMaxOutputs = OPTA2IOT_MAX_OUTPUTS;
  static const byte IoPollDelay = OPTA2IOT_DELAY_POOL;

  bool ioSetup();
  bool ioLoop();
  bool ioGetDigitalInput(size_t index);            // get digital or pulse input value
  float ioGetAnalogInput(size_t index);           // get analog input value
  void ioSetDigitalOuput(size_t index, bool on);  // set digital output value

  // Network

  enum NetworkType {
    None = 0,
    Rj45,        // ETH (why? 'cause word Ethernet already exists)
    Standard,    // Wifi STA
    AccessPoint  // Wifi AP
  };
  static const size_t NetworkRetryDelay = OPTA2IOT_DELAY_RETRY;

  bool networkSetup();
  bool networkLoop();
  IPAddress networkParseIp(const String &ip);
  IPAddress networkLocalIp();
  bool networkIsConnected();
  bool networkIsAccessPoint();
  bool networkIsStandard();
  bool networkIsRj45();

  // Time

  const char *TimeServer = OPTA2IOT_TIME_SERVER;

  bool timeSetup();
  bool timeLoop(bool startBenchmark = false);
  void timeUpdate();
  String timeGet();

  // MQTT

  bool mqttSetup();
  bool mqttLoop();
  bool mqttIsConnected();
  void mqttPublishDevice();
  void mqttPublishInputs();

  // Web

  bool webSetup();
  bool webLoop();

};  // class

}  // namespace

#endif  // #ifndef OPTA2IOT_H