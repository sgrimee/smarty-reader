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

#include "Arduino.h"
#include <string.h>

#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <AsyncMqttClient.h>

#include "smarty_user_config.h"
#include "debug_helpers.h"
#include "SmartyMeter.h"
#include "smarty_helpers.h"


#define READ_SMARTY_EVERY_S 10 // seconds

Ticker mqttReconnectTimer;
Ticker wifiReconnectTimer;
Ticker smartyDataReadTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;

AsyncMqttClient mqttClient;

#ifdef USE_FAKE_SMART_METER
#warning Using fake smart meter!
#endif

SmartyMeter smarty(decrypt_key, D3);


// MQTT

void connectToMqtt() {
  DEBUG_PRINTLN("Connecting to MQTT...");
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent) {
  DEBUG_PRINTLN("Connected to MQTT.");
  DEBUG_PRINT("Session present: ");
  DEBUG_PRINTLN(sessionPresent);
  publish_units_mqtt(smarty, mqttClient);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  DEBUG_PRINTLN("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void onMqttPublish(uint16_t packetId) {
  DEBUG_PRINTLN("Publish acknowledged.");
  DEBUG_PRINT("  packetId: ");
  DEBUG_PRINTLN(packetId);
}


// WIFI

void connectToWifi() {
  DEBUG_PRINTLN("Connecting to Wi-Fi...");
#ifdef HOSTNAME
  WiFi.hostname(HOSTNAME);
#endif
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  DEBUG_PRINTF("Connected to wifi with Hostname: %s\n", WiFi.hostname().c_str());
  connectToMqtt();
  smartyDataReadTimer.attach(READ_SMARTY_EVERY_S, read_smarty_data);
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  DEBUG_PRINTLN("Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  smartyDataReadTimer.detach(); // ensure we don't read smarty data if wifi is disconnected
  wifiReconnectTimer.once(2, connectToWifi);
}


// Initial setup
//
void setup()
{
  delay(500);
  pinMode(LED_BUILTIN, OUTPUT);   // Initialize outputs
  digitalWrite(LED_BUILTIN, LOW); // On
  DEBUG_BEGIN(115200);            // transmit-only UART for debugging on D4 (LED!)
  DEBUG_PRINTLN("\nSerial debug is working.");
  
  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);

  connectToWifi();

#ifdef USE_FAKE_SMART_METER
  smarty.setFakeVector((char *)fake_vector, sizeof(fake_vector));
#endif
  smarty.begin();
  
  digitalWrite(LED_BUILTIN, HIGH); // Off
}

void read_smarty_data()
{
  DEBUG_PRINTLN("\n------- Reading from Smarty");
  if (smarty.readAndDecodeData())
  {
    smarty.printDsmr();
    publish_dsmr_mqtt(smarty, mqttClient);
  }
  DEBUG_PRINTLN("Done reading.");
}

// Main loop
//
void loop()
{
  DEBUG_PRINT(".");
  delay(100);
}


/*
  Publish dsmr values (no units) to mqtt. 
*/
void publish_dsmr_mqtt(SmartyMeter &theSmarty, AsyncMqttClient &theClient)
{
  DEBUG_PRINTLN("Entering publish_dmsr_mqtt");
  char topic[70];
  for (int i = 0; i < theSmarty.num_dsmr_fields; i++)
  {
    sprintf(topic, "%s/%s/value", MQTT_TOPIC, dsmr[i].name);
    DEBUG_PRINTF("Publishing topic %s with value %s.\n", topic, dsmr[i].value);
    theClient.publish(topic, 0, false, dsmr[i].value);
  }
  DEBUG_PRINTLN("Exiting publish_dmsr_mqtt");
}

/*
  Publish the units to mqtt. Meant to be called just once after connecting.
*/
void publish_units_mqtt(SmartyMeter &theSmarty, AsyncMqttClient &theClient)
{
  DEBUG_PRINTLN("Entering publish_units_mqtt");
  char topic[70];
  for (int i = 0; i < theSmarty.num_dsmr_fields; i++)
  {
    #ifdef IGNORE_EMPTY_UNITS
    if (dsmr[i].unit[0] == 0)
    {
      DEBUG_PRINTF("Ignoring mqtt publish of unit for %s\n", dsmr[i].name);
      continue;
    }
    #endif
    sprintf(topic, "%s/%s/unit", MQTT_TOPIC, dsmr[i].name);
    DEBUG_PRINTF("Publishing topic %s with value %s\n", topic, dsmr[i].unit);
    theClient.publish(topic, 0, true, dsmr[i].unit); 
  }
}
