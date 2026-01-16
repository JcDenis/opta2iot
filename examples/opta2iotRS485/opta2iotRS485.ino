#include <Arduino.h>
#include "opta2iot.h"

opta2iot::Opta opta;

unsigned long lastSend = 0;

void setup() {

  /*
   * Start opta RS485 setup as We want to use RS485 as simple serial port.
   */
  if (opta.setup()
      && opta.rs485Setup()) {
    opta.thread();
  }
}

void loop() {
  if (opta.running()) {
    
    /**
      * Every 10 seconds, send device id as message on serial port
      */
    if (millis() - lastSend > 10000) {
      String message = opta.configGetDeviceId();

      /**
       * Try to send message till serial is avalable
       */
      while (!opta.rs485Send(message)) { }
      Serial.println("TX: " + message);

      lastSend = millis();
    }

    /**
     * Check if a message is received.
     */
    if (opta.rs485Incoming()) {

      /**
       * Read last mesasge.
       *
       * Read the message, erase it and allow opta to receive another message.
       */
      String message = opta.rs485Received();
      Serial.println("RX: " + message);
    }
  }
}
