
#ifndef _VITO_CLIENT_H_
#define _VITO_CLIENT_H_


#include <Ethernet.h>

byte Vito_ClientCheck(void);
void Vito_ClientInit(void);
void Vito_ProcessData(EthernetClient cl, char chr);
void Vito_ReadParameters(void);
void Vito_ClientNewDay(void);
void Vito_ClientSetVitoTime(void);

extern EthernetClient vito_client;


#endif


