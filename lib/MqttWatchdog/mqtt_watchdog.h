/*
   The system yield() and delay() routines do not call mqttClient.loop()
*/

#ifndef MQTT_WATCHDOG_H
#define MQTT_WATCHDOG_H

#include <PubSubClient.h>

void mqtt_yield(PubSubClient &mqttClient);
void mqtt_delay(unsigned long millisecs, PubSubClient &mqttClient);

#endif