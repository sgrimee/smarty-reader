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
  way to debug is to use debugV() and connect RX of an USB2Serial
  adapter (FTDI, Profilic, CP210, ch340/341) to D4 and use a terminal program
  like CuteCom or CleverTerm to listen to D4.
*/

#include "Arduino.h"
#include <string.h>

#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <AsyncMqttClient.h>

#define WEBSOCKET_DISABLED true
#include "shared_remote_debug.h"
#include "RemoteDebug.h"

#include "smarty_user_config.h"
#include "SmartyMeter.h"

#define READ_SMARTY_EVERY_S 59 // seconds
#define DATA_REQUEST_PIN D3

Ticker mqttReconnectTimer;
Ticker wifiReconnectTimer;
Ticker smartyDataReadTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;

AsyncMqttClient mqttClient;

#ifdef USE_FAKE_SMART_METER
#warning Using fake smart meter!
#endif

SmartyMeter smarty(decrypt_key, DATA_REQUEST_PIN);


// MQTT

void connectToMqtt() {
  debugD("Connecting to MQTT...");
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent) {
  debugI("Connected to MQTT.");
  publish_units_mqtt(smarty, mqttClient);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  debugW("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}


// WIFI

void connectToWifi() {
#ifdef HOSTNAME
  WiFi.hostname(HOSTNAME);
#endif
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Debug.begin(HOSTNAME);
  Debug.setResetCmdEnabled(true); // Enable the reset command

  connectToMqtt();
  smartyDataReadTimer.attach(READ_SMARTY_EVERY_S, read_smarty_data);
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
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
  
  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
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
  debugD("\n------- Reading from Smarty");
  if (smarty.readAndDecodeData())
  {
    smarty.printDsmr();
    publish_dsmr_mqtt(smarty, mqttClient);
  }
  debugV("Done reading.");
}

// Main loop
//
void loop()
{
  Debug.handle();
  debugI(".");
  delay(500);
}


/*
  Publish dsmr values (no units) to mqtt. 
*/
void publish_dsmr_mqtt(SmartyMeter &theSmarty, AsyncMqttClient &theClient)
{
  debugV("Entering publish_dmsr_mqtt");
  char topic[70];
  for (int i = 0; i < theSmarty.num_dsmr_fields; i++)
  {
    sprintf(topic, "%s/%s/value", MQTT_TOPIC, dsmr[i].name);
    debugD("Publishing topic %s with value %s.\n", topic, dsmr[i].value);
    theClient.publish(topic, 0, false, dsmr[i].value);
  }
  debugV("Exiting publish_dmsr_mqtt");
}

/*
  Publish the units to mqtt. Meant to be called just once after connecting.
*/
void publish_units_mqtt(SmartyMeter &theSmarty, AsyncMqttClient &theClient)
{
  debugV("Entering publish_units_mqtt");
  char topic[70];
  for (int i = 0; i < theSmarty.num_dsmr_fields; i++)
  {
    if (dsmr[i].unit[0] == 0)
    {
      debugV("Ignoring mqtt publish of unit for %s\n", dsmr[i].name);
      continue;
    }
    sprintf(topic, "%s/%s/unit", MQTT_TOPIC, dsmr[i].name);
    debugD("Publishing topic %s with value %s\n", topic, dsmr[i].unit);
    theClient.publish(topic, 0, true, dsmr[i].unit); 
  }
}
