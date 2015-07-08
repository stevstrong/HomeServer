/******************************************************************************
	Dreambox
****************************************
******************************************************************************/
#include "sys_cfg.h"
#include "dreambox.h"
#include <Ethernet.h>
#include "ether_server.h"
#include "ether_client.h"
#include "file_client.h"
#include "vito.h"
//#include "serial1.h"

//#define DM800SE	0
//#define DM800		1
typedef enum { DM800SE, DM800 } dm_device_t;
dm_device_t dm_device;
typedef enum { DM_OK, DM_STANDBY, CONNECTION_TIMEOUT } dm_state_t;
dm_state_t dm_state[2];
typedef enum { CMD_TIMER_LIST, CMD_TIMER_CLEANUP, CMD_GET_STATUS} dm_cmd_t;
dm_cmd_t dm_cmd;
// Initialize the Ethernet client library with the IP address and port of the server
// that you want to connect to (port 23 is default for telnet);
EthernetClient dm_client;
byte dm_ip[2];
int dm_port = 80;
//
typedef enum {
	START = 1,
	DATA = 2,
	END = 4,
} list_status_t;	// add here further clients
byte reply;
/*****************************************************************************/
// callback function to receive header line data (after each "\r\n")
/*****************************************************************************/
byte Dreambox_ParseHeaderLine(void)
{
	return 1; // flush any header info, don't need
}
/*****************************************************************************/
// callback function to receive payload line data (after each "\r\n")
/*****************************************************************************/
byte Dreambox_ParsePayloadLine(void)
{
//<?xml version="1.0" encoding="UTF-8"?>
	// check end of XML header line and remove the entire line from the buffer
	char * p = strstr_P(s_buf, PSTR("?>"));
	if ( p>0 ) {
		p += 2; // set pointer to the string to copy
		if ( *p==0 )	s_ind = 0;	// set pointer to the beginning of the buffer
		else {
			strcpy(s_buf, p);
			s_ind -= (p - s_buf);	// update index
		}
		Serial.println(F("DM parsed payload line: XML header removed."));
	} else {
	// parse here the data dependent on the command
		switch ( dm_cmd ) {
			case CMD_TIMER_LIST:
/* reply:
<e2timerlist>
</e2timerlist>
*/
				if ( strstr_P(s_buf, PSTR("</e2timerlist>")) ) {
					reply |= END;
					return 1;	// flush any further data
				}
				p = strstr_P(s_buf, PSTR("<e2timerlist>"));
				if ( p>0 ) {
					reply |= START;
					// remove all data before this xml tag from buffer
					p += 13; // set pointer to the end of this tag
					if ( *p==0 )	s_ind = 0;	// set pointer to the beginning of the buffer
					else {
						strcpy(s_buf, p);
						s_ind -= (p - s_buf);	// update index
					}
				}
				if ( (reply & START) && strlen(s_buf)>0 ) {
					reply |= DATA;
					return 1;	// flush any further data
				}
				break;
			case CMD_TIMER_CLEANUP:
/* reply:
<?xml version="1.0" encoding="UTF-8"?>
<e2simplexmlresult>
	<e2state>True</e2state>
	<e2statetext>List of Timers has been cleaned</e2statetext>	
</e2simplexmlresult>
*/
				break;
			case CMD_GET_STATUS:
				break;
		}
	}
	return 0;
}
/*****************************************************************************/
/*****************************************************************************/
void Dreambox_Init(void)
{
	dm_device = DM800SE;
	dm_state[DM800SE] = DM_OK;
	dm_state[DM800] = DM_OK;
	dm_ip[DM800SE] = (192,168,100,25);
	dm_ip[DM800] = (192,168,100,44);
}
/*****************************************************************************/
void Dreambox_GetReply(void)
{
	EtherClient_ReceiveData(dm_client, CLIENT_DREAMBOX);
#if _DEBUG_>0
	Serial.print(F("Dreambox received: ")); Serial.println(s_buf);
#endif
}
/*****************************************************************************/
void Dreambox_Disconnect(void)
{
	dm_client.stop();
}
/*****************************************************************************/
void Dreambox_Connect(void)
{
	int reply;
#if _DEBUG_>0
	Serial.print(F("Connecting to "));
	if (dm_device==DM800SE )
		Serial.print(F("DM800SE ... "));
	else
		Serial.print(F("DM800 ... "));
#endif
	// if you get a connection, report back via serial:
	long timeout = millis() + 3000;  // time within to get reply
	while (1) {
		if ( millis()>timeout ) {  // if no answer received within the prescribed time
			//time_client.stop();
#if _DEBUG_>0
			Serial.print(F("timed out..."));
#endif
			break;
		}
		WDG_RST;
		reply = dm_client.connect(dm_ip[dm_device], dm_port);
		if ( reply>0 ) break;
		else  delay(100);
	}
	if ( reply<=0 ) {
#if _DEBUG_>0
		// if you didn't get a connection to the server:
		Serial.print(F("failed: "));
		if ( reply==-1 ) Serial.println(F("timed out."));
		else if ( reply==-2 ) Serial.println(F("invalid server."));
		else if ( reply==-3 ) Serial.println(F("truncated."));
		else if ( reply==-4 ) Serial.println(F("invalid response"));
		else Serial.println(reply);
#endif
		dm_state[dm_device] = CONNECTION_TIMEOUT;
	} else {
    // connection was successful.
#if _DEBUG_>0
		Serial.println(F("done."));
#endif
		dm_state[dm_device] = DM_OK;
	}
}
/*****************************************************************************/
/*****************************************************************************/
byte DM_Error(void)
{
	dm_client.stop();
	return dm_state[dm_device];
}
/*****************************************************************************/
/*****************************************************************************/
byte Dreambox_TimerList_Read(void)
{
	strcpy_P(s_buf, PSTR("POST /web/timerlist HTTP/1.1\r\n"));
	dm_client.println(s_buf);
/* reply:
<?xml version="1.0" encoding="UTF-8"?>
<e2timerlist>
</e2timerlist>
*/
	reply = 0;
	dm_cmd = CMD_TIMER_LIST;
	Dreambox_GetReply();
	return (reply & DATA);
}
/*****************************************************************************/
void Dreambox_TimerList_Cleanup(void)
{
	strcpy_P(s_buf, PSTR("POST /web/timercleanup?cleanup=true HTTP/1.1\r\n"));
	dm_client.println(s_buf);
/* reply:
<?xml version="1.0" encoding="UTF-8"?>
<e2simplexmlresult>
	<e2state>True</e2state>
	<e2statetext>List of Timers has been cleaned</e2statetext>	
</e2simplexmlresult>
*/
	reply = 0;
	dm_cmd = CMD_TIMER_CLEANUP;
	Dreambox_GetReply();
//	return (reply & DATA);
}
/*****************************************************************************/
void Dreambox_SetToStandby(void)
{
}
/*****************************************************************************/
void Dreambox_CheckStandby(void)
{
	strcpy_P(s_buf, PSTR("POST /web/powerstate HTTP/1.1\r\n"));
	dm_client.println(s_buf);
/*
<e2powerstate>
<e2instandby>true</e2instandby>
</e2powerstate>
*/
//				Dreambox_SendMessage(PSTR("Receiver will now be turned off ..."));
				delay(5000);
				Dreambox_SetToStandby();
}
/*****************************************************************************/
void Dreambox_CheckStatus(void)
{
	if ( dm_device==DM800SE ) return;	// dummy to skip
	byte dm_dev = dm_device;
	for ( dm_dev=DM800SE; dm_dev<=DM800; dm_dev++) {
		Dreambox_Connect();
		if ( dm_state[dm_dev]==DM_OK ) {
			Dreambox_CheckStandby();
			Dreambox_TimerList_Cleanup();
			//if ( Dreambox_TimerList_Read()==0 )
				//Dreambox_SwitchOff();
		}
		Dreambox_Disconnect();
	}
}
