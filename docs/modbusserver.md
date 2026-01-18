# Arduino Opta Industrial IoT gateway

[![Release](https://img.shields.io/github/v/release/JcDenis/opta2iot?color=lightblue)](https://github.com/JcDenis/opta2iot/releases)
[![Issues](https://img.shields.io/github/issues/JcDenis/opta2iot)](https://github.com/JcDenis/opta2iot/issues)
[![Requests](https://img.shields.io/github/issues-pr/JcDenis/opta2iot)](https://github.com/JcDenis/opta2iot/pulls)
[![License](https://img.shields.io/github/license/JcDenis/opta2iot?color=white)](https://github.com/JcDenis/opta2iot/blob/master/LICENSE)


## Modbus Server

* Holding Registers and Input Registers content same values.
* Length of Holding Registers and Input Registers are variable.
* The last Holding Register must be equal to the first Holding Register (total length)


### Length

Registers 0-9 are reserved for the length of others parts of Holding Registers.

* 0 : Total length of Holding Registers (last offset)
* 1 : Length of Commands part
* 2 : Length of Inputs part
* 3 : Length of Outputs part
* 4 : Length of Device part
* 5 : Length of Network part
* 6 : Length of MQTT part


### Commands

Registers 10-29 are reserved for Commands.

* 10 : Always 0
* 11 : Reboot device
* 12 : Reset device
* 13 : Update time (query NTP server)


### Inputs

Start offset is always 30.

* offset + 0 : Always 0
* offset + 1 : Number of Inputs
* offset + 2 : First Input type
* offset + 3 : First Input value 
* offset + 4 : Second Input Type
* offset + 5 : Second Input value
* ...


### Outputs

Start offset depends of length of previous registers, for Outputs it is 30 + Inputs length, or 30 + (offset 2).

* offset + 0 : Always 0
* offset + 1 : Number of Outputs
* offset + 2 : First Output type
* offset + 3 : First Output value
* offset + 4 : Second Output type
* offset + 5 : Second Ouput value
* ...


### Device

Start offset depends of length of previous registers, for Device it is 30 + Inputs length + Outputs length, or 30 + (offset 2) + (offset 3).

* offset + 0 : Always 0
* ... not yet set


### Network

Start offset depends of length of previous registers, for Network it is 30 + Inputs length + Outputs length + Device length, or 30 + (offset 2) + (offset 3) + (offset 4).

* offset + 0 : Always 0
* ... not yet set


### MQTT

Start offset depends of length of previous registers, for MQTT it is 30 + Inputs length + Outputs length + Device length + Network length, or 30 + (offset 2) + (offset 3) + (offset 4) + (offset 5).

* offset + 0 : Always 0
* offset + 1 : First part of MQTT server IP
* offset + 2 : Second part of MQTT server IP
* offset + 3 : Third part of MQTT server IP
* offset + 4 : Fourth part of MQTT server IP
* offset + 5 : MQTT server port
* offset + 6 : MQTT user login length
* offest + 7 : First byte (char) of MQTT user login
* offest + 8 : Second byte (char) of MQTT user login
* ...
* offset + 7 + MQTT user login length : MQTT user password length
* offset + 8 + MQTT user login length : First byte (char) of MQTT user password
* offset + 9 + MQTT user login length : Second byte (char) of MQTT user password
* ...
* offset + 7 + MQTT user login length + MQTT user password length : MQTT update interval


## Update device (server/slave) configuration

If a client change a value it must write all Holding Registers for the given part. 
To change a value on a given part, the first offset of this part must be set to 1.

For exemple if you want to change the MQTT server port, you must set again MQTT IP, user, password, interval.
Set length of other parts to 0, then continue only with MQTT part.

This should be looks like:

* 0 : 20 = Set total length to last offset
* 1 : 0 = Set length of Command part to 0
* 2 : 0 = Set length of Inputs part to 0
* 3 : 0 = Set length of Outputs part to 0
* 4 : 0 = Set length of Device part to 0
* 5 : 0 = Set length of Network part to 0
* 6 : 9 = Set length of MQTT part its sum (offset 10 to 18)
* 10 : 1 : Set first offset of the MQTT part to 1 to say you change this part
* 11 : 10 : Set first part off MQTT server IP to 10
* 12 : 1 : Set second part off MQTT server IP to 1
* 13 : 5 : Set third part off MQTT server IP to 5
* 14 : 110 : Set fourth part off MQTT server IP to 110
* 15 : 1883 : Set MQTT servre port to 1883
* 16 : 0 : Set MQTT server user login length to nothing if you do not use one
* 17 : 0 : Set MQTT server user password length to nothing if you do not use one
* 18 : 0 : Set MQTT update interval to 0 to disable auto publish
* 19 : 20 : The total length

Another exemple to reboot device



...
