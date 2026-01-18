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

#ifndef OPTA2IOT_LABEL_H
#define OPTA2IOT_LABEL_H

namespace opta2iot {

const char label_main_reset[] = "Resetting device";
const char label_main_reboot[] = "Rebooting device";
const char label_main_thread[] = "Starting threaded loop";
const char label_setup_end[] = "Setup completed \\o/";

const char label_watchdog_start[] = "Starting watchdog";

const char label_serial_setup[] = "\n+—————————————————————————————————————+\n| Arduino Opta Industrial IoT gateway |\n+—————————————————————————————————————+\n\n";
const char label_serial_line[] = "* ";
const char label_serial_info[] = " > ";
const char label_serial_warn[] = "!> ";
const char label_serial_reboot[] = "You should reboot device";
const char label_serial_cmd_ip[] = "Getting local IP address";
const char label_serial_cmd_time[] = "Getting local time";
const char label_serial_cmd_registers[] = "Reading Modbus server Input Registers";
const char label_serial_cmd_registers_end[] = "End of Input registers";
const char label_serial_receive[] = "Receiving serial message: ";

const char label_board_setup[] = "Configuring board";
const char label_board_name[] = "Board name: ";
const char label_board_name_none[] = "Unknown board name ";
const char label_board_name_lite[] = "Arduino OPTA Lite - AFX00003 ";
const char label_board_name_rs485[] = "Arduino OPTA RS485 - AFX00001";
const char label_board_name_wifi[] = "Aruidno OPTA Wifi - AFX00002 ";
const char label_board_error[] = "Failed to find board type";
const char label_board_reset[] = "Last reset reason: ";

const char label_flash_setup[] = "Configuring flash memory";
const char label_flash_init_error[] = "QSPI initialization failed";
const char label_flash_format[] = "Formatting partition: ";
const char label_flash_format_error[] = "Error formatting partition";
const char label_flash_erase_wait[] = "Erasing partitions, please wait...";
const char label_flash_erase_done[] = "Erase completed";
const char label_flash_missing[] = "Missing partition: ";
const char label_flash_existing[] = "Existing partition: ";
const char label_flash_firmware[] = "Flashing firmware";
const char label_flash_firmware_error[] = "Error writing firmware data";
const char label_flash_certificate[] = "Flashing certificate";
const char label_flash_certificate_error[] = "Error writing certificates";
const char label_flash_mapped[] = "Flashing memory mapped WiFi firmware";
const char label_flash_mapped_error[] = "Error writing memory mapped firmware";

const char label_store_read_fail[] = "Failed to read stored file";

const char label_led_setup[] = "Configuring User LEDs";
const char label_led_green[] = "Set Green LED on pin: ";
const char label_led_red[] = "Set Red LED on pin ";
const char label_led_blue[] = "Set Blue LED on pin: ";
const char label_led_heartbeat[] = "I'm alive at ";

const char label_button_setup[] = "Configuring buttons";
const char label_button_user[] = "Set user button on pin: ";
const char label_button_duration[] = "Button was activated: ";

const char label_config_setup[] = "Configuring parameters";
const char label_config_hold[] = "Hold for 5 seconds the user button to fully reset device. Waiting...";
const char label_config_json_read[] = "Reading configuration from JSON";
const char label_config_json_read_error[] = "Failed to parse JSON";
const char label_config_json_uncomplete[] = "Missing required keys in JSON";
const char label_config_default_read[] = "Loading default configuration";
const char label_config_file_write[] = "Writing configuration to flash memory";
const char label_config_file_read[] = "Reading configuration from flash memory";
const char label_config_file_error[] = "Configuration file not found";
const char label_config_set_deviceid[] = "Set device id to: ";
const char label_config_set_deviceuser[] = "Set device user to: ";
const char label_config_set_devicepassword[] = "Set device password to: ";
const char label_config_set_timeoffset[] = "Set time offset to: ";
const char label_config_set_networkip[] = "Set network IP to: ";
const char label_config_set_networkgateway[] = "Set network Gateway to: ";
const char label_config_set_networksubnet[] = "Set network Subnet to: ";
const char label_config_set_networkdns[] = "Set network DNS to: ";
const char label_config_set_networkdhcp[] = "Set network DHCP to: ";
const char label_config_set_networkwifi[] = "Set network Wifi to: ";
const char label_config_set_networkssid[] = "Set network SSID to: ";
const char label_config_set_networkpassword[] = "Set network password to: ";
const char label_config_set_mqttip[] = "Set MQTT server IP: ";
const char label_config_set_mqttport[] = "Set MQTT server port: ";
const char label_config_set_mqttuser[] = "Set MQTT user: ";
const char label_config_set_mqttpassword[] = "Set MQTT password: ";
const char label_config_set_mqttbase[] = "Set MQTT base topic: ";
const char label_config_set_mqttinterval[] = "Set MQTT interval: ";
const char label_config_set_modbustype[] = "Set Modbus mode: ";
const char label_config_set_modbusid[] = "Set Modbus RTU device ID: ";
const char label_config_set_modbusip[] = "Set Modbus TCP server IP: ";
const char label_config_set_modbusport[] = "Set Modbus TCP server port: ";

const char label_io_setup[] = "Configuring IO";
const char label_io_resolution[] = "Set IO resolution to: ";
//...

const char label_rs485_setup[] = "Configuring RS485";
const char label_rs485_none[] = "RS485 is disabled";

const char label_modbus_setup[] = "Configuring Modbus";
const char label_modbus_none[] = "Modbus is disabled";
const char label_modbus_server[] = "As server";
const char label_modbus_client[] = "As client";
const char label_modbus_rtu[] = "Using RTU";
const char label_modbus_tcp[] = "Using TCP";
const char label_modbus_start_error[] = "Failed to start Modbus";
const char label_modbus_registers_size[] = "Size of modbus Holdings Registers: ";
const char label_modbus_registers_change[] = "Modbus Holding Registers change";

const char label_network_setup[] = "Configuring network";
const char label_network_mode[] = "Set network mode as: ";
const char label_network_fail[] = "Communication with network module failed";
const char label_network_dhcp_ip[] = "DHCP attributed IP is: ";
const char label_network_static_ip[] = "Using static IP: ";
const char label_network_ssid[] = "Using SSID and password: ";
const char label_network_ap_fail[] = "Failed to create Wifi Access Point";
const char label_network_ap_success[] = "Wifi access point listening";
const char label_network_ap_plug[] = "Device connected to Access Point";
const char label_network_ap_unplug[] = "Device disconnected from Access Point";
const char label_network_eth_fail[] = "Network connection failed";
const char label_network_eth_success[] = "Network connected with IP: ";
const char label_network_eth[] = "Connecting Ethernet network";
const char label_network_eth_plug[] = "Ethernet cable connected";
const char label_network_eth_unplug[] = "Ethernet cable disconnected";
const char label_network_sta[] = "Connecting Wifi Standard network";
const char label_network_sta_fail[] = "Failed to connect Wifi";
const char label_network_sta_success[] = "Wifi connected";

const char label_time_setup[] = "Configuring time";
const char label_time_loop_start[] = "Getting loop time";
const char label_time_loop_line[] = "Loops per second: ";
const char label_time_loop_average[] = "Average of loops per second: ";
const char label_time_update[] = "Updating local time";
const char label_time_update_fail[] = "Failed to update local time";
const char label_time_update_success[] = "Time set to: ";

const char label_mqtt_setup[] = "Configuring MQTT client";
const char label_mqtt_server[] = "Using broker: ";
const char label_mqtt_broker[] = "Connecting to MQTT broker";
const char label_mqtt_broker_fail[] = "Failed to connect to MQTT broker";
const char label_mqtt_broker_success[] = "MQTT broker found";
const char label_mqtt_subscribe[] = "Subcribed to MQTT topic: ";
const char label_mqtt_receive[] = "Receiving MQTT command: ";
const char label_mqtt_publish_device[] = "Publishing device informations to MQTT";
const char label_mqtt_publish_inputs[] = "Publishing inputs informations to MQTT";

const char label_web_setup[] = "Configuring web server";
const char label_web_ethernet[] = "Creating Ethernet Web server";
const char label_web_wifi[] = "Creating Wifi Web server";
const char label_web_config[] = "Parsing received configuration";
const char label_web_config_fail[] = "Failed to load configuration from response";
const char label_web_config_fail_id[] = "Missing device ID";
const char label_web_config_fail_user[] = "Missing device user";
const char label_web_config_keep_device[] = "Get previous device password";
const char label_web_config_keep_wifi[] = "Get previous Wifi password";
const char label_web_config_keep_mqtt[] = "Get previous MQTT password";

}

#endif // OPTA2IOT_LABEL_H