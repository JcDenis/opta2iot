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
 * This example uses opta.setup() to load and use all supported features.
 * Opta device is ready to use as is.
 */

// Create Opta object
opta2iot::Opta opta;

void setup() {
  // This executes all Opta setup stuff
  if (opta.setup()) {

    // Setup other stuff here ...

    // Then launches Opta loop in a dedicated thread (required)
    opta.thread();
  }
}

void loop() {
  // Check if main Opta loop is still active
  if (opta.running()) {
     // example where we freeze .ino loop for 60 seconds
    delay(20000);
    // Example on how to store in persistent flash memory a custom config
    opta.storeWrite("more_config", String(opta.timeGet()).c_str());
    delay(40000);
    if (Serial) {
      Serial.println("| This loop does not freeze Opta loop");
      Serial.println("| Watchdog timeout: " + String(opta.watchdogTimeout()));
      Serial.println("| Ethernet connexion: " + String(opta.networkIsConnected() ? "yes" : "no"));
      Serial.println("| MQTT connexion: " + String(opta.mqttIsConnected() ? "yes" : "no"));
      Serial.println("| Reading custom config: " + String(opta.storeRead("more_config")));
      Serial.println();
    }
    // ...
  }
}
