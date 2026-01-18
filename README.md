# Arduino Opta Industrial IoT gateway

[![Release](https://img.shields.io/github/v/release/JcDenis/opta2iot?color=lightblue)](https://github.com/JcDenis/opta2iot/releases)
[![Issues](https://img.shields.io/github/issues/JcDenis/opta2iot)](https://github.com/JcDenis/opta2iot/issues)
[![Requests](https://img.shields.io/github/issues-pr/JcDenis/opta2iot)](https://github.com/JcDenis/opta2iot/pulls)
[![License](https://img.shields.io/github/license/JcDenis/opta2iot?color=white)](https://github.com/JcDenis/opta2iot/blob/master/LICENSE)


## About

> Arduino sketch for the Arduino Opta series https://opta.findernet.com/fr/arduino

The goal of this sketch is to implement an easy to use MQTT gateway for industrial equipments.

**Based on "Remoto" (27-12-2024) by Alberto Perro under CERN-OHL-P license at https://github.com/albydnc/remoto**


## Features

* Support Opta RS485 AFX00001, Opta Wifi AFX00002, Opta Lite AFX00003 
* Partition formating (first boot or on demand) with Wifi firmware update
* Configurable Ethernet with DHCP or static IP
* Configurable Wifi STA or AP and with DHCP or static IP
* Configurable bidirectionnal MQTT Client with password support 
* Configurable inputs (pulse, digital, analog)
* RS485 helpers
* Serial commands
* Password protected Web server for visualization and configuration
* Persistent configuration storage in flash memory
* Watchdog (variable between network reconnection and loop)
* Lots of simple methods to deal with inputs/outputs/storage...
* Lite and fast loop to keep MQTT publishing of input change state under 50ms


## To do

* Support for OTA update
* Support for Modbus
* Support for device expansions boards


## USAGE

### Network

**Wifi AP MODE**  
If Wifi SSID is not configured and Wifi is set as prefered network, the Wifi goes into Access Point mode.  
Default IP is `192.168.1.231`, default SSID is `opta99999` and password is `opta2iot`.

**Wifi STA MODE**  
If Wifi SSID and password are configured and Wifi is set as prefered network, the wifi goes into Standard mode.

**Ethernet mode**  
If Ethernet is set as prefered network, wifi is disbaled.

**DHCP**  
If DHCP mode is enabled in configuration, connection is tried to be established with dynamic IP, 
else configured static IP address is used.

Network settings are configured in setup process and can not be changed without a device reboot.

### Serial

This sketch display activity on serial port and also support several commands.  
These commands are not case sensitive.

* `CONFIG ` 	: Send to serial monitor the user config
* `DHCP`    	: Switch ethernet DHCP mode in configuration
* `FORMAT`  	: Create/format partitions (and reboot)
* `INFO`    	: Send to serial monitor the board informations
* `IP`      	: Send to serial monitor the device IP
* `LOOP`    	: Send to serial monitor the number of loops per second
* `PUBLISH` 	: Publish to MQTT device and inputs state
* `REBOOT`  	: Reboot device
* `RESET`       : Reset config to default
* `TIME`        : Send to monitor the local time
* `UPDATE TIME` : Query NTP server to update local time
* `VERSION`		: Send to serial monitor the library version
* `WIFI`    	: Switch Wifi/Ethernet mode in configuration

You should do a `REBOOT` after `DHCP`, `WIFI`, `RESET` actions.

### MQTT

This sketch has MQTT client that supports MQTT 3.3.1 and broker with password credentials.

MQTT broker IP, port, user, password, topic, delay can be configured through web server interface or config.h file.

Publishing input state and device information topics:
* `<base_topic>/<device_id>/Ix/val` for input value
* `<base_topic>/<device_id>/Ix/type` for input type (0 = analog, 1 = digital, 2 = pulse)
* `<base_topic>/<device_id>/device/type` for the device type (Opta Lite...)
* `<base_topic>/<device_id>/device/ip` for the device current IP
* `<base_topic>/<device_id>/device/version` for the device installed sketch version

Command output state and device information topics:
* `<base_topic>/<device_id>/Ox` for output value with `0` = OFF, `1` = ON
* `<base_topic>/<device_id>/device/get` to force device information publishing (value doesn't matter)

Input state can also be published on demand by sending an HTTP request to the `/publish` URL.

Each **INPUTS** can be set in three ways:
* `ANALOG` : Send value between 0 and 10
* `DIGITAL` : Send value 0 or 1
* `PULSE` : Send only value 1, ideal for counting

### Web server

This sketch provides a web interface for visualization and configuration through a web server with basic authentication.
Web server is available in both Ethernet and Wifi mode.

* Default static IP address for web server is `192.168.1.231`
* Default user and password for web interface are `admin`:`admin` and can be changed in configuration.

Available web server entrypoints are:
* `GET /` : HTML visualization page
* `POST /form` : send new json data to this URL to udpate configuration
* `GET /config` : json configuration data
* `GET /device` : HTML device configration page
* `GET /style.css` : CSS for HTML pages
* `GET /favicon.ico` : Icon for HTML pages
* `GET /publish` : Publish to MQTT device and inputs state

**Note:** All pages require basic authentication !

### LED

During boot:
* Fast blink Green : Waiting for user to press button to fully reset device
* Fast blink Green to Red : Device is going to reboot

After boot:
* Fix Green and Red with no blue : Connecting networks (this freeze device)
* Fast blink Green to Red : Device is going to reboot
* Fast short blink Green and Red : heartbeat
* Slow blink Red : No network connection
* Slow blink Green : MQTT connection OK
* No Green and no Red : Network connection OK but no MQTT connection

Wifi device:
* Fix Blue : Wifi in STA mode
* Slow blink Blue : Wifi in AP mode
* No Blue : Not in Wifi mode

### Button

During boot:
* On fast blink red LEDS, user can reset device to default by pressing button more than 5 seconds

After boot:
* User can reset device to default by pressing button more than 5 seconds
* Without network, user can switch WIFI mode by pressing button around 2 seconds
* Without network, user can change DHCP mode by pressing button less than 1 second
* With network and MQTT connection, user can force publishing input state to MQTT by pressing button less than 1 second

Note that actions take effect on button release. WIFI and DHCP actions reboot device.

### Watchdog

A configurable watchog is present to reboot device on problem. 
There are two timeout that switch automatically :
* Configurable timeout for the loop (should be greater than 1 second)
* Fixed maximum timeout for the setup and for long operation like network connection.

Maximum timeout of Opta board is 32270 milliseconds and cannot be stop.

### USB

There is a bug on USB, there is no way to detect if cable is disconnected. 
And on cable disconnetion, board reboot afeter about 10 seconds.


## ARDUINO IDE

For Arduino Finder Opta on its M7 core.

### Boards manager

From Arduino IDE menu: _Tools > Boards > Boards Manager_, you must install: 

* `Arduino Mbed OS Opta Boards` by Arduino

### Library manager

From Arduino IDE menu: _Tools > Manage libraries_, you must install: 

* `ArduinoHttpClient` by Arduino at https://github.com/arduino-libraries/ArduinoHttpClient
* `ArduinoMqttClient` by Arduino at https://github.com/arduino-libraries/ArduinoMqttClient
* `ArduinoJson` by Benoit Blanchon at https://github.com/bblanchon/ArduinoJson.git
* `ArduinoRS485` by Arduino at https://github.com/arduino-libraries/ArduinoRS485
* `base64` by Densaugeo at https://github.com/Densaugeo/base64_arduino

### Settings

* Tools > Boards > Arduino Mbed OS Opta Boards > Opta
* Tools > Security > None
* Tools > Flash split > 2Mb M7
* Tools > Target core > Main Core

### Install

* Copy folder `opta2iot` to your Arduino IDE `libraries` folder, 
* Restart your Arduino IDE
* Select your Opta board and port
* In menu go to: _file > Examples > Examples from Custom Libraries > Opta Industrial IoT_ and select an example.
* Upload sketch to your Opta board. Enjoy.

To use opta2iot in your sketch, follow steps above, add this line at the begining of your sketch:

`#include "opta2iot.h"`


## CONTRIBUTORS

* Alberto Perro (source author)
* Jean-Christian Paul Denis (opta2iot author)

You are welcome to contribute to this code.

## LICENSE

CERN-OHL-P-2.0 license