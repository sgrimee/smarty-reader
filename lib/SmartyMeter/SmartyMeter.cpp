/*
  SmartyMeter.cpp - Library to get reading from Luxembourgish Smart electricty (and gas) meters.
  
  Original code Copyright (C) 2018 Guy WEILER www.weigu.lu

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

#define SMARTY_DEBUG
#include "debug_helpers.h"

#include "SmartyMeter.h"
#include "smarty_helpers.h"

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
const char *id_current_l3 = "1-0:71.7.0(";
const char *id_gas_index = "0-1:24.2.1("; // 0-1:24.2.1(101209112500W)(12785.123*m3)

SmartyMeter::SmartyMeter(uint8_t decrypt_key[], byte data_request_pin)
{
  _decrypt_key = decrypt_key;
  _data_request_pin = data_request_pin;
  _fake_vector_size = 0;
}

void SmartyMeter::setFakeVector(char *fake_vector, int fake_vector_size)
{
  _fake_vector = fake_vector;
  _fake_vector_size = fake_vector_size;
}

void SmartyMeter::begin()
{
  DEBUG_PRINTLN("SmartyMeter::begin");
  pinMode(_data_request_pin, OUTPUT);
  Serial.begin(115200); // Hardware serial connected to smarty
  Serial.setRxBufferSize(1024);
}

/*  readAndDecodeData

    Attempts to read data from the meter and decode it. 
    Returns true if successful.
*/
bool SmartyMeter::readAndDecodeData()
{
  uint8_t telegram[MAX_TELEGRAM_LENGTH];
  char buffer[MAX_TELEGRAM_LENGTH - 30];
  Vector Vector_SM;

  DEBUG_PRINTLN("SmartyMeter::readAndDecodeData - about to read telegram");
  int telegram_size = readTelegram(telegram, sizeof(telegram));
  if (telegram_size == 0)
  {
    DEBUG_PRINTLN("SmartyMeter::readAndDecodeData - No data received");
    return false;
  }
  print_telegram(telegram, telegram_size);

  init_vector(telegram, &Vector_SM, "Vector_SM", _decrypt_key);
  if (Vector_SM.datasize == MAX_TELEGRAM_LENGTH)
  {
    DEBUG_PRINTLN("SmartyMeter::readAndDecodeData - MAX_TELEGRAM_LENGTH, aborting");
    return false;
  }
  print_vector(&Vector_SM);
  decrypt_vector_to_buffer(&Vector_SM, buffer, sizeof(buffer));
  parseDsmrString(buffer);
  printDsmr();
  return true;
}

/*
      Read data from the counter on the serial line
      Saves data to telegram and returns its size.
*/
int SmartyMeter::readTelegram(uint8_t telegram[], int max_telegram_length)
{
  int cnt = 0;
  DEBUG_PRINTLN("Entering readTelegram");
  memset(telegram, '1', max_telegram_length); // initialise telegram buffer

  if (_fake_vector_size > 0)
  {
    memcpy(telegram, _fake_vector, _fake_vector_size);
    return _fake_vector_size;
  }

  digitalWrite(_data_request_pin, LOW); // Request serial data On
  delay(10);
  while ((Serial.available()) && (cnt != max_telegram_length))
  {
    telegram[cnt] = Serial.read();
    cnt++;
  }
  digitalWrite(_data_request_pin, HIGH); // Request serial data Off
  return (cnt);
}

void SmartyMeter::clearDsmr()
{
  identification = "";
  p1_version = "";
  timestamp = "";
  equipment_id = "";
  energy_delivered_tariff1 = "";
  energy_returned_tariff1 = "";
  reactive_energy_delivered_tariff1 = "";
  reactive_energy_returned_tariff1 = "";
  power_delivered = "";
  power_returned = "";
  reactive_power_delivered = "";
  reactive_power_returned = "";
  electricity_threshold = "";
  electricity_switch_position = "";
  electricity_failures = "";
  electricity_sags_l1 = "";
  electricity_sags_l2 = "";
  electricity_sags_l3 = "";
  electricity_swells_l1 = "";
  electricity_swells_l2 = "";
  electricity_swells_l3 = "";
  message_short = "";
  message2_long = "";
  message3_long = "";
  message4_long = "";
  message5_long = "";
  current_l1 = "";
  current_l2 = "";
  current_l3 = "";
  gas_index = "()";
}

/*
  Parse fields in mystgring (buffer) and store in global variables
*/
void SmartyMeter::parseDsmrString(char *mystring)
{
  clearDsmr();

  DEBUG_PRINTLN("parseDsmrString: string to parse: ");
  DEBUG_PRINTLN(mystring);

  bool result;
  char *field = (char *)"bla";

  identification = strtok(mystring, "\n"); // get the first field

  while ((field[0] != '!') && (field != NULL))
  { // walk through other fields
    field = strtok(NULL, "\n");
    DEBUG_PRINT("Testing field: ");
    DEBUG_PRINTLN(field);
    if (strlen(field) < 10)
    {
      continue;
    }
    if (test_field(field, id_p1_version))
    {
      replace_by_val_in_first_braces(field);
      p1_version = field;
      continue;
    }
    result = test_field(field, id_timestamp);
    if (result)
    {
      replace_by_val_in_first_braces(field);
      timestamp = field;
      continue;
    }
    result = test_field(field, id_equipment_id);
    if (result)
    {
      replace_by_val_in_first_braces(field);
      convert_equipment_id(field);
      equipment_id = field;
      continue;
    }
    result = test_field(field, id_energy_delivered_tariff1);
    if (result)
    {
      replace_by_val_in_first_braces(field);
      energy_delivered_tariff1 = field;
      continue;
    }
    result = test_field(field, id_energy_returned_tariff1);
    if (result)
    {
      replace_by_val_in_first_braces(field);
      energy_returned_tariff1 = field;
      continue;
    }
    result = test_field(field, id_reactive_energy_delivered_tariff1);
    if (result)
    {
      replace_by_val_in_first_braces(field);
      reactive_energy_delivered_tariff1 = field;
      continue;
    }
    result = test_field(field, id_reactive_energy_returned_tariff1);
    if (result)
    {
      replace_by_val_in_first_braces(field);
      reactive_energy_returned_tariff1 = field;
      continue;
    }
    result = test_field(field, id_power_delivered);
    if (result)
    {
      replace_by_val_in_first_braces(field);
      power_delivered = field;
      continue;
    }
    result = test_field(field, id_power_returned);
    if (result)
    {
      replace_by_val_in_first_braces(field);
      power_returned = field;
      continue;
    }
    result = test_field(field, id_reactive_power_delivered);
    if (result)
    {
      replace_by_val_in_first_braces(field);
      reactive_power_delivered = field;
      continue;
    }
    result = test_field(field, id_reactive_power_returned);
    if (result)
    {
      replace_by_val_in_first_braces(field);
      reactive_power_returned = field;
      continue;
    }
    result = test_field(field, id_electricity_threshold);
    if (result)
    {
      replace_by_val_in_first_braces(field);
      electricity_threshold = field;
      continue;
    }
    result = test_field(field, id_electricity_switch_position);
    if (result)
    {
      replace_by_val_in_first_braces(field);
      electricity_switch_position = field;
      continue;
    }
    result = test_field(field, id_electricity_failures);
    if (result)
    {
      replace_by_val_in_first_braces(field);
      electricity_failures = field;
      continue;
    }
    result = test_field(field, id_electricity_sags_l1);
    if (result)
    {
      replace_by_val_in_first_braces(field);
      electricity_sags_l1 = field;
      continue;
    }
    result = test_field(field, id_electricity_sags_l2);
    if (result)
    {
      replace_by_val_in_first_braces(field);
      electricity_sags_l2 = field;
      continue;
    }
    result = test_field(field, id_electricity_sags_l3);
    if (result)
    {
      replace_by_val_in_first_braces(field);
      electricity_sags_l3 = field;
      continue;
    }
    result = test_field(field, id_electricity_swells_l1);
    if (result)
    {
      replace_by_val_in_first_braces(field);
      electricity_swells_l1 = field;
      continue;
    }
    result = test_field(field, id_electricity_swells_l2);
    if (result)
    {
      replace_by_val_in_first_braces(field);
      electricity_swells_l2 = field;
      continue;
    }
    result = test_field(field, id_electricity_swells_l3);
    if (result)
    {
      replace_by_val_in_first_braces(field);
      electricity_swells_l3 = field;
      continue;
    }
    result = test_field(field, id_message_short);
    if (result)
    {
      replace_by_val_in_first_braces(field);
      message_short = field;
      continue;
    }
    result = test_field(field, id_message2_long);
    if (result)
    {
      replace_by_val_in_first_braces(field);
      message2_long = field;
      continue;
    }
    result = test_field(field, id_message3_long);
    if (result)
    {
      replace_by_val_in_first_braces(field);
      message3_long = field;
      continue;
    }
    result = test_field(field, id_message4_long);
    if (result)
    {
      replace_by_val_in_first_braces(field);
      message4_long = field;
      continue;
    }
    result = test_field(field, id_message5_long);
    if (result)
    {
      replace_by_val_in_first_braces(field);
      message5_long = field;
      continue;
    }
    result = test_field(field, id_current_l1);
    if (result)
    {
      replace_by_val_in_first_braces(field);
      current_l1 = field;
      continue;
    }
    result = test_field(field, id_current_l2);
    if (result)
    {
      replace_by_val_in_first_braces(field);
      current_l2 = field;
      continue;
    }
    result = test_field(field, id_current_l3);
    if (result)
    {
      replace_by_val_in_first_braces(field);
      current_l3 = field;
      continue;
    }
    // result = test_field(field, id_gas_index);
    // if (result)
    // {
    //   replace_by_val_in_last_braces(field);
    //   gas_index = field;
    //   continue;
    // }
  }
  DEBUG_PRINTLN("Exiting parseDsmrString");
}

void SmartyMeter::printDsmr()
{
  DEBUG_PRINTLN("\nEntering printDsmr");
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
  // DEBUG_PRINT("gas index: ");
  // DEBUG_PRINTLN(gas_index);
}
