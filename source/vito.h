
#ifndef _VITO_H_
#define _VITO_H_

#include <Arduino.h>
#include <SdFat.h>

typedef struct //Params
{
	const char * name;			// PGM_P == const char *
	unsigned int addr;
	unsigned char len;
	unsigned char divid;
	char typ;
} vito_param_t;

#define PARAM_READINGS_SIZE 100
extern const char * const read_params[];
extern vito_param_t vito_param;
extern byte send_frame[];
extern byte rec_frame[];
extern char param_readings[PARAM_READINGS_SIZE];
//
void SaveParamsToSD(char * pars, char * vals);
void ReadFileFromSD(void);
void File_Init(void);
uint8_t File_Exists(char * filename);
uint8_t File_Open(char * filename);
void File_Close(void);
int File_Read(uint8_t * strt, int bytesRead);
//
void Socket_init(void);
//
void Vito_ReceiveInit(void);
byte Vito_ReceiveData(byte data);
void Vito_ReadData(void);
byte Vito_ParseRecData(void);

void Vito_GetParam(byte in);
char * Vito_GetParamName(byte in);

signed char GetKeyIndex(const char * key);
uint8_t Vito_BuildCommand(byte in, char cmd);//, void * value);

extern const char temp_aussen[] 		PROGMEM;
extern const char temp_luft_ein[]		PROGMEM;
extern const char temp_luft_aus[]		PROGMEM;
extern const char temp_hzg_vl[]		        PROGMEM;
extern const char temp_hzg_rl[]		        PROGMEM;
extern const char temp_ww[]			PROGMEM;
extern const char status_verdichter[]		PROGMEM;
extern const char status_pumpe_hzg[]		PROGMEM;
extern const char status_pumpe_ww[]		PROGMEM;
extern const char verdichter_betriebstd[]	PROGMEM;
extern const char verdichter_nrstarts[]	        PROGMEM;
extern const char wp_uhrzeit[]			PROGMEM;
extern const char wp_betriebsart[]              PROGMEM;
extern const char strom_wp[]			PROGMEM;
extern const char strom_h[]			PROGMEM;
extern const char strom_datum[]		        PROGMEM;

#endif


