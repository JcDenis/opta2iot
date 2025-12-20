# sopta2iot by Jean-Christian Paul Denis


## About

> Arduino sketch for the Arduino Opta Lite (without Wifi nor RS485)

The goal of this sketch is to implement an easy to use MQTT gateway for industrial equipments.

Based on "Remoto" 
Author: Alberto Perro
Date: 27-12-2024
License: CERN-OHL-P
Source: https://github.com/albydnc/remoto


## Features

* Partition formatting (first boot or on demand)
* Configurable Ethernet with DHCP or static IP
* Configurable MQTT Client
* Configurable inputs (pulse, digital, analog)
* Serial commands
* Password protected Web server for visualization and configuration
* Peristent configration storage in flash memory
* lite and fast loop to keep MQTT publishing of input change state under 50ms


## To do

* OTA support
* Integrate others Opta series (wifi, rs485)
* Enhance timeout on EthernetServer begin
* (re)implement scheduler


## USAGE

### Serial

This sketch display activity on serial port and also support several commands :

* CONFIG : Get user config
* DHCP   : Switch ethernet DHCP mode
* FORMAT : Create/format partitions
* INFO   : Get board informations
* IP     : Get ethernet IP
* LOOP   : Get loops per second
* REBOOT : Reboot device
* RESET  : Reset config to default

### MQTT

This sketch has MQTT client that supports MQTT 3.3.1 and broker with password credentials.

MQTT broker IP, port, user, password, topic, delay can be configured through web server interface or config.h file.

Publishing input state and device information topics:
* `<base_topic>/<device_id>/Ix/val` for input value
* `<base_topic>/<device_id>/Ix/type` for input type (0 = analog, 1 = digital, 2 = pulse)
* `<base_topic>/<device_id>/device/type` for the device type (Opta Lite...)
* `<base_topic>/<device_id>/device/ip` for the device current IP
* `<base_topic>/<device_id>/device/version` for tehe device installed sketch version

Command output state and device information topics:
* `<base_topic>/<device_id>/Ox` for output value with `0` = OFF, `1` = ON
* `<base_topic>/<device_id>/device/get` to force device information publishing (value doesn't matter)

Input state can also be published on demand by sending an HTTP request to the `/publish` URL.

### Web server

This sketch provides a web interface for visualization and configuration through a web server with basic authentication.

* Default static IP address for web server is 192.168.1.231
* Default user and password for web interface are admin:admin and can be changed in configuration.

Available web server entrypoints are:
* `GET /` : HTML visualization page
* `POST /form` : send new json data to this URL to udpate configuration
* `GET /config` : json configuration data
* `GET /device` : HTML device configration page
* `GET /style.css` : CSS for HTML pages
* `GET /favicon.ico` : Icon for HTML pages

**Note:** All pages require basic autehntication !

### LED

* Short blink top left green led on boot means user can push button to reset configuration to default
* Blink top left red LED means no network
* Blink top left green led means mqtt connected
* No top left led means netwrok OK but no mqtt connection.

### Button

* During boot (when top left green led blink) user can reset configuration to default by pressing button
* With Network and MQTT connection, user can force publishing input state by pressing button
* Without network cable, user can change DHCP mode by pressing button.


## ARDUINO IDE

For Arduino Finder Opta Lite on its M7 core.

### Boards manager

* `Arduino Mbed OS Opta Boards` by Arduino

### Library manager

* `ArduinoJson` by Benoit Blanchon at https://github.com/bblanchon/ArduinoJson.git
* `MQTT` by Joel Gaehwiler at https://github.com/256dpi/arduino-mqtt
* `base64` by Densaugeo at https://github.com/Densaugeo/base64_arduino

### Settings

* Tools > Boards > Arduino Mbed OS Opta Boards > Opta
* Tools > Security > None
* Tools > Flash split > 2Mb M7
* Tools > Target core > Main Core