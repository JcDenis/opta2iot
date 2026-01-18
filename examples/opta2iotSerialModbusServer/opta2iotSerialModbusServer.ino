#include <Arduino.h>
#include "opta2iot.h"

opta2iot::Opta opta;

/**
 * Use Opta Web interface to set device as Modbus RTU server.
 * Nothing more.
 */
void setup() {
  if (opta.setup()) {
    opta.thread();
  }
}

void loop() {
  if (opta.running()) {
    // ...
  }
}
