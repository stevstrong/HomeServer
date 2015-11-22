
#ifndef _VITO_CLIENT_H_
#define _VITO_CLIENT_H_

#ifndef USE_RS485
 #include <Ethernet.h>
 extern EthernetClient vito_client;
#endif

uint8_t VitoClient_Check(void);
void VitoClient_Init(void);
void VitoClient_ReadParameters(void);
void VitoClient_NewDay(void);
void VitoClient_SetVitoTime(void);
void VitoClient_CheckDHW(void);


#endif


