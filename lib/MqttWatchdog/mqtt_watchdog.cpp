#include "Arduino.h"
#include "mqtt_watchdog.h"

/*
   Need a local yield that that calls yield() and mqttClient.loop()
   The system yield() routine does not call mqttClient.loop()
*/

void mqtt_yield(PubSubClient &mqttClient)
{
    yield();
    mqttClient.loop();
}

/*
 *  Returns the number of milliseconds elapsed since  start_time_ms.
 */
unsigned long elapsed_time(unsigned long start_time_ms)
{
    return millis() - start_time_ms;
}

/*
   Need a local delay calls yield() and mqttClient.loop()
   The system delay() routine does not call mqttClient.loop()
*/
void mqtt_delay(unsigned long millisecs, PubSubClient &mqttClient)
{
    unsigned long start = millis();
    mqtt_yield(mqttClient);
    if (millisecs > 0)
    {
        while (elapsed_time(start) < millisecs)
        {
            mqtt_yield(mqttClient);
        }
    }
}