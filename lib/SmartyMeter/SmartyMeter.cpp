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

#define MAX_ORBIS_SIZE 15

struct dsmr_field_t dsmr[] = {
  {"pwr_dlvrd", "1-0:1.7.0", "kW", ""},
  {"pwr_rtrnd", "1-0:2.7.0", "kW", ""},
  {"react_engy_dlvrd_tariff1", "1-0:3.8.0", "kVArh", ""},
  {"react_engy_rtrnd_tariff1", "1-0:4.8.0", "kVArh", ""},

  {"act_pwr_p_minus_l1", "1-0:22.7.0", "kW", ""},
  {"act_pwr_p_minus_l2", "1-0:42.7.0", "kW", ""},
  {"act_pwr_p_minus_l3", "1-0:62.7.0", "kW", ""},
  {"act_pwr_p_plus_l1", "1-0:21.7.0", "kW", ""},
  {"act_pwr_p_plus_l2", "1-0:41.7.0", "kW", ""},
  {"act_pwr_p_plus_l3", "1-0:61.7.0", "kW", ""},
  {"appt_export_pwr", "1-0:10.7.0", "kVA", ""},
  {"appt_import_pwr", "1-0:9.7.0", "kVA", ""},
  {"brkr_ctrl_state_1", "0-1:96.3.10", "", ""},
  {"brkr_ctrl_state_2", "0-2:96.3.10", "", ""},
  {"elec_failures", "0-0:96.7.21", "", ""},
  {"elec_sags_l1", "1-0:32.32.0", "", ""},
  {"elec_sags_l2", "1-0:52.32.0", "", ""},
  {"elec_sags_l3", "1-0:72.32.0", "", ""},
  {"elec_swells_l1", "1-0:32.36.0", "", ""},
  {"elec_swells_l2", "1-0:52.36.0", "", ""},
  {"elec_swells_l3", "1-0:72.36.0", "", ""},
  {"elec_switch_postn", "0-0:96.3.10", "", ""},
  {"elec_threshold", "0-0:17.0.0", "kVA", ""},
  {"engy_dlvrd_tariff1", "1-0:1.8.0", "kWh", ""},
  {"engy_rtrnd_tariff1", "1-0:2.8.0", "kWh", ""},
    {"equipment_id", "0-0:42.0.0", "", ""},
  {"gas_index", "0-1:24.2.1", "m3", ""},
  {"limiter_curr_monitor", "1-1:31.4.0", "A", ""},
  {"msg_short", "0-0:96.13.0", "", ""},
  {"msg2_long", "0-0:96.13.2", "", ""},
  {"msg3_long", "0-0:96.13.3", "", ""},
  {"msg4_long", "0-0:96.13.4", "", ""},
  {"msg5_long", "0-0:96.13.5", "", ""},
  {"p1_version", "1-3:0.2.8", "", ""},
  {"phase_curr_l1", "1-0:31.7.0", "A", ""},
  {"phase_curr_l2", "1-0:51.7.0", "A", ""},
  {"phase_curr_l3", "1-0:71.7.0", "A", ""},
  {"phase_volt_l1", "1-0:32.7.0", "V", ""},
  {"phase_volt_l2", "1-0:52.7.0", "V", ""},
  {"phase_volt_l3", "1-0:72.7.0", "V", ""},

  {"react_pwr_dlvrd", "1-0:3.7.0", "kVAr", ""},
  {"react_pwr_q_minus_l1", "1-0:24.7.0", "kVAr", ""},
  {"react_pwr_q_minus_l2", "1-0:44.7.0", "kVAr", ""},
  {"react_pwr_q_minus_l3", "1-0:64.7.0", "kVAr", ""},
  {"react_pwr_q_plus_l1", "1-0:23.7.0", "kVAr", ""},
  {"react_pwr_q_plus_l2", "1-0:43.7.0", "kVAr", ""},
  {"react_pwr_q_plus_l3", "1-0:63.7.0", "kVAr", ""},
  {"react_pwr_rtrnd", "1-0:4.7.0", "kVAr", ""},
  {"timestamp", "0-0:1.0.0", "", ""}
};

uint8_t telegram[MAX_TELEGRAM_LENGTH];
char buffer[MAX_TELEGRAM_LENGTH];
Vector Vector_SM;
int empty_reads = 0;


SmartyMeter::SmartyMeter(uint8_t decrypt_key[], byte data_request_pin) : _decrypt_key(decrypt_key),
                                                                         _data_request_pin(data_request_pin),
                                                                         _fake_vector_size(0)
{
  num_dsmr_fields = sizeof(dsmr) / sizeof(dsmr_field_t);
}

void SmartyMeter::setFakeVector(char *fake_vector, int fake_vector_size)
{
  DEBUG_PRINTLN("Using fake vector instead of data from serial port.");
  _fake_vector = fake_vector;
  _fake_vector_size = fake_vector_size;
}

void SmartyMeter::begin()
{
  //DEBUG_PRINTLN("SmartyMeter::begin");
  pinMode(_data_request_pin, OUTPUT);
  Serial.begin(115200); // Hardware serial connected to smarty
  Serial.setRxBufferSize(MAX_TELEGRAM_LENGTH); 
}


/*  
    Attempts to read data from the meter and decode it. 
    Returns true if successful.
*/
bool SmartyMeter::readAndDecodeData()
{
  int telegram_size = readTelegram(telegram);
  DEBUG_PRINTF("SmartyMeter::readAndDecodeData - %d bytes read\n", telegram_size);
  if (telegram_size == 0)
  {
    empty_reads++;
    if (empty_reads > 10) {
      DEBUG_PRINTLN("No data received for too long, resetting device.");
      while(1){;}
    }
    return false;
  }
  empty_reads = 0;
  print_telegram(telegram, telegram_size);
  if (! init_vector(telegram, &Vector_SM, "Vector_SM", _decrypt_key))
  {
    DEBUG_PRINTLN("ERROR in init_vector, aborting.");
    return false;
  }
  //print_vector(&Vector_SM);
  decrypt_vector_to_buffer(&Vector_SM, buffer);
  parseDsmrString(buffer); 
  return true; 
}

/*
      Read data from the counter on the serial line
      Saves data to telegram and returns its size.
*/
int SmartyMeter::readTelegram(uint8_t telegram[])
{
  int cnt = 0;
  DEBUG_PRINTLN("Entering readTelegram");
  int max_telegram_size = sizeof(telegram);
  memset(telegram, 0, max_telegram_size); // initialise telegram buffer

  if (_fake_vector_size > 0)
  {
    DEBUG_PRINTLN("readTelegram using fake vector");
    memcpy(telegram, _fake_vector, _fake_vector_size);
    return _fake_vector_size;
  }

  digitalWrite(_data_request_pin, LOW); // Request serial data On
  while (Serial.available() && (cnt != max_telegram_size))
  {
    telegram[cnt] = Serial.read();
    if ((cnt==0) && (telegram[0] != 0xDB)) {
      DEBUG_PRINTLN("The first byte should be 0xDB. Resetting device to re-sync.");
      while(1){;}
    }
    cnt++;
  }
  digitalWrite(_data_request_pin, HIGH); // Request serial data Off
  return (cnt);
}

void SmartyMeter::clearDsmr()
{
  DEBUG_PRINTLN("About to clear dsmr fields.");
  for (int i = 0; i < num_dsmr_fields; i++)
  {
    dsmr[i].value[0] = 0;
  }
  DEBUG_PRINTLN("dsmr fields cleared.");
}

/*
  Parse fields in mystgring (buffer) and store in global variables
*/
void SmartyMeter::parseDsmrString(char *mystring)
{
  clearDsmr();

  DEBUG_PRINTF("parseDsmrString: string to parse:\n%s\n", mystring);

  char *line;
  char orbis[MAX_ORBIS_SIZE + 1];
  bool found;

  strtok(mystring, "\n"); // get the first line
  while (true)
  {
    // process next lines
    line = strtok(NULL, "\n");
    if ((line == NULL) || (line[0] == '!'))
    {
      break;
    }
    //DEBUG_PRINTF("Processing line: %s\n", line);

    // Extract orbis code from line
    int first_open_bracket_pos = strcspn(line, "(");
    //DEBUG_PRINTF("Bracket pos: %d\n", first_open_bracket_pos);
    if (first_open_bracket_pos >= (int)strlen(line))
    {
      //DEBUG_PRINTLN("No bracket in this line, skipping.");
      continue;
    }
    strncpy(orbis, line, first_open_bracket_pos);
    orbis[first_open_bracket_pos] = 0;

    //DEBUG_PRINTF("Will try to match %d orbis fields.\n", num_dsmr_fields);
    found = false;
    for (int i = 0; i < num_dsmr_fields; i++)
    {
      //DEBUG_PRINTF("Comparing extracted orbis %s with orbis %s\n", orbis, dsmr[i].id);
      if (strcmp(orbis, dsmr[i].id) == 0)
      {
        found = true;
        if (strcmp(dsmr[i].name, "gas_index") == 0)
        {
          // example 0-1:24.2.1(101209112500W)(12785.123*m3)
          replace_by_val_in_last_braces(line);
        }
        else
        {
          // example 1-0:71.7.0(000*A)
          replace_by_val_in_first_braces(line);
        }
        if (strcmp(dsmr[i].name, "equipment_id") == 0)
        {
          convert_equipment_id(line);
        }
        remove_unit_if_present(line);
        //DEBUG_PRINTF("Setting dsmr %s with value %s\n", dsmr[i].name, line);
        strncpy(dsmr[i].value, line, MAX_VALUE_LENGTH);
        break;
      }
    }
    if (!found) {
      DEBUG_PRINTF("Could not match orbis %s\n", orbis); 
    }
  }
  DEBUG_PRINTLN("Exiting parseDsmrString");
}

void SmartyMeter::printDsmr()
{
  DEBUG_PRINTLN("\nSmartyMeter::printDsmr:");
  for (int i = 0; i < num_dsmr_fields; i++)
  {
    //delay(10);
    DEBUG_PRINTF("%12s | %33s | %s (%s)\n",
                 dsmr[i].id,
                 dsmr[i].name,
                 dsmr[i].value,
                 dsmr[i].unit);
  }
  DEBUG_PRINTLN();
}
