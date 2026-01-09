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

#include <Arduino.h>
#include "opta2iot.h"

/*
 * In this example the device is configured from this .ino file and can not be changed later.
 *
 * We remove unused features like:
 * * Serial
 * * Config
 * * Web server
 *
 * All configuration will be made from setup.
 */

opta2iot::Opta opta;

void setup() {
  // Configure non default values
  opta.configSetDeviceId("12345");
  opta.configSetNetworkWifi(false);
  opta.configSetNetworkDhcp(true);
  opta.configSetMqttIp("10.10.0.10");
  opta.configSetMqttUser("TheUser");
  opta.configSetMqttPassword("ThePassword");

  // Execute device setup without removed parts
  if (opta.startSetup()
      && opta.boardSetup()
      && opta.ledSetup()
      && opta.buttonSetup()
      && opta.ioSetup()
      && opta.networkSetup()
      && opta.timeSetup()
      && opta.mqttSetup()
      && opta.endSetup()) {}
}

void loop() {
  // Execute device loop without removed parts
  while (opta.running()
         && opta.startLoop()
         && opta.ledLoop()
         && opta.buttonLoop()
         && opta.ioLoop()
         && opta.networkLoop()
         && opta.timeLoop()
         && opta.mqttLoop()
         && opta.endLoop()) {}
}
