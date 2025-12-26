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

#pragma once

#define DEFAULT_DEVICE_ID "99999"
#define DEFAULT_DEVICE_USER "admin"
#define DEFAULT_DEVICE_PASSWORD "admin"

#define DEFAULT_MQTT_IP "192.168.1.100"
#define DEFAULT_MQTT_PORT 1883
#define DEFAULT_MQTT_USER "mqtt_user"
#define DEFAULT_MQTT_PASSWORD "mqtt_password"
#define DEFAULT_MQTT_BASE "/opta/"
#define DEFAULT_MQTT_INTERVAL 0

#define DEFAULT_NET_DHCP false
#define DEFAULT_NET_IP "192.168.1.231"
#define DEFAULT_NET_WIFI true
#define DEFAULT_NET_SSID ""
#define DEFAULT_NET_PASSWORD ""

#define DEFAULT_TIME_OFFSET 0