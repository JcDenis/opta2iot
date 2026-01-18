#include <Arduino.h>
#include "opta2iot.h"

opta2iot::Opta opta;

unsigned long lastPoll = 0;

void setup() {
  if (opta.setup()) {
    opta.thread();
  }
}

void loop() {
  if (opta.running()) {
    // Poll second Opta every 30000
    if (millis() - lastPoll > 30000) {
      lastPoll = millis();
      int index = 0;

      const byte server = 98;
      int response[100];

      /**
       * Read all Hoding Registers from Opta servers to get length of interesting parts.
       */
      if (opta.modbusGetInputRegisters(response, server, 0, 10)) {

        // Print parts length
        Serial.println(">>> Reading parts length <<<");
        Serial.println("- Size of Holding Registers: " + String(response[0]));
        Serial.println("- Size of Commands part: " + String(response[1]));
        Serial.println("- Size of Inputs part: " + String(response[2]));
        Serial.println("- Size of Outputs part: " + String(response[3]));
        Serial.println("- Size of Device part: " + String(response[4]));
        Serial.println("- Size of Network part: " + String(response[5]));
        Serial.println("- Size of MQTT part: " + String(response[6]));
        Serial.println();

        // To read Inputs, skip length descriptions and skip Commands
        int read_offset = 10 + response[1];

        // We want to read Input and Outputs
        int read_length = response[2] + response[3];

        // Read only interresting parts.
        if (opta.modbusGetInputRegisters(response, server, read_offset, read_length)) {
          read_offset = 0;

          // Read Inputs values.
          Serial.println(">>> Reading Inputs <<<");

          read_offset++; // skip first offset of this part
          const int inputs_num = response[read_offset++];

          for (index = 0; index < inputs_num; index++) {
              Serial.print("- Inputs " + String(index + 1));
              Serial.print(", Type = " + String(response[read_offset++]));
              Serial.println(", Value = " + String(response[read_offset++]));
          }
          Serial.println("");

          // Read Outputs values.
          Serial.println(">>> Reading Outputs <<<");

          read_offset++; // skip first offset of this part
          const int outputs_num = response[read_offset++];

          for (index = 0; index < outputs_num; index++) {
              Serial.print("- Outputs " + String(index + 1));
              Serial.print(", Type = " + String(response[read_offset++]));
              Serial.println(", Value = " + String(response[read_offset++]));
          }
          Serial.println("");
        }
      }

      Serial.println(".");
    }
  }
}
