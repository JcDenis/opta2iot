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
     // exemple where we freeze .ino loop for 30 seconds
    delay(30000);
    Serial.println("This loop does not freeze Opta loop");
    // ...
  }
}
