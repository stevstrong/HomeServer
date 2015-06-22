#include "sys_cfg.h"
#include <SPI.h>
#include <Ethernet.h>
#include <Time.h>
#include "vito_client.h"
#include "vito.h"
#include "file_client.h"

// Initialize the Ethernet client library with the IP address and port of the server
// that you want to connect to (port 23 is default for telnet);
EthernetClient vito_client;
// local functions
byte VitoClient_Connect(void);
byte VitoClient_GetReply(byte chrs);
byte VitoClient_SendGet(void);
/*****************************************************************************/
// read a parameter value out from device
/*****************************************************************************/
char * VitoClient_ReadParameter(const char * key)
{
	Vito_BuildCommand( GetKeyIndex(key), 'r' );
	VitoClient_SendGet();
	vito_client.stop();
	Vito_ParseRecData();
   return (char*)rec_frame;
}
/*****************************************************************************/
// read a parameter value out from device
/*****************************************************************************/
void VitoClient_WriteParameter(const char * key)
{
	Vito_BuildCommand( GetKeyIndex(key), 'w' );
	VitoClient_SendGet();
	vito_client.stop();
}
/*****************************************************************************/
// returns the value of the parameter from the last reading log
/*****************************************************************************/
char * VitoClient_GetParameterValue(char * paramName)
{
	strcpy(param_name, paramName);	// load param name
	// get parameter value in string format
	return File_GetRecordedParameter(-1);
}
/*****************************************************************************/
// DHW (domestic hot water) Control
// set the hw value according to time and day
// it should be activated on working days between 03:00 and 05:00.
/*****************************************************************************/
void VitoClient_CheckDHW(void)
{
	// check week-day and time
	if ( weekday()<5 && hour()>4 && hour()<5 )
	{
		Serial.println(F("checking hw ... "));
		//  get the hw temp value from last readings
		Vito_GetParamName(GetKeyIndex(temp_ww));	// load param name
		char * p = File_GetRecordedParameter(-1);	// read last recorded value
		int hw = 10*atoi(p);	// convert it into interger
		if ( hw<100 ) return;	// feasibility test, hw should never get lower than 10°C
		if ( (p=strchr(p,'.'))>0 ) hw += atoi(p); // add unary value to 10x value
		// get last status-pumpe-ww
		Vito_GetParamName(GetKeyIndex(status_pumpe_ww));	// load param name
		char * p1 = File_GetRecordedParameter(-1);	// read last recorded value
		byte pump = atoi(p1);
		// read set hw temp value from device
		char * p2 = VitoClient_ReadParameter(temp_ww_soll);	// read parameter from device
		int hw_set = atoi(p2);
		// do here the control
		//	send_frame = {0x41, length, 0x00, command, adrr_high, addr_low, nr_bytes, CRC/data};	// CRC in read mode
		//	41 07 00 02 60 00 02 f4 01 0x60
		if ( pump>0 && hw_set>450 ) {
			// set hw temp back to normal
			Serial.println(F("set hw back to 45°C."));
			File_LogMessage(PSTR("Vito DHW set back to 45°C."), NEW_ENTRY | ADD_NL | P_MEM); 
			*(uint16_t*)(send_frame+7) = 450;
			VitoClient_WriteParameter(temp_ww_soll);
		} else if ( pump==0 && hw<400 && hw_set==450 ) {
			// set hw temp to 50°C
			Serial.println(F("set hw to 50°C."));
			File_LogMessage(PSTR("Vito DHW set to 50°C."), NEW_ENTRY | ADD_NL | P_MEM); 
			*(uint16_t*)(send_frame+7) = 500;	// 50 °C
			VitoClient_WriteParameter(temp_ww_soll);
		} else {
			Serial.println(F("nothing changed."));
		}
	}
}
/*****************************************************************************/
/*****************************************************************************/
void VitoClient_Init(void)
{
//  Serial.println(F("client init..."));
	Vito_ReceiveInit();
}
/*****************************************************************************/
/*****************************************************************************/
byte Vito_ResetPoll(void)
{
#define MAX_RETRY  3
	byte dBuf[] = {0x16, 0x00, 0x00};
//  Serial.println(F("reset poll..."));
	byte i = MAX_RETRY, ret = 0;
	while ( ret==0 && (i--)>0 ) {
		WDG_RST;
		if ( VitoClient_Connect()!=1 ) continue;	// connection error
		vito_client.flush();  // empty receive buffer
		vito_client.write(dBuf, 3);  // reset polling
		// wait for answer 06
		ret = VitoClient_GetReply(1);
	}
	if ( ret==0 ) {	// error occured
		WDG_RST;
#if _DEBUG_>0
		Serial.println(F("ERROR: Vito_ResetPoll: polling could not be stopped!"));
#endif
	}
	return ret;
}
/****************************************************************************/
/****************************************************************************/
byte VitoClient_Check(void)
{
//  Serial.println(F("client check..."));
	byte ret = Vito_ResetPoll();
	// re-initialise Ethernet
	if ( ret==0 )  {
		File_LogError(PSTR("Vito client check failure."), NEW_ENTRY | ADD_NL | P_MEM); 
//    while(1);  // let the watch-dog do the rest... // Ethernet_Init();
	}
	return ret;
}
/****************************************************************************/
/****************************************************************************/
byte VitoClient_GetReply(byte chrs)
{
byte ret = 0;
//  Serial.println(F("wait for reply..."));
	uint32_t time2 = millis() + 3000;	// try up to 3 seconds
	while ( vito_client.connected() && ret==0 ) {
		WDG_RST;
		if ( millis()>time2 ) {
#if _DEBUG_>0
			Serial.println(F("ERROR: VitoClient_GetReply: reply from Vito timed out!"));
#endif
			break;
		}
		// check here for available input data
		while ( vito_client.available() ) {
			byte rd = vito_client.read();
			if ( ret==0 ) {
				if ( chrs==1 && rd==0x06 )
					ret = 1;
				else
					ret = Vito_ReceiveData(rd); // process only if not yet received all bytes
			}
		}
		delay(10);
	}
	if ( ret==0 ) vito_client.stop();  // workaround for hang-up
	return ret;  // terminate reception if no data received after 3s or not connected anymore
}
/*****************************************************************************/
/*****************************************************************************/
byte VitoClient_SendGet(void)
{
	byte retry = 1;
	while ( (retry--)>0 ) {
		// connect to Vito
		if ( VitoClient_Check()==0 ) continue;  // don't go on if there is no connection
		// be sure that Vito client is connect before calling this function
		vito_client.write(send_frame, send_frame[1]+3);
		// wait for reply
		return VitoClient_GetReply(0);
	}
}
/*******************************************************************************/
 static byte byte2bcd(byte ival)
 {
 	return ((ival / 10) << 4) | (ival % 10);
 }
/*******************************************************************************/
void VitoClient_SetVitoTime(void)
{
  // set date-time of Vito
  File_Log(PSTR("Setting Vito date and time."), NEW_ENTRY | ADD_NL | P_MEM);
  signed char index = GetKeyIndex(wp_uhrzeit);
#if _DEBUG_>0
  Serial.print(F("Setting Vito date-time parameter: ")); Serial.println(Vito_GetParamName(index));
#endif
  // initialize string to send: 2015011106162558
  send_frame[7] = byte2bcd(year()/100);
  send_frame[8] = byte2bcd(year()%100);
  send_frame[9] = byte2bcd(month());
  send_frame[10] = byte2bcd(day());
  send_frame[11] = byte2bcd((weekday()+5)%7);  // day of week
  send_frame[12] = byte2bcd(hour());
  send_frame[13] = byte2bcd(minute());
  send_frame[14] = byte2bcd(second());
// build here command frame
  Vito_BuildCommand(index, 'w');
#if 0 //_DEBUG_>0
  Serial.print(F("After build: "));
  for (byte i=0; i<16; i++) {
    byte c = send_frame[i];
    if ( c<16 ) Serial.print(0);
    Serial.print(c,HEX);
  }
  Serial.println();
//    return;
#endif
// send here command and get reply
  if ( VitoClient_SendGet()==0 ) {  // get all data
#if _DEBUG_>0
    Serial.println(F("Set Vito date/time could not get reply!"));
#endif
    File_LogError(PSTR("Set Vito date/time could not get reply!"), NEW_ENTRY | ADD_NL | P_MEM);
  }
}
/*****************************************************************************/
 /*****************************************************************************/
void VitoClient_NewDay(void)
{
#if 0	// not needed because current not read by pulse
	// - reset current counters of the Vito-bridge ->set new day
  File_Log(PSTR("Setting Vito bridge date and time."), NEW_ENTRY | ADD_NL | P_MEM);
	// set new day for the Vito-bridge: send new date
  signed char index = GetKeyIndex(strom_datum);
#if _DEBUG_>0
  Serial.println(Vito_GetParamName(index));
#endif
  // initialize string to send
  send_frame[7] = day();
  send_frame[8] = month();
  send_frame[9] = year()>>8;
  send_frame[10] = year();
// build here command frame
  Vito_BuildCommand(index, 'w');
// send here command frame and get reply
  if ( VitoClient_SendGet()==0 ) {
#if _DEBUG_>0
    Serial.println(F("Error: Strom-Datum could not get reply!"));
#endif
//    File_LogError(PSTR("Strom-Datum could not get reply!"), NEW_ENTRY | ADD_NL | P_MEM);
  }
#endif
  VitoClient_SetVitoTime();
}
/*****************************************************************************/
/*****************************************************************************/
void VitoClient_ReadParameters(void)
{
//  Serial.println(F("Vito_ReadParameters started..."));
	// initialize parameter value string
	memset(param_readings, 0, sizeof(param_readings));
	// write time  if (j==0)
	sprintf_P(param_readings, PSTR("%02i:%02i"), hour(), minute());
	// read all parameters from the array 'read_params[]'
	byte j = 0;
	byte p_ind = 5;  // next char position after previously written minute
	int8_t index;
	while ( (index = GetKeyIndex( (const char *)pgm_read_word(&read_params[j]) ))>=0 )
	{
		// read parameter value from Vito
		byte len = Vito_BuildCommand(index, 'r');
/*
		for (i=0; i<len; i++) { dat = send_frame[i]; if (dat<16) Serial.print('0'); Serial.print(dat, HEX); }
		Serial.println();
*/
		// get all reply bytes
		if ( VitoClient_SendGet()==0 ) {
#if _DEBUG_>0
			Serial.print(F("ERROR: Read parameters: '"));
			Serial.print(Vito_GetParamName(index));
			Serial.println(F("' could not be read!"));
#endif
			param_readings[p_ind++] = ',';	// add empty parameter value to the string
			j++;
			continue;
		}
		
#if _DEBUG_>1
//    Serial.println(F("Read parameters: client checked."));
      Serial.println(Vito_GetParamName(index));
#endif
		// parse here the received data
		Vito_ParseRecData();
//  Serial.print(F("parsed data: "));
#if _DEBUG_>1
      Serial.println((char*)rec_frame);
#endif
      // add the value to the readings
      param_readings[p_ind++] = ',';
      strcpy(&param_readings[p_ind], (char*)rec_frame);
      int8_t s_len = strlen((char*)rec_frame);
//  Serial.print(F("parsed string length: ")); Serial.println(s_len);
      p_ind += s_len;
      j++;  // only advance here to the next parameter, if everything was OK
	}
	vito_client.stop();
#if _DEBUG_>0
	Serial.println(param_readings);
//  Serial.println(F("Vito_ReadParameters done."));
#endif
}
/*****************************************************************************/
/*****************************************************************************/
byte VitoClient_Connect(void)
{
	if ( vito_client.connected() ) return 1;

	byte vito_ip[] = {192,168,100,40};
	int vito_port = 8088;
	int8_t repl = 0;
#if _DEBUG_>0
	Serial.print(F("Connecting to Vito ... "));
#endif
	// if you get a connection, report back via serial:
	uint32_t time = millis()+5000;  // time within to get reply
	while (1) {
		WDG_RST;
		repl = vito_client.connect(vito_ip, vito_port);
		if ( repl>0 ) break;
		if ( millis()>time ) {  // if no answer received within the prescribed time
			//time_client.stop();
#if _DEBUG_>0
			Serial.print(F("timed out..."));
#endif
			break;
		}
		delay(100);
	}
	if ( repl<=0 ) {
#if _DEBUG_>0
		// if you didn't get a connection to the server:
		Serial.print(F("failed: "));
		if ( repl==-1 ) Serial.println(F("timed out."));
		else if ( repl==-2 ) Serial.println(F("invalid server."));
		else if ( repl==-3 ) Serial.println(F("truncated."));
		else if ( repl==-4 ) Serial.println(F("invalid response"));
		else Serial.println(repl);
#endif
		return repl;
	}
  // connection was successful.
#if _DEBUG_>0
	Serial.println(F("done."));
#endif
	return 1;
}
