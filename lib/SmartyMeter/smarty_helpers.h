#include "Arduino.h"

#ifndef smarty_helpers_h
#define smarty_helpers_h

#define MAX_TELEGRAM_LENGTH 1536

struct Vector
{
    const char *name;
    uint8_t key[16];
    uint8_t ciphertext[MAX_TELEGRAM_LENGTH];
    uint8_t authdata[17];
    uint8_t iv[12];
    uint8_t tag[16];
    uint8_t authsize;
    uint16_t datasize;
    uint8_t tagsize;
    uint8_t ivsize;
};

void print_telegram(uint8_t telegram[], int telegram_size);
void init_vector(uint8_t telegram[], Vector *vect, const char *Vect_name, uint8_t *key_SM);
void decrypt_vector_to_buffer(Vector *vect, char buffer[], int buffer_size);
void convert_equipment_id(char *mystring);
void replace_by_val_in_first_braces(char *mystring);
void replace_by_val_in_last_braces(char *mystring);
void remove_unit_if_present(char *mystring);
void print_vector(Vector *vect);
void print_hex(char x);

#endif // smarty_helpers_h