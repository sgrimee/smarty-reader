/*
  smartyreader.ino

  read p1 port from luxemburgish smartmeter,
  decode and publish with MQTT over Wifi
  using ESP8266 (Wemos D1 mini pro)


  Copyright (C) 2018 Guy WEILER www.weigu.lu

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.


  Hardware serial (Serial) on Wemos D1 mini pro uses UART0 of ESP8266, which is
  mapped to pins TX (GPIO1) and RX (GPIO3). It is used to access the Smartmeter.
  You have to remove the Wemos from its socket to program it! because the
  hardware serial port is also used during programming.

  Serial1 uses UART1 which is a transmit-only UART. UART1 TX pin is D4 (GPIO2,
  LED!!). If you use serial (UART0) to communicate with hardware, you can't use
  the Arduino Serial Monitor at the same time to debug your program! The best
  way to debug is to use DEBUG_PRINTLN() and connect RX of an USB2Serial
  adapter (FTDI, Profilic, CP210, ch340/341) to D4 and use a terminal program
  like CuteCom or CleverTerm to listen to D4.
*/

#include "smarty_user_config.h"
#include "debug_helpers.h"
#include "SmartyMeter.h"

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include <string.h>

#define LOOP_DELAY 59000 // 59 seconds

#ifdef USE_WIFI
WiFiClient espClient;
#else
#warning Wifi is disabled
#endif

#ifdef USE_MQTT
PubSubClient mqttClient(espClient);
#else
#warning MQTT is disabled!
#endif

#ifdef USE_FAKE_SMART_METER
#warning Using fake smart meter!
#endif

char mqtt_msg[128];
SmartyMeter smarty(decrypt_key, D3);

/* 
    Arduino setup
*/
void setup()
{
  delay(1000);
  pinMode(LED_BUILTIN, OUTPUT);   // Initialize outputs
  digitalWrite(LED_BUILTIN, LOW); // On
  DEBUG_BEGIN(115200);            // transmit-only UART for debugging on D4 (LED!)
  DEBUG_PRINTLN("\nSerial is working.");
  setup_networking();

#ifdef USE_MQTT
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
#endif // USE_MQTT

#ifdef USE_FAKE_SMART_METER
  smarty.setFakeVector((char *)fake_vector, sizeof(fake_vector));
#endif
  smarty.begin();

  digitalWrite(LED_BUILTIN, HIGH); // Off
  delay(2000);
}

/*  
    Arduino main loop 
*/
void loop()
{
  DEBUG_PRINTLN("------------------");
#ifdef USE_MQTT
  if (!mqttClient.connected())
  {
    reconnect_mqtt();
  }
  mqttClient.loop();
#endif // USE_MQTT
  bool data_available = smarty.readAndDecodeData();
  if (data_available)
  {
    DEBUG_PRINTLN("loop: data received.");
    //create a message to post to mqtt
    snprintf(mqtt_msg, MQTT_MAX_PACKET_SIZE, "{"
                                             "\"dt\":\"%s\","
                                             "\"e1\":\"%s\","
                                             "\"pwr\":\"%s\","
                                             "\"gas\":\"%s\"}",
             smarty.timestamp,
             smarty.energy_delivered_tariff1,
             smarty.power_delivered,
             smarty.gas_index);
    DEBUG_PRINTLN("Message to publish:");
    DEBUG_PRINTLN(mqtt_msg);
#ifdef USE_MQTT
    mqttClient.publish(MQTT_TOPIC, mqtt_msg);
#endif // USE_MQTT
  }
  else
  {
    DEBUG_PRINTLN("loop: no data received");
  }

  DEBUG_PRINTLN("End of loop, waiting");
  delay(LOOP_DELAY);
}

/* Networking */

void setup_networking()
{
#ifdef USE_WIFI
  DEBUG_PRINTLN("Entering WiFi setup.");
  WiFi.softAPdisconnect(); // to eliminate Hotspot
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  delay(200);
#ifdef USE_WIFI_STATIC
  WiFi.config(wemos_ip, gateway_ip, subnet_mask);
#endif // USE_WIFI_STATIC
  WiFi.hostname(HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    DEBUG_PRINT(".");
  }
  DEBUG_PRINTLN("\nWiFi connected");
  DEBUG_PRINTLN("IP address: ");
  DEBUG_PRINTLN(WiFi.localIP());
  randomSeed(micros());
#endif // USE_WIFI
}

/* MQTT */

void reconnect_mqtt()
{
#ifdef USE_MQTT
  DEBUG_PRINTLN("Connecting to mqtt");
  while (!mqttClient.connected())
  {
    DEBUG_PRINT(".");
    if (mqttClient.connect(MQTT_CLIENT_ID))
    {
      DEBUG_PRINTLN("\nPublishing connection confirmation to mqtt.");
      mqttClient.publish(MQTT_TOPIC, "{\"dt\":\"connected\"}");
    }
    else
    {
      delay(5000);
    }
  }
#endif
}
