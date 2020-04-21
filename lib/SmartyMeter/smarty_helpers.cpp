#define SMARTY_DEBUG
#include "debug_helpers.h"
#include "smarty_helpers.h"

#include <AES.h>
#include <Crypto.h>
#include <GCM.h>

/*
      Print hex dump on debug channel, formatted for use as 'fake_vector'
*/
void print_telegram(uint8_t telegram[], int telegram_size)
{
    const int bpl = 22; // bytes per line
    DEBUG_PRINTF("print_telegram with length: %d\n", telegram_size);
    DEBUG_PRINTLN("Raw data for import in smarty_user_config.h:");
    DEBUG_PRINTLN("const char fake_vector[] = {");
    int mul = (telegram_size / bpl);
    for (int i = 0; i < mul; i++)
    {
        DEBUG_PRINT("    ");
        for (int j = 0; j < bpl; j++)
        {
            DEBUG_PRINT("0x");
            print_hex(telegram[i * bpl + j]);
            DEBUG_PRINT(", ");
        }
        DEBUG_PRINTLN();
    }
    DEBUG_PRINT("    ");
    for (int j = 0; j < (telegram_size % bpl); j++)
    {
        DEBUG_PRINT("0x");
        print_hex(telegram[mul * bpl + j]);
        if (j < (telegram_size % bpl) - 1)
        {
            DEBUG_PRINT(", ");
        }
    }
    DEBUG_PRINTLN("};\n");
}

/*  
    Decode the raw data and fill the vector
    Return true if successful
*/
bool init_vector(uint8_t telegram[], Vector *vect, const char *Vect_name, uint8_t *key_SM)
{
    DEBUG_PRINTLN("Entering init_vector");

    if (telegram[0] != 0xDB) {
        DEBUG_PRINTLN("ERROR, first byte of telegram should be 0xDB, aborting.");
        return false;
    }

    vect->name = Vect_name; // vector name
    for (int i = 0; i < 16; i++)
        vect->key[i] = key_SM[i];
    uint16_t Data_Length = uint16_t(telegram[11]) * 256 + uint16_t(telegram[12]) - 17; // get length of data
    DEBUG_PRINTF("init_vector: data length read in telegram: %d\n", Data_Length);
    if ((Data_Length+18) > MAX_TELEGRAM_LENGTH) {
        DEBUG_PRINTF("ERROR: data length (%d) is too large for MAX_TELEGRAM_LENGTH (%d)\n", Data_Length, MAX_TELEGRAM_LENGTH);
        return false;
    }
    for (int i = 0; i < Data_Length; i++)
        vect->ciphertext[i] = telegram[i + 18];
    uint8_t AuthData[] = {0x30, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                          0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
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
    return true;
}


/* 
  Decrypt text in the vector and put it in the buffer
*/
void decrypt_vector_to_buffer(Vector *vect, char buffer[])
{
    GCM<AES128> *gcmaes128 = 0;

    DEBUG_PRINTLN("Entering decrypt_vector_to_buffer");
    gcmaes128 = new GCM<AES128>();
    size_t posn, len;
    size_t inc = vect->datasize;
    int buffer_size = sizeof(buffer);
    memset(buffer, 0, buffer_size); // ensure final string will be zero terminated
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
    DEBUG_PRINTLN("Exiting decrypt_vector_to_buffer");
}

void convert_equipment_id(char *mystring)
{
    // coded in HEX
    //DEBUG_PRINTLN("Entering convert_equipment_id");
    int len = strlen(mystring);
    for (int i = 0; i < len / 2; i++)
        mystring[i] = char(((int(mystring[i * 2])) - 48) * 16 + (int(mystring[i * 2 + 1]) - 48));
    mystring[(len / 2)] = 0;
}

/*
  Replace mystring with the value found in the first set of parentheses.
  e.g. 1-0:3.7.0(00.000) becomes 00.000
*/
void replace_by_val_in_first_braces(char *mystring)
{
    //DEBUG_PRINTLN("Entering replace_by_val_in_first_braces");
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
  Replace mystring with the value found in the *last* set of parentheses.
  e.g. 0-1:24.2.1(101209112500W)(12785.123*m3) becomes 12785.123*m3
*/
void replace_by_val_in_last_braces(char *mystring)
{
    //DEBUG_PRINTLN("Entering replace_by_val_in_last_braces");
    // get pointer to last occurrence of '('
    char *ret = strrchr(mystring, '(');
    replace_by_val_in_first_braces(ret);
    strcpy(mystring, ret);
}

/*
  Remove all characters after a *, if present.
  12785.123*m3 becomes 12785.123
*/
void remove_unit_if_present(char *mystring)
{
    //DEBUG_PRINTLN("Entering remove_unit_if_present");
    char *pos = strchr(mystring, '*');
    if (pos)
        // replace character with terminating null
        *pos = 0;
}

void print_vector(Vector *vect)
{
    const int sll = 50; // length of a line if printing serial raw data

    DEBUG_PRINTLN("\nEntering print_vector");
    DEBUG_PRINTF("Vector_Name: %s\n", vect->name);
    DEBUG_PRINT("Key: ");
    for (int cnt = 0; cnt < 16; cnt++)
        print_hex(vect->key[cnt]);
    DEBUG_PRINT("\nData (Text): ");
    int mul = (vect->datasize / sll);
    for (int i = 0; i < mul; i++)
    {
        for (int j = 0; j < sll; j++)
        {
            print_hex(vect->ciphertext[i * sll + j]);
        }
        DEBUG_PRINTLN();
    }
    for (int j = 0; j < (vect->datasize % sll); j++)
    {
        print_hex(vect->ciphertext[mul * sll + j]);
    }
    DEBUG_PRINT("\nAuth_Data: ");
    for (int cnt = 0; cnt < 17; cnt++)
        print_hex(vect->authdata[cnt]);
    DEBUG_PRINT("\nInit_Vect: ");
    for (int cnt = 0; cnt < 12; cnt++)
        print_hex(vect->iv[cnt]);
    DEBUG_PRINT("\nAuth_Tag: ");
    for (int cnt = 0; cnt < 12; cnt++)
        print_hex(vect->tag[cnt]);
    DEBUG_PRINTF("\nAuth_Data Size: %d\n", vect->authsize);
    DEBUG_PRINTF("Data Size: %d\n", vect->datasize);
    DEBUG_PRINTF("Auth_Tag Size: %d\n", vect->tagsize);
    DEBUG_PRINTF("Init_Vect Size: %d\n", vect->ivsize);
    DEBUG_PRINTLN();
}

void print_hex(char x)
{
    if (x < 0x10) // add leading 0 if needed
    {
        DEBUG_PRINT("0");
    };
    DEBUG_PRINT(x, HEX);
}