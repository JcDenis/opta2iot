/* opta2iot
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

#ifndef OPTA2IOT_DEFINE_H
#define OPTA2IOT_DEFINE_H

/* Default values.
 *
 * Default values are used if configuration file doesn't exist in flash memory or if some values are erroneous.
 * These values can be changed from serial command or web page later.
 */

// Device
#define OPTA2IOT_DEVICE_ID "99999"
#define OPTA2IOT_DEVICE_USER "admin"
#define OPTA2IOT_DEVICE_PASSWORD "admin"

// Network
#define OPTA2IOT_NET_DHCP false
#define OPTA2IOT_NET_IP "192.168.1.231"
#define OPTA2IOT_NET_GATEWAY "192.168.1.1"
#define OPTA2IOT_NET_SUBNET "255.255.0.0"
#define OPTA2IOT_NET_DNS "4.4.4.4"
#define OPTA2IOT_NET_WIFI true
#define OPTA2IOT_NET_SSID ""
#define OPTA2IOT_NET_PASSWORD ""

// MQTT
#define OPTA2IOT_MQTT_IP "192.168.1.100"
#define OPTA2IOT_MQTT_PORT 1883
#define OPTA2IOT_MQTT_USER "mqtt_user"
#define OPTA2IOT_MQTT_PASSWORD "mqtt_password"
#define OPTA2IOT_MQTT_BASE "/opta/"
#define OPTA2IOT_MQTT_INTERVAL 0

// Time
#define OPTA2IOT_TIME_OFFSET 0
#define OPTA2IOT_TIME_SERVER "pool.ntp.org"

// Serial
#define OPTA2IOT_SERIAL_VERBOSE true;

// IO
#define OPTA2IOT_IO_RESOLUTION 16 // no_config, analog resolution
#define OPTA2IOT_IO_POLL 50   // no_config. In milliseconds, inputs poll loop delay

// SERIAL
#define OPTA2IOT_SERIAL_BAUDRATE 115200 // no_config, serial port speed (USB)

// RS485
#define OPTA2IOT_RS485_BAUDRATE 19200 // no_config, rs485 speed

// Network
#define OPTA2IOT_NETWORK_POLL 60  // no_config. In milliseconds, network client/server connection retry delay
#define OPTA2IOT_NETWORK_TIMEOUT 5000 // no_config. In milliseconds, network/mqtt/... connection timeout

// Other
#define OPTA2IOT_WATCHDOG_TIMEOUT 5000 // no_config. In milliseconds, freeze time before device reboot
// Note: watchdog is set to max on somes task (for example on ethernet connection)

#endif  // #ifndef OPTA2IOT_DEFINE_H