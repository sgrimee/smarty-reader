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

#include <AES.h>
#include <Crypto.h>
#include <ESP8266WiFi.h>
#include <GCM.h>
#include <PubSubClient.h>
#include <string.h>

#include "user_config.h"

// clang-format off
#ifndef DEBUG
  #define DEBUG_BEGIN(...)
  #define DEBUG_PRINT(...)
  #define DEBUG_PRINTLN(...)
#else
  #define DEBUG_BEGIN(...) Serial1.begin(__VA_ARGS__)
  #define DEBUG_PRINT(...) Serial1.print(__VA_ARGS__)
  #define DEBUG_PRINTLN(...) Serial1.println(__VA_ARGS__)  
#endif
// clang-format on

#define MAX_LINE_LENGTH 1024
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

const byte DATA_REQUEST_SM = D3; //active Low!
const int sll = 50;              // length of a line if printing serial raw data

struct TestVector
{
  const char *name;
  uint8_t key[16];
  uint8_t ciphertext[MAX_LINE_LENGTH];
  uint8_t authdata[17];
  uint8_t iv[12];
  uint8_t tag[16];
  uint8_t authsize;
  uint16_t datasize;
  uint8_t tagsize;
  uint8_t ivsize;
};

TestVector Vector_SM;
GCM<AES128> *gcmaes128 = 0;

char *identification;
char *p1_version;
char *timestamp;
char *equipment_id;
char *energy_delivered_tariff1;
char *energy_returned_tariff1;
char *reactive_energy_delivered_tariff1;
char *reactive_energy_returned_tariff1;
char *power_delivered;
char *power_returned;
char *reactive_power_delivered;
char *reactive_power_returned;
char *electricity_threshold;
char *electricity_switch_position;
char *electricity_failures;
char *electricity_sags_l1;
char *electricity_sags_l2;
char *electricity_sags_l3;
char *electricity_swells_l1;
char *electricity_swells_l2;
char *electricity_swells_l3;
char *message_short;
char *message2_long;
char *message3_long;
char *message4_long;
char *message5_long;
char *current_l1;
char *current_l2;
char *current_l3;

const char *id_p1_version = "1-3:0.2.8(";
const char *id_timestamp = "0-0:1.0.0(";
const char *id_equipment_id = "0-0:42.0.0(";
const char *id_energy_delivered_tariff1 = "1-0:1.8.0(";
const char *id_energy_returned_tariff1 = "1-0:2.8.0(";
const char *id_reactive_energy_delivered_tariff1 = "1-0:3.8.0(";
const char *id_reactive_energy_returned_tariff1 = "1-0:4.8.0(";
const char *id_power_delivered = "1-0:1.7.0(";
const char *id_power_returned = "1-0:2.7.0(";
const char *id_reactive_power_delivered = "1-0:3.7.0(";
const char *id_reactive_power_returned = "1-0:4.7.0(";
const char *id_electricity_threshold = "0-0:17.0.0(";
const char *id_electricity_switch_position = "0-0:96.3.10(";
const char *id_electricity_failures = "0-0:96.7.21(";
const char *id_electricity_sags_l1 = "1-0:32.32.0(";
const char *id_electricity_sags_l2 = "1-0:52.32.0(";
const char *id_electricity_sags_l3 = "1-0:72.32.0(";
const char *id_electricity_swells_l1 = "1-0:32.36.0(";
const char *id_electricity_swells_l2 = "1-0:52.36.0(";
const char *id_electricity_swells_l3 = "1-0:72.36.0(";
const char *id_message_short = "0-0:96.13.0(";
const char *id_message2_long = "0-0:96.13.2(";
const char *id_message3_long = "0-0:96.13.3(";
const char *id_message4_long = "0-0:96.13.4(";
const char *id_message5_long = "0-0:96.13.5(";
const char *id_current_l1 = "1-0:31.7.0(";
const char *id_current_l2 = "1-0:51.7.0(";
const char *id_current_l3 = "1-0:71.7const .0(";

uint8_t telegram[MAX_LINE_LENGTH];
char buffer[MAX_LINE_LENGTH - 30];
char msg[128], filename[13];
int test, Tlength;

void setup()
{
  delay(1000);

  pinMode(LED_BUILTIN, OUTPUT); // Initialize outputs
  pinMode(DATA_REQUEST_SM, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW); // On

  DEBUG_BEGIN(115200); // transmit-only UART for debugging on D4 (LED!)
  DEBUG_PRINTLN("\nSerial is working.");

  setup_networking();

#ifdef USE_MQTT
  mqttClient.setServer(mqtt_server, mqttPort);
#endif

  digitalWrite(LED_BUILTIN, HIGH); // Off

  delay(100);

  Serial.begin(115200); // Hardware serial connected to smarty
  Serial.setRxBufferSize(1024);

  delay(2000);
}

void loop()
{
  DEBUG_PRINTLN("------------------");
  DEBUG_PRINTLN("Loop begins");

#ifdef USE_MQTT
  if (!mqttClient.connected())
  {
    reconnect_mqtt();
  }
  mqttClient.loop();
// after mqtt is connected, we should use local_delay instead of delay
// to ensure the watchdog is fed correctly
#endif

  Tlength = readTelegram();
  if (Tlength == 0)
  {
    DEBUG_PRINTLN("No data received, waiting");
    local_delay(LOOP_DELAY);
    return;
  }
  debug_print_raw_data(Tlength);

  init_vector(&Vector_SM, "Vector_SM", key_SM_LAM_1);
  if (Vector_SM.datasize != 1024)
  {
    debug_print_vector(&Vector_SM);

    decrypt_text_to_buffer(&Vector_SM);

    parse_dsmr_string(buffer);
    debug_print_dsmr();

    // create a message to post to mqtt
    snprintf(msg, 128, "{"
                       "\"dt\":\"%s\","
                       "\"c1\":\"%s\","
                       "\"p1\":\"%s\","
                       "\"pwr\":\"%s\"}",

             timestamp,
             energy_delivered_tariff1,
             energy_returned_tariff1,
             power_delivered);
    DEBUG_PRINTLN("Message to publish:");
    DEBUG_PRINTLN(msg);
#ifdef USE_MQTT
    test = mqttClient.publish(topic, msg);
#endif
  }
  else
  {
    DEBUG_PRINTLN("Ignoring vector with MAX datasize");
  }

  DEBUG_PRINTLN("End of loop, waiting");
  local_delay(LOOP_DELAY);
}

/*
  Read data from the counter on the serial line
*/
int readTelegram()
{
  int cnt = 0;

  DEBUG_PRINTLN("Entering readTelegram");

  // initialise telegram buffer with 1 values and read data
  memset(telegram, '1', sizeof(telegram));

#ifdef USE_FAKE_SMART_METER
  cnt = sizeof(fake_vector);
  DEBUG_PRINTLN(cnt);
  memcpy(telegram, fake_vector, cnt);
#else
  digitalWrite(DATA_REQUEST_SM, LOW); // Request serial data On
  local_delay(10);
  while ((Serial.available()) && (cnt != MAX_LINE_LENGTH))
  {
    telegram[cnt] = Serial.read();
    cnt++;
  }
  digitalWrite(DATA_REQUEST_SM, HIGH); // Request serial data Off
#endif
  return (cnt);
}

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
#endif // ifdef STATIC
  WiFi.hostname(smartyreader_hostname);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    DEBUG_PRINT(".");
  }
  DEBUG_PRINTLN("\nWiFi connected");
  DEBUG_PRINTLN("IP address: ");
  DEBUG_PRINTLN(WiFi.localIP());

  randomSeed(micros());
#endif
}

void reconnect_mqtt()
{
#ifdef USE_MQTT
  DEBUG_PRINTLN("Connecting to mqtt");
  while (!mqttClient.connected())
  {
    DEBUG_PRINT(".");
    if (mqttClient.connect(clientId))
    {
      DEBUG_PRINTLN("\nPublishing connection confirmation to mqtt.");
      mqttClient.publish(topic, "{\"dt\":\"connected\"}");
    }
    else
    {
      delay(5000);
    } // Wait 5 seconds before retrying
  }
#endif
}

// Convert binary coded decimal to decimal number
byte bcdToDec(byte val) { return ((val / 16 * 10) + (val % 16)); }

// Convert decimal number to binary coded decimal
byte decToBcd(byte val) { return ((val / 10 * 16) + (val % 10)); }

/*
  Decode the raw data and fill the vector
*/
void init_vector(TestVector *vect, const char *Vect_name, uint8_t *key_SM)
{
  DEBUG_PRINTLN("Entering init_vector");
  vect->name = Vect_name; // vector name
  for (int i = 0; i < 16; i++)
    vect->key[i] = key_SM[i];
  uint16_t Data_Length = uint16_t(telegram[11]) * 256 + uint16_t(telegram[12]) - 17; // get length of data
  if (Data_Length > MAX_LINE_LENGTH)
    Data_Length = MAX_LINE_LENGTH;
  for (int i = 0; i < Data_Length; i++)
    vect->ciphertext[i] = telegram[i + 18];
  uint8_t AuthData[] = {0x30, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
                        0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE,
                        0xFF};
  for (int i = 0; i < 17; i++)
    vect->authdata[i] = AuthData[i];
  for (int i = 0; i < 8; i++)
    vect->iv[i] = telegram[2 + i];
  for (int i = 8; i < 12; i++)
    vect->iv[i] = telegram[6 + i];
  for (int i = 0; i < 12; i++)
    vect->tag[i] = telegram[Data_Length + 18 + i];
  vect->authsize = 17;
  vect->datasize = Data_Length;
  vect->tagsize = 12;
  vect->ivsize = 12;
  DEBUG_PRINTLN("Exiting init_vector");
}

/*
  Decrypt text in the vector and put it in the buffer
*/
void decrypt_text_to_buffer(TestVector *vect)
{
  DEBUG_PRINTLN("Entering decrypt_text");
  gcmaes128 = new GCM<AES128>();
  size_t posn, len;
  size_t inc = vect->datasize;
  memset(buffer, 0xBA, sizeof(buffer));
  gcmaes128->setKey(vect->key, gcmaes128->keySize());
  gcmaes128->setIV(vect->iv, vect->ivsize);
  for (posn = 0; posn < vect->datasize; posn += inc)
  {
    len = vect->datasize - posn;
    if (len > inc)
      len = inc;
    gcmaes128->decrypt((uint8_t *)buffer + posn, vect->ciphertext + posn, len);
  }
  delete gcmaes128;
  DEBUG_PRINTLN("Exiting decrypt_text");
}

/*
  Parse fields in mystgring (buffer) and store in global variables
*/
void parse_dsmr_string(char *mystring)
{
  DEBUG_PRINTLN("Entering parse_dsmr_string");
  DEBUG_PRINTLN("String to parse: ");
  DEBUG_PRINTLN(mystring);

  bool result;
  char *field = (char *)"bla";

  identification = strtok(mystring, "\n"); // get the first field

  while ((field[0] != '!') && (field != NULL))
  { // walk through other fields
    field = strtok(NULL, "\n");
    // DEBUG_PRINT("Testing field: ");
    // DEBUG_PRINTLN(field);
    if (strlen(field) < 10)
    {
      continue;
    }
    if (test_field(field, id_p1_version))
    {
      replace_field_by_value(field);
      p1_version = field;
      continue;
    }
    result = test_field(field, id_timestamp);
    if (result)
    {
      replace_field_by_value(field);
      timestamp = field;
      continue;
    }
    result = test_field(field, id_equipment_id);
    if (result)
    {
      replace_field_by_value(field);
      convert_equipment_id(field);
      equipment_id = field;
      continue;
    }
    result = test_field(field, id_energy_delivered_tariff1);
    if (result)
    {
      replace_field_by_value(field);
      energy_delivered_tariff1 = field;
      continue;
    }
    result = test_field(field, id_energy_returned_tariff1);
    if (result)
    {
      replace_field_by_value(field);
      energy_returned_tariff1 = field;
      continue;
    }
    result = test_field(field, id_reactive_energy_delivered_tariff1);
    if (result)
    {
      replace_field_by_value(field);
      reactive_energy_delivered_tariff1 = field;
      continue;
    }
    result = test_field(field, id_reactive_energy_returned_tariff1);
    if (result)
    {
      replace_field_by_value(field);
      reactive_energy_returned_tariff1 = field;
      continue;
    }
    result = test_field(field, id_power_delivered);
    if (result)
    {
      replace_field_by_value(field);
      power_delivered = field;
      continue;
    }
    result = test_field(field, id_power_returned);
    if (result)
    {
      replace_field_by_value(field);
      power_returned = field;
      continue;
    }
    result = test_field(field, id_reactive_power_delivered);
    if (result)
    {
      replace_field_by_value(field);
      reactive_power_delivered = field;
      continue;
    }
    result = test_field(field, id_reactive_power_returned);
    if (result)
    {
      replace_field_by_value(field);
      reactive_power_returned = field;
      continue;
    }
    result = test_field(field, id_electricity_threshold);
    if (result)
    {
      replace_field_by_value(field);
      electricity_threshold = field;
      continue;
    }
    result = test_field(field, id_electricity_switch_position);
    if (result)
    {
      replace_field_by_value(field);
      electricity_switch_position = field;
      continue;
    }
    result = test_field(field, id_electricity_failures);
    if (result)
    {
      replace_field_by_value(field);
      electricity_failures = field;
      continue;
    }
    result = test_field(field, id_electricity_sags_l1);
    if (result)
    {
      replace_field_by_value(field);
      electricity_sags_l1 = field;
      continue;
    }
    result = test_field(field, id_electricity_sags_l2);
    if (result)
    {
      replace_field_by_value(field);
      electricity_sags_l2 = field;
      continue;
    }
    result = test_field(field, id_electricity_sags_l3);
    if (result)
    {
      replace_field_by_value(field);
      electricity_sags_l3 = field;
      continue;
    }
    result = test_field(field, id_electricity_swells_l1);
    if (result)
    {
      replace_field_by_value(field);
      electricity_swells_l1 = field;
      continue;
    }
    result = test_field(field, id_electricity_swells_l2);
    if (result)
    {
      replace_field_by_value(field);
      electricity_swells_l2 = field;
      continue;
    }
    result = test_field(field, id_electricity_swells_l3);
    if (result)
    {
      replace_field_by_value(field);
      electricity_swells_l3 = field;
      continue;
    }
    result = test_field(field, id_message_short);
    if (result)
    {
      replace_field_by_value(field);
      message_short = field;
      continue;
    }
    result = test_field(field, id_message2_long);
    if (result)
    {
      replace_field_by_value(field);
      message2_long = field;
      continue;
    }
    result = test_field(field, id_message3_long);
    if (result)
    {
      replace_field_by_value(field);
      message3_long = field;
      continue;
    }
    result = test_field(field, id_message4_long);
    if (result)
    {
      replace_field_by_value(field);
      message4_long = field;
      continue;
    }
    result = test_field(field, id_message5_long);
    if (result)
    {
      replace_field_by_value(field);
      message5_long = field;
      continue;
    }
    result = test_field(field, id_current_l1);
    if (result)
    {
      replace_field_by_value(field);
      current_l1 = field;
      continue;
    }
    result = test_field(field, id_current_l2);
    if (result)
    {
      replace_field_by_value(field);
      current_l2 = field;
      continue;
    }
    result = test_field(field, id_current_l3);
    if (result)
    {
      replace_field_by_value(field);
      current_l3 = field;
      continue;
    }
  }
  DEBUG_PRINTLN("Exiting parse_dsmr_string");
}

void convert_equipment_id(char *mystring)
{ //coded in HEX
  //DEBUG_PRINTLN("Entering convert_equipment_id");
  int len = strlen(mystring);
  for (int i = 0; i < len / 2; i++)
    mystring[i] = char(((int(mystring[i * 2])) - 48) * 16 + (int(mystring[i * 2 + 1]) - 48));
  mystring[(len / 2)] = 0;
}

/*
  Replace mystring with the value in parenteses.
  e.g. 1-0:3.7.0(00.000) becomes 00.000
*/
void replace_field_by_value(char *mystring)
{
  //DEBUG_PRINTLN("Entering replace_field_by_value");
  int a = strcspn(mystring, "(");
  int b = strcspn(mystring, ")");
  int j = 0;
  for (int i = a + 1; i < b; i++)
  {
    mystring[j] = mystring[i];
    j++;
  }
  mystring[j] = 0;
}

/*
  Return true if the first cmp_len characters of the two fields are the same, false otherwise.
*/
bool test_field(char *field2, const char *dmsr_field_id)
{
  const int cmp_len = 10;
  if (strlen(field2) < cmp_len)
  {
    DEBUG_PRINT("Trying to compare a field that is too short: ");
    DEBUG_PRINTLN(field2);
    return false;
  }
  for (int i = 0; i < cmp_len; i++)
  {
    if (field2[i] != dmsr_field_id[i])
      return false;
  }
  return true;
}

void debug_print_raw_data(int Tlength)
{
  DEBUG_PRINT("Raw length in Byte: ");
  DEBUG_PRINTLN(Tlength);
  DEBUG_PRINTLN("Raw data: ");
  int mul = (Tlength / sll);
  for (int i = 0; i < mul; i++)
  {
    for (int j = 0; j < sll; j++)
    {
      debug_print_hex(telegram[i * sll + j]);
    }
    DEBUG_PRINTLN();
  }
  for (int j = 0; j < (Tlength % sll); j++)
  {
    debug_print_hex(telegram[mul * sll + j]);
  }
  DEBUG_PRINTLN();
}

void debug_print_vector(TestVector *vect)
{
  DEBUG_PRINTLN("\nEntering debug_print_vector");
  DEBUG_PRINT("Vector_Name: ");
  DEBUG_PRINTLN(vect->name);
  DEBUG_PRINT("Key: ");
  for (int cnt = 0; cnt < 16; cnt++)
    debug_print_hex(vect->key[cnt]);
  DEBUG_PRINT("\nData (Text): ");
  int mul = (vect->datasize / sll);
  for (int i = 0; i < mul; i++)
  {
    for (int j = 0; j < sll; j++)
    {
      debug_print_hex(vect->ciphertext[i * sll + j]);
    }
    DEBUG_PRINTLN();
  }
  for (int j = 0; j < (Tlength % sll); j++)
  {
    debug_print_hex(vect->ciphertext[mul * sll + j]);
  }
  DEBUG_PRINT("\nAuth_Data: ");
  for (int cnt = 0; cnt < 17; cnt++)
    debug_print_hex(vect->authdata[cnt]);
  DEBUG_PRINT("\nInit_Vect: ");
  for (int cnt = 0; cnt < 12; cnt++)
    debug_print_hex(vect->iv[cnt]);
  DEBUG_PRINT("\nAuth_Tag: ");
  for (int cnt = 0; cnt < 12; cnt++)
    debug_print_hex(vect->tag[cnt]);
  DEBUG_PRINT("\nAuth_Data Size: ");
  DEBUG_PRINTLN(vect->authsize);
  DEBUG_PRINT("Data Size: ");
  DEBUG_PRINTLN(vect->datasize);
  DEBUG_PRINT("Auth_Tag Size: ");
  DEBUG_PRINTLN(vect->tagsize);
  DEBUG_PRINT("Init_Vect Size: ");
  DEBUG_PRINTLN(vect->ivsize);
  DEBUG_PRINTLN();
}

void debug_print_dsmr()
{
  DEBUG_PRINTLN("\nEntering debug_print_dsmr");
  DEBUG_PRINT("identification: ");
  DEBUG_PRINTLN(identification);
  DEBUG_PRINT("p1_version: ");
  DEBUG_PRINTLN(p1_version);
  DEBUG_PRINT("timestamp: ");
  DEBUG_PRINTLN(timestamp);
  DEBUG_PRINT("equipment_id: ");
  DEBUG_PRINTLN(equipment_id);
  DEBUG_PRINT("energy_delivered_tariff1: ");
  DEBUG_PRINTLN(energy_delivered_tariff1);
  DEBUG_PRINT("energy_returned_tariff1: ");
  DEBUG_PRINTLN(energy_returned_tariff1);
  DEBUG_PRINT("reactive_energy_delivered_tariff1: ");
  DEBUG_PRINTLN(reactive_energy_delivered_tariff1);
  DEBUG_PRINT("reactive_energy_returned_tariff1: ");
  DEBUG_PRINTLN(reactive_energy_returned_tariff1);
  DEBUG_PRINT("power_delivered: ");
  DEBUG_PRINTLN(power_delivered);
  DEBUG_PRINT("power_returned: ");
  DEBUG_PRINTLN(power_returned);
  DEBUG_PRINT("reactive_power_delivered: ");
  DEBUG_PRINTLN(reactive_power_delivered);
  DEBUG_PRINT("reactive_power_returned: ");
  DEBUG_PRINTLN(reactive_power_returned);
  DEBUG_PRINT("electricity_threshold: ");
  DEBUG_PRINTLN(electricity_threshold);
  DEBUG_PRINT("electricity_switch_position: ");
  DEBUG_PRINTLN(electricity_switch_position);
  DEBUG_PRINT("electricity_failures: ");
  DEBUG_PRINTLN(electricity_failures);
  DEBUG_PRINT("electricity_sags_l1: ");
  DEBUG_PRINTLN(electricity_sags_l1);
  DEBUG_PRINT("electricity_sags_l2: ");
  DEBUG_PRINTLN(electricity_sags_l2);
  DEBUG_PRINT("electricity_sags_l3: ");
  DEBUG_PRINTLN(electricity_sags_l3);
  DEBUG_PRINT("electricity_swells_l1: ");
  DEBUG_PRINTLN(electricity_swells_l1);
  DEBUG_PRINT("electricity_swells_l2: ");
  DEBUG_PRINTLN(electricity_swells_l2);
  DEBUG_PRINT("electricity_swells_l3: ");
  DEBUG_PRINTLN(electricity_swells_l3);
  DEBUG_PRINT("message_short: ");
  DEBUG_PRINTLN(message_short);
  DEBUG_PRINT("message2_long: ");
  DEBUG_PRINTLN(message2_long);
  DEBUG_PRINT("message3_long: ");
  DEBUG_PRINTLN(message3_long);
  DEBUG_PRINT("message4_long: ");
  DEBUG_PRINTLN(message4_long);
  DEBUG_PRINT("message5_long: ");
  DEBUG_PRINTLN(message5_long);
  DEBUG_PRINT("current_l1: ");
  DEBUG_PRINTLN(current_l1);
  DEBUG_PRINT("current_l2: ");
  DEBUG_PRINTLN(current_l2);
  DEBUG_PRINT("current_l3: ");
  DEBUG_PRINTLN(current_l3);
}

/*
   Need a local yield that that calls yield() and mqttClient.loop()
   The system yield() routine does not call mqttClient.loop()
*/
void local_yield()
{
  yield();
#ifdef USE_MQTT
  mqttClient.loop();
#endif
}

/*
   Need a local delay calls yield() and mqttClient.loop()
   The system delay() routine does not call mqttClient.loop()
*/
void local_delay(unsigned long millisecs)
{
  unsigned long start = millis();
  local_yield();
  if (millisecs > 0)
  {
    while (elapsed_time(start) < millisecs)
    {
      local_yield();
    }
  }
}

/*
 *  Returns the number of milliseconds elapsed since  start_time_ms.
 */
unsigned long elapsed_time(unsigned long start_time_ms)
{
  return millis() - start_time_ms;
}

void debug_print_hex(char x)
{
  if (x < 0x10) // add leading 0 if needed
  {
    DEBUG_PRINT("0");
  };
  DEBUG_PRINT(x, HEX);
}