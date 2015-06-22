
#ifndef _VITO_CLIENT_H_
#define _VITO_CLIENT_H_


#include <Ethernet.h>

byte VitoClient_Check(void);
void VitoClient_Init(void);
void Vito_ProcessData(EthernetClient cl, char chr);
void VitoClient_ReadParameters(void);
void VitoClient_NewDay(void);
void VitoClient_SetVitoTime(void);
void VitoClient_CheckDHW(void);

extern EthernetClient vito_client;


#endif


