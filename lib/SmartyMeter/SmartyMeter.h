/*
  SmartyMeter.h - Library to get reading from Luxembourgish Smart electricty (and gas) meters.
  
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

#ifndef SmartyMeter_h
#define SmartyMeter_h

#include "Arduino.h"

#define MAX_VALUE_LENGTH 33

// Dutch smart meter requirements
struct dsmr_field_t
{
  const char *name;
  const char *id;
  const char *unit;
  char value[MAX_VALUE_LENGTH];
};

extern struct dsmr_field_t dsmr[];

class SmartyMeter
{
public:
  SmartyMeter(uint8_t decrypt_key[], byte data_request_pin);
  void setFakeVector(char *fake_vector, int fake_vector_size);
  void begin();
  bool readAndDecodeData();
  void printDsmr();
  int num_dsmr_fields;

private:
  uint8_t *_decrypt_key;
  byte _data_request_pin;
  char *_fake_vector;
  int _fake_vector_size;
  int readTelegram(uint8_t telegram[], int max_telegram_length);
  void parseDsmrString(char *mystring);
  void clearDsmr();
};

#endif // SmartyMeter_h