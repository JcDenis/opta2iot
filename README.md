# Arduino Opta Industrial IoT gateway

[![Release](https://img.shields.io/github/v/release/jcdenis/opta2iot?color=lightblue)](https://github.com/JcDenis/opta2iot/releases)
![Date](https://img.shields.io/github/release-date/jcdenis/opta2iot?color=red)
[![License](https://img.shields.io/github/license/jcdenis/opta2iot?color=white)](https://github.com/JcDenis/opta2iot/blob/master/LICENSE)


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
* Serial commands
* Password protected Web server for visualization and configuration
* Peristent configration storage in flash memory
* Lite and fast loop to keep MQTT publishing of input change state under 50ms


## To do

* OTA support
* Enhance timeout/freeze on all client or server connections
* Fix freeze on web authentication
* Add support for RS485
* Add support for Modbus


## USAGE

### Network

**Wifi AP MODE**  
If Wifi SSID is not configured and Wifi is set as prefered network, the Wifi goes into AP mode.  
Default IP is `192.168.1.231`, default SSID is `opta99999` and password is `admin`.

**Wifi STA MODE**  
If Wifi SSID and password are configured and Wifi is set as prefred network, the wifi goes into STA mode.

**Ethernet mode**  
If Ethernet is set as prefered network, wifi is disbaled.

**DHCP**  
If DHCP mode is enabled in configuration, connection is tried to be established with dynamic IP, 
else configured static IP address is used.

### Serial

This sketch display activity on serial port and also support several commands.  
These commands are not case sensitive.

* `CONFIG ` : Get user config
* `DHCP`    : Switch ethernet DHCP mode (and reboot)
* `FORMAT`  : Create/format partitions (and reboot)
* `INFO`    : Get board informations
* `IP`      : Get ethernet IP
* `LOOP`    : Get loops per second
* `PUBLISH` : Publish to MQTT device and inputs state
* `REBOOT`  : Reboot device
* `RESET`   : Reset config to default (and reboot)
* `WIFI`    : Switch Wifi/Ethernet mode (and reboot)

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
* Fix Green and Red : Waiting for user to press button to fully reset device
* Fast blink Green to Red : Device is going to reboot

After boot:
* Fix Green and fix Red with no blue : Connecting networks (this freeze device)
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

* During boot, on fix green and red LEDS, user can reset device to default by pressing button
* With Network and MQTT connection, user can force publishing input state by pressing button
* With network cable disconnected, user can change DHCP mode by pressing button.


## ARDUINO IDE

For Arduino Finder Opta on its M7 core.

### Boards manager

* `Arduino Mbed OS Opta Boards` by Arduino

### Library manager

* `ArduinoHttpClient` by Arduino at https://github.com/arduino-libraries/ArduinoHttpClient
* `ArduinoJson` by Benoit Blanchon at https://github.com/bblanchon/ArduinoJson.git
* `MQTT` by Joel Gaehwiler at https://github.com/256dpi/arduino-mqtt
* `base64` by Densaugeo at https://github.com/Densaugeo/base64_arduino

### Settings

* Tools > Boards > Arduino Mbed OS Opta Boards > Opta
* Tools > Security > None
* Tools > Flash split > 2Mb M7
* Tools > Target core > Main Core


## CONTRIBUTORS

* Alberto Perro (source author)
* Jean-Christian Paul Denis (opta2iot author)

You are welcome to contribute to this code.

## LICENSE

CERN-OHL-P-2.0 license