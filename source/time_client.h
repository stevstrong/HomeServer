#ifndef TIME_CLIENT_H
#define TIME_CLIENT_H

#include <Time.h>
#include <Ethernet.h>
#include <EthernetUdp.h>

void Time_ClientInit(void);
//int ConnectToTimeServer(void);
void Time_ClientPing(void);
//byte Time_ClientCheck(void);
void Time_ProcessData(EthernetClient client, char chr);
void Time_ParseData(char chr);
int Time_GetSecond(void);
void Time_ClientUpdateFileString(void);
byte Time_NewDay(void);

// variables
extern EthernetClient time_client;
extern char time_char;
extern uint32_t next_time;
extern char date_str[9];  // 8 chars + ending 0
extern char time_str[9];  // 8 chars + ending 0
extern char file_str[13]; // 12 chars + ending 0


#endif


