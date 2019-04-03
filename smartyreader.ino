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
  way to debug is to use Serial1.println() and connect RX of an USB2Serial
  adapter (FTDI, Profilic, CP210, ch340/341) to D4 and use a terminal program
  like CuteCom or CleverTerm to listen to D4.
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include <Crypto.h>
#include <AES.h>
#include <GCM.h>
#include <string.h>

#define MAX_LINE_LENGTH 1024
// uncomment if debugging requested
#define DEBUG
// uncomment if DHCP needed
#define STATIC

IPAddress wemos_ip (192,168,178,100); //static IP
IPAddress dns_ip     (192,168,178,1);
IPAddress gateway_ip (192,168,178,1);
IPAddress subnet_mask(255,255,255,0);
WiFiClient espClient;
PubSubClient client(espClient);

const char ssid[] = "mywifi";
const char password[] = "mypass";
const char *mqtt_server = "192.168.178.101";
const int  mqttPort = 1883;
const char *clientId = "smarty_lam1_p1";
const char *topic = "lamsmarty";
const char *smartyreader_hostname = "Smartyreader";

const byte DATA_REQUEST_SM = D3; //active Low!
const int sll = 50; // length of a line if printing serial raw data

struct TestVector {
    const char *name;
    uint8_t key[16];
    uint8_t ciphertext[MAX_LINE_LENGTH];
    uint8_t authdata[17];
    uint8_t iv[12];
    uint8_t tag[16];
    uint8_t authsize;
    uint16_t datasize;
    uint8_t tagsize;
    uint8_t ivsize; };

TestVector Vector_SM;
GCM<AES128> *gcmaes128 = 0;

//Key for SAG1030700089067
uint8_t key_SM_LAM_1[] = {0xAE, 0xBD, 0x21, 0xB7, 0x69, 0xA6, 0xD1, 0x3C,
                          0x0D, 0xF0, 0x64, 0xE3, 0x83, 0x68, 0x2E, 0xFF};

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

char *id_p1_version = "1-3:0.2.8(";
char *id_timestamp = "0-0:1.0.0(";
char *id_equipment_id = "0-0:42.0.0(";
char *id_energy_delivered_tariff1 = "1-0:1.8.0(";
char *id_energy_returned_tariff1 = "1-0:2.8.0(";
char *id_reactive_energy_delivered_tariff1 = "1-0:3.8.0(";
char *id_reactive_energy_returned_tariff1 = "1-0:4.8.0(";
char *id_power_delivered = "1-0:1.7.0(";
char *id_power_returned = "1-0:2.7.0(";
char *id_reactive_power_delivered = "1-0:3.7.0(";
char *id_reactive_power_returned = "1-0:4.7.0(";
char *id_electricity_threshold = "0-0:17.0.0(";
char *id_electricity_switch_position = "0-0:96.3.10(";
char *id_electricity_failures = "0-0:96.7.21(";
char *id_electricity_sags_l1 = "1-0:32.32.0(";
char *id_electricity_sags_l2 = "1-0:52.32.0(";
char *id_electricity_sags_l3 = "1-0:72.32.0(";
char *id_electricity_swells_l1 = "1-0:32.36.0(";
char *id_electricity_swells_l2 = "1-0:52.36.0(";
char *id_electricity_swells_l3 = "1-0:72.36.0(";
char *id_message_short = "0-0:96.13.0(";
char *id_message2_long = "0-0:96.13.2(";
char *id_message3_long = "0-0:96.13.3(";
char *id_message4_long = "0-0:96.13.4(";
char *id_message5_long = "0-0:96.13.5(";
char *id_current_l1 = "1-0:31.7.0(";
char *id_current_l2 = "1-0:51.7.0(";
char *id_current_l3 = "1-0:71.7.0(";

uint8_t telegram[MAX_LINE_LENGTH];
char buffer[MAX_LINE_LENGTH-30];
char msg[128], filename[13];
int test,Tlength;

void setup() {
  delay(1000);
  pinMode(LED_BUILTIN, OUTPUT);    // Initialize outputs
  digitalWrite(LED_BUILTIN,LOW);   // On
  setup_wifi();
  client.setServer(mqtt_server,mqttPort);
  digitalWrite(LED_BUILTIN,HIGH);   // Off
  #ifdef DEBUG
  Serial1.begin(115200); // transmit-only UART for debugging on D4 (LED!)
  Serial1.println("Serial is working!");
  #endif // #ifdef DEBUG
  pinMode(DATA_REQUEST_SM, OUTPUT);
  delay(100);
  Serial.begin(115200); // Hardware serial connected to smarty
  Serial.setRxBufferSize(1024);
  #ifndef DEBUG
  for(int i=0; i<3; i++){
        digitalWrite(LED_BUILTIN,HIGH);   // Off
        delay(200);
        digitalWrite(LED_BUILTIN,LOW);   // On
        delay(200); }
  #endif // ifndef DEBUG
  delay(2000);
  }

void loop() {
  #ifdef DEBUG
  Serial1.println("------------------");
  Serial1.println("------------------");
  Serial1.println("Loop begins");
  Serial1.println("------------------");
  #endif // ifdef DEBUG
  //digitalWrite(LED_BUILTIN,LOW);   // On
  if (!client.connected()) { reconnect(); }
  client.loop();
  memset(telegram, '1', sizeof(telegram));
  digitalWrite(DATA_REQUEST_SM,LOW);   // Request serial data On
  delay(10);
  Tlength = readTelegram();
  digitalWrite(DATA_REQUEST_SM,HIGH);   // Request serial data Off
  #ifdef DEBUG
  print_raw_data(Tlength);
  Serial1.println("------------------");
  #endif // ifdef DEBUG
  init_vector(&Vector_SM,"Vector_SM",key_SM_LAM_1);
  if (Vector_SM.datasize != 1024) {
    decrypt_text(&Vector_SM);
    parse_dsmr_string(buffer);
    snprintf (msg, 128, "{\"dt\":\"%s\",\"id\":\"%c%c%c\",\"c1\":\"%s\",\""
              "p1\":\"%s\"}",
              timestamp,equipment_id[13],equipment_id[14],equipment_id[15],
              energy_delivered_tariff1, energy_returned_tariff1);
    test=client.publish(topic, msg);
    #ifdef DEBUG
    Serial1.println("Print Vector:");
    print_vector(&Vector_SM);
    Serial1.println("------------------");
    Serial1.println("Print DSMR:");
    print_dsmr();
    Serial1.println("------------------");
    Serial1.println("Published message:");
    Serial1.println(msg);
    Serial1.println("------------------");
    #endif // ifdef DEBUG
  }
  #ifndef DEBUG
  for(int i=0; i<2; i++){
        digitalWrite(LED_BUILTIN,HIGH);   // Off
        delay(200);
        digitalWrite(LED_BUILTIN,LOW);   // On
        delay(200); }*/
  delay(150);
  digitalWrite(LED_BUILTIN,HIGH);   // Off
  #endif // ifndef DEBUG
  delay(59000);
}

int readTelegram() {
  int cnt = 0;
  while ((Serial.available()) && (cnt!= MAX_LINE_LENGTH)) {
    telegram[cnt] = Serial.read();
    cnt++; }
  return (cnt);
}

void setup_wifi() {
  WiFi.softAPdisconnect(); // to eliminate Hotspot
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  delay(200);
  #ifdef STATIC    
  WiFi.config(wemos_ip, gateway_ip, subnet_mask);
  #endif // ifdef STATIC
  WiFi.hostname(smartyreader_hostname);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
  randomSeed(micros());
}

void reconnect() {
  while (!client.connected()) { // Loop until we're reconnected
    if (client.connect(clientId)) { // Attempt to connect
      client.publish(topic, "{\"dt\":\"connected\"}"); // Once connected, publish an announcement...
    }
    else { delay(5000); } // Wait 5 seconds before retrying
  }
}

// Convert binary coded decimal to decimal number
byte bcdToDec(byte val) { return((val/16*10) + (val%16)); }

// Convert decimal number to binary coded decimal
byte decToBcd(byte val) { return((val/10*16) + (val%10)); }

void init_vector(TestVector *vect, char *Vect_name, uint8_t *key_SM) {
  vect->name = Vect_name;  // vector name
  for (int i = 0; i < 16; i++)
     vect->key[i] = key_SM[i];
  uint16_t Data_Length = uint16_t(telegram[11])*256 + uint16_t(telegram[12])-17; // get length of data
  if (Data_Length>MAX_LINE_LENGTH) Data_Length = MAX_LINE_LENGTH;
  for (int i = 0; i < Data_Length; i++)
     vect->ciphertext[i] = telegram[i+18];
  uint8_t AuthData[] = {0x30, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
                        0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE,
                        0xFF};
  for (int i = 0; i < 17; i++)
     vect->authdata[i] = AuthData[i];
  for (int i = 0; i < 8; i++)
     vect->iv[i] = telegram[2+i];
  for (int i = 8; i < 12; i++)
     vect->iv[i] = telegram[6+i];
  for (int i = 0; i < 12; i++)
     vect->tag[i] = telegram[Data_Length+18+i];
  vect->authsize = 17;
  vect->datasize = Data_Length;
  vect->tagsize = 12;
  vect->ivsize  = 12;
}

void decrypt_text(TestVector *vect) {
  gcmaes128 = new GCM<AES128>();
  size_t posn, len;
  size_t inc = vect->datasize;
  memset(buffer, 0xBA, sizeof(buffer));
  gcmaes128->setKey(vect->key, gcmaes128->keySize());
  gcmaes128->setIV(vect->iv, vect->ivsize);
  for (posn = 0; posn < vect->datasize; posn += inc) {
    len = vect->datasize - posn;
    if (len > inc) len = inc;
    gcmaes128->decrypt((uint8_t*)buffer + posn, vect->ciphertext + posn, len);
   }
  delete gcmaes128;
}

void parse_dsmr_string(char *mystring) {
   bool result;
   char *field;
   identification = strtok(mystring, "\n"); // get the first field
   while ((field[0] != '!') && (field != NULL)) { // walk through other fields
     field = strtok(NULL, "\n");
     result=test_field(field,id_p1_version);
     if (result) {
        get_field(field);
        p1_version = field;
      }
     result=test_field(field,id_timestamp);
     if (result) {
        get_field(field);
        timestamp = field;
      }
     result=test_field(field,id_equipment_id);
     if (result) {
        get_field(field);
        convert_equipment_id(field);
        equipment_id = field;
      }
     result=test_field(field,id_energy_delivered_tariff1);
     if (result) {
        get_field(field);
        energy_delivered_tariff1 = field;
      }
     result=test_field(field,id_energy_returned_tariff1);
     if (result) {
        get_field(field);
        energy_returned_tariff1 = field;
      }
     result=test_field(field,id_reactive_energy_delivered_tariff1);
     if (result) {
        get_field(field);
        reactive_energy_delivered_tariff1 = field;
      }
     result=test_field(field,id_reactive_energy_returned_tariff1);
     if (result) {
        get_field(field);
        reactive_energy_returned_tariff1 = field;
      }
     result=test_field(field,id_power_delivered);
     if (result) {
        get_field(field);
        power_delivered = field;
      }
     result=test_field(field,id_power_returned);
     if (result) {
        get_field(field);
        power_returned = field;
      }
     result=test_field(field,id_reactive_power_delivered);
     if (result) {
        get_field(field);
        reactive_power_delivered = field;
      }
     result=test_field(field,id_reactive_power_returned);
     if (result) {
        get_field(field);
        reactive_power_returned = field;
      }
     result=test_field(field,id_electricity_threshold);
     if (result) {
        get_field(field);
        electricity_threshold = field;
      }
     result=test_field(field,id_electricity_switch_position);
     if (result) {
        get_field(field);
        electricity_switch_position = field;
      }
     result=test_field(field,id_electricity_failures);
     if (result) {
        get_field(field);
        electricity_failures = field;
      }
     result=test_field(field,id_electricity_sags_l1);
     if (result) {
        get_field(field);
        electricity_sags_l1 = field;
      }
     result=test_field(field,id_electricity_sags_l2);
     if (result) {
        get_field(field);
        electricity_sags_l2 = field;
      }
     result=test_field(field,id_electricity_sags_l3);
     if (result) {
        get_field(field);
        electricity_sags_l3 = field;
      }
     result=test_field(field,id_electricity_swells_l1);
     if (result) {
        get_field(field);
        electricity_swells_l1 = field;
      }
     result=test_field(field,id_electricity_swells_l2);
     if (result) {
        get_field(field);
        electricity_swells_l2 = field;
      }
     result=test_field(field,id_electricity_swells_l3);
     if (result) {
        get_field(field);
        electricity_swells_l3 = field;
      }
     result=test_field(field,id_message_short);
     if (result) {
        get_field(field);
        message_short = field;
      }
     result=test_field(field,id_message2_long);
     if (result) {
        get_field(field);
        message2_long = field;
      }
     result=test_field(field,id_message3_long);
     if (result) {
        get_field(field);
        message3_long = field;
      }
     result=test_field(field,id_message4_long);
     if (result) {
        get_field(field);
        message4_long = field;
      }
     result=test_field(field,id_message5_long);
     if (result) {
        get_field(field);
        message5_long = field;
      }
     result=test_field(field,id_current_l1);
     if (result) {
        get_field(field);
        current_l1 = field;
      }
     result=test_field(field,id_current_l2);
     if (result) {
        get_field(field);
        current_l2 = field;
      }
     result=test_field(field,id_current_l3);
     if (result) {
        get_field(field);
        current_l3 = field;
      }
   }
}

void convert_equipment_id(char *mystring) { //coded in HEX
  int len =strlen(mystring);
  for (int i=0; i<len/2; i++)
    mystring[i]=char(((int(mystring[i*2]))-48)*16+(int(mystring[i*2+1])-48));
  mystring[(len/2)]= NULL;
}

void get_field(char *mystring) {
  int a = strcspn (mystring,"(");
  int b = strcspn (mystring,")");
  int j = 0;
  for (int i=a+1; i<b; i++) {
    mystring[j] = mystring[i];
    j++;
  }
  mystring[j]= NULL;
}

bool test_field(char *field2, char *dmsr_field_id) {
  bool result = true;
  for (int i=0; i<10; i++) {
    if(field2[i] == dmsr_field_id[i]) result= result && true;
    else result = false;
  }
  return result;
}

void print_raw_data(int Tlength) {
  Serial1.print("Raw length in Byte: ");
  Serial1.println(Tlength);
  Serial1.println("Raw data: ");
  int mul = (Tlength/sll);
  for(int i=0; i<mul; i++) {
    for(int j=0; j<sll;j++) { Serial1.print(telegram[i*sll+j],HEX); }
    Serial1.println();
  }
  for(int j=0; j<(Tlength%sll);j++){ Serial1.print(telegram[mul*sll+j],HEX); }
  Serial1.println();
}

void print_vector(TestVector *vect) {
    int ll = 64;
    Serial1.print("\nVector_Name: ");
    Serial1.println(vect->name);
    Serial1.print("Key: ");
    for(int cnt=0; cnt<16;cnt++)
        Serial1.print(vect->key[cnt],HEX);
    Serial1.print("\nData (Text): ");
    int mul = (vect->datasize/sll);
    for(int i=0; i<mul; i++) {
      for(int j=0; j<sll;j++) { Serial1.print(vect->ciphertext[i*sll+j],HEX); }
      Serial1.println();
    }
    for(int j=0; j<(Tlength%sll);j++){
      Serial1.print(vect->ciphertext[mul*sll+j],HEX);
    }
    Serial1.println();
    Serial1.print("\nAuth_Data: ");
    for(int cnt=0; cnt<17;cnt++)
        Serial1.print(vect->authdata[cnt],HEX);
    Serial1.print("\nInit_Vect: ");
    for(int cnt=0; cnt<12;cnt++)
        Serial1.print(vect->iv[cnt],HEX);
    Serial1.print("\nAuth_Tag: ");
    for(int cnt=0; cnt<12;cnt++)
        Serial1.print(vect->tag[cnt],HEX);
    Serial1.print("\nAuth_Data Size: ");
    Serial1.println(vect->authsize);
    Serial1.print("Data Size: ");
    Serial1.println(vect->datasize);
    Serial1.print("Auth_Tag Size: ");
    Serial1.println(vect->tagsize);
    Serial1.print("Init_Vect Size: ");
    Serial1.println(vect->ivsize);
    Serial1.println(); }

void print_dsmr() {
  Serial1.println(identification);
  Serial1.println(p1_version);
  Serial1.println(timestamp);
  Serial1.println(equipment_id);
  Serial1.println(energy_delivered_tariff1);
  Serial1.println(energy_returned_tariff1);
  Serial1.println(reactive_energy_delivered_tariff1);
  Serial1.println(reactive_energy_returned_tariff1);
  Serial1.println(power_delivered);
  Serial1.println(power_returned);
  Serial1.println(reactive_power_delivered);
  Serial1.println(reactive_power_returned);
  Serial1.println(electricity_threshold);
  Serial1.println(electricity_switch_position);
  Serial1.println(electricity_failures);
  Serial1.println(electricity_sags_l1);
  Serial1.println(electricity_sags_l2);
  Serial1.println(electricity_sags_l3);
  Serial1.println(electricity_swells_l1);
  Serial1.println(electricity_swells_l2);
  Serial1.println(electricity_swells_l3);
  Serial1.println(message_short);
  Serial1.println(message2_long);
  Serial1.println(message3_long);
  Serial1.println(message4_long);
  Serial1.println(message5_long);
  Serial1.println(current_l1);
  Serial1.println(current_l2);
  Serial1.println(current_l3);
}
