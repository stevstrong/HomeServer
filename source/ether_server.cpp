#include "sys_cfg.h"
#include <SPI.h>
#include <Ethernet.h>
#include <SdFat.h>
#include "ether_server.h"
#include "file_client.h"
#include "time_client.h"
#include "vito.h"
#include "vito_client.h"
#include "energy_cam.h"

// data buffer to store data received by the server
char s_buf[SERVER_BUFFER_MAX_SIZE];
byte s_ind;
// Initialize the Ethernet server on port 80
EthernetServer my_server(80);
uint8_t remoteip[4];
uint32_t oldip;
byte allow;
long accessTimeout;

void Ether_ServerProcessData(EthernetClient cl);
/*****************************************************************************/
/*****************************************************************************/
void Ether_ServerInit(void)
{
#if _DEBUG_>0
  Serial.print(F("Initializing Ethernet server..."));
#endif

  my_server.begin();
  s_ind = 0;
  allow = 0;
  *(uint32_t*)remoteip = 0;
  oldip = 1;
  accessTimeout = millis();

#if _DEBUG_>0
  Serial.println(F("done."));
#endif
}
/*****************************************************************************/
/*****************************************************************************/
void Ether_ServerReceiveData(EthernetClient cli)
{
#if _DEBUG_>0
//      Serial.print(F("ether server received: "));
#endif
  char c;
  s_ind = 0;
    // read and store the first header line only
    while ( cli.available() && s_ind<SERVER_BUFFER_MAX_SIZE )
    {
      c = cli.read();
#if _DEBUG_>1
//      if ( s_line>0 )  Serial.write(c);
#endif
      if ( c=='\r' )  continue;  // take next char
      if ( c=='\n' )  break;      // end of line
      s_buf[s_ind++] = c;
    }
    // only store limited number of chars, the rest will be ignored.
    if ( s_ind>=SERVER_BUFFER_MAX_SIZE )  s_ind = SERVER_BUFFER_MAX_SIZE-1;
    s_buf[s_ind] = 0;  // mark end of the received string
  // read here the rest of data
  while ( cli.available() )
      c = cli.read();
#if _DEBUG_>0
    Serial.println(s_buf);
#endif
}
/*****************************************************************************/
/*****************************************************************************/
void Ether_ServerCheckForClient(void)
{
  //Serial.println(F("checking server..."));
  // listen for incoming clients
  EthernetClient s_client = my_server.available();
  if ( !s_client)  return;

  Time_ClientUpdateFileString();  // update date and time

  s_client.getRemoteIP(remoteip);
#if _DEBUG_>0
  Serial.print(F("---> request from: "));
	for (byte bcount=0; bcount < 4; bcount++)
	{
		Serial.print(remoteip[bcount], DEC);
		if (bcount<3) Serial.print(".");
	}
  Serial.println();
#endif
  // security check remote IP. search for the same ip in the local record (clients.txt)
  if ( oldip == *(uint32_t*)remoteip ) {
    // client is same as previous one
    if ( allow )  accessTimeout = millis() + 5*60*1000;  // renew timeout if it is registered IP
  } else {
    // new client
    // check if client is already recorded
    byte i=0,found=0;
    for (; i<100; i++) {
      strcpy_P(s_buf, PSTR("clients.txt"));
      File_GetFileLine(i);  // the input file name is in f_buf, the read line is returned also in f_buf
      if ( s_buf[0]==0 ) break;
      // the line is in s_buf. parse the Ip address
      char * chrPtr = strtok(s_buf, " ");
      chrPtr = strtok(NULL, " .");
  //    Serial.println(chrPtr);
      if ( remoteip[0]==atoi(strtok(NULL, " .")) && remoteip[1]==atoi(strtok(NULL, ".")) && remoteip[2]==atoi(strtok(NULL, ".")) && remoteip[3]==atoi(strtok(NULL, ".")) ) {
  //      allow = 1; accessTimeout = millis() + 5*60;  // allow access for 5 minutes
       Serial.print(F("client has index ")); Serial.println(i);
       found = 1;
       break;
      }
    }
    if ( found==0 ) {
      Serial.print(F("recording new client: "));
      sprintf(s_buf, "%u\.%u\.%u\.%u", remoteip[0], remoteip[1], remoteip[2], remoteip[3]);
      Serial.println(s_buf);
      File_LogClient(s_buf);
    }
    // update old ip
    oldip = *(uint32_t*)remoteip;
  }

  if ( s_client.available() ) {
    // read client request, store it into the s_buf and process it
    Ether_ServerReceiveData(s_client);
    Ether_ServerProcessData(s_client);
  }
#if _DEBUG_>0
  Serial.println(F("<--- client end."));
#endif
  s_client.stop();
}

/***********************************************************************************/
void putdecimal(word w, char * s, byte digs)
{
  s += digs;
//  *s = 0;  // write string ending mark
  do {
      *(--s) = ( w % 10 ) + '0';
  } while ( (w/=10)!=0 && (--digs)>0 );
}
/***********************************************************************************/
void Ether_BufAddInt(word w, byte digits)
{
  putdecimal(w, s_buf+s_ind, digits);
  s_ind += digits;
}
/***********************************************************************************/
void Ether_BufAdd_P(const char * str)
{
  strcpy_P(s_buf+s_ind, str);
  s_ind += strlen_P(str);
}
/***********************************************************************************/
/***********************************************************************************/
void Ether_PrintSimpleHeader(EthernetClient cl)
{
	cl.println(F("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\nConnection: close\r\n"));
}
/***********************************************************************************/
/***********************************************************************************/
void Ether_PrintStandardHeader(EthernetClient cl)
{
	Ether_PrintSimpleHeader(cl);
	cl.println(F("<!DOCTYPE HTML>\n<html>\n<head><meta charset='UTF-8'>\n<title>My homepage</title></head>\n"));
	cl.println(F("<body>\n<p>&emsp;<a href='/'>Home</a>&emsp;<a href='/files/'>SD-Card</a></p>"));
	cl.print(F("<p>It is ")); cl.print(dayStr(weekday())); cl.print(", 20"); cl.print(date_str); cl.print(", "); cl.print(time_str); cl.println(F("</p>"));
	//cl.print("20"); cl.print(date_str); cl.print(", "); cl.print(time_str); cl.println(F("</p>"));
}
/***********************************************************************************/
/***********************************************************************************/
void Ether_ServerProcessData(EthernetClient cl)
{
	uint32_t time0 = millis();
	if ( strncmp_P(s_buf, PSTR("GET /"), 5)==0 )
	{
		//if ( s_buf[5]=='?' ) {  // requested a file or execute a command
		if ( strncmp_P(s_buf+5, PSTR("?regmyip"), 8)==0 ) {  // requested a file or execute a command
			// register IP -> allow access for 5 minutes
			allow = 1;
			accessTimeout = millis() + 5*60*1000;  // allow access for 5 minutes
			goto bad_request_1;  // tricky 'bad request' response for unknown users
		}
      else if ( 0 )   //s_buf[5]==' ' )  // nothing specified
		{
			// load index page from SD-card line by line and add parameters value to the end
			strcpy_P(s_buf,PSTR("web/index.htm"));
			if ( !file.open(s_buf, O_READ) ) {
				//File_LogError(PSTR("Could not open 'index.htm'!"), NEW_ENTRY | ADD_NL | P_MEM);
				Serial.println(F("Could not open 'web/index.htm'!"));
				goto bad_request_1;
			}
			File_SendFile(cl);
			file.close();
			// send here the additional data
			cl.print(F("<div class='myp'>\n<p id='crtDateTime'>"));
			sprintf(s_buf, "%s - %s - %s", dayStr(weekday()), date_str, time_str);
			cl.print(s_buf);
			cl.print(F("</p>\n<p id='params'>"));
			File_GetRecordLine(0);
			cl.print(s_buf);
			cl.print(F("</p>\n<p id='values'>"));
			File_GetRecordLine(-1);
			cl.print(s_buf);
			cl.println(F("</p>\n</div>\n<script>ParseData();</script>"));
/*
			// send a standard HTTP response header
			//Ether_PrintStandardHeader(cl);
#if 0
			Vito_ReadParameters();
			EC_ReadValue();
			Serial.print(F("read line: ")); Serial.println(param_readings);
			char * ptr = strtok(param_readings, ",");
#else
			File_GetRecordLine(-1);
			//Serial.print(F("read line: ")); Serial.println(s_buf);
			char * ptr = strtok(s_buf, ",");
#endif
			cl.println(F("<style>th,td{text-align:right;}</style>"));
			cl.println(F("<table style='width:20%'>\n<tr><th style='text-align:left'>Parameter</th><th>Value</th></tr>"));
			s_ind = 32;
			for (byte i=0; ; i++) {
				signed char index = GetKeyIndex((const char *)pgm_read_word(&read_params[i]));  // get the index of the parameter to log
				if ( index<0 ) break;
				cl.print(F("<tr><td style='text-align:left'>"));
				cl.print( Vito_GetParamName(index) );
				cl.print( F("</td><td>"));
				if ( ptr!=0 ) { ptr = strtok(NULL, ","); cl.print( ptr ); }
				cl.println( F("</td></tr>"));
			}
			// add EnergyCam absolute and relative values
			cl.print(F("<tr><td style='text-align:left'>Strom-H_abs</td><td>"));
			if ( ptr!=0 ) { ptr = strtok(NULL, ","); cl.print( ptr ); }
			cl.println( F("</td></tr>"));
			cl.print(F("<tr><td style='text-align:left'>Strom-H_rel</td><td>"));
			if ( ptr!=0 ) { ptr = strtok(NULL, ",\n"); cl.print( ptr ); }
			cl.println( F("</td></tr>\n</table>"));
			cl.println(F("</body></html>"));
*/
		} else {	// parse the requested dir/file
//dir_file_check:
			char * str = strchr(s_buf+5, ' ');	// find next space pointing before 'HTTP1/x'
			if ( str==0 ) goto bad_request;
			*str = 0;  // tricky marking end of dir/file to 0
			if ( *(str-1)=='/' ) *(str-1) = 0;	// remove any directory delimiter '/' from the end
			// set string to lower case
			strlwr(s_buf+5);
			// list dir or send file content of SD-card
			File_CheckDirFile(cl);
		}
	} else if ( strncmp_P(s_buf, PSTR("POST /"), 6)==0 ) {  // requested a file or execute a command
		// POST /readfile=15-06-07.TXT HTTP/1.1
		*(strchr(s_buf+5, ' ')) = 0;
		// feasibility check
		if ( strchr(s_buf+5, '=')>0 ) {
			// check POST request
			File_CheckPostRequest(cl);
		}
	} else {  //    unknown request
bad_request:
#if _DEBUG_>0
      Serial.print(F("Ether server: unknown request: ")); Serial.println(s_buf+5);
#endif
bad_request_1:
      cl.println(F("HTTP/1.0 400 Bad Request\r\nContent-Type:text/plain\r\nConnection:close\r\n\r\nBad Request:"));
      cl.println(s_buf+5);
  }
proc_end:
  // send here required time info
//  if ( s_buf[0]>0 ) { cl.print(F("\n<p style='text-align:right'>Page sent in ")); cl.print(millis()-time0); cl.println(F(" ms.</p>")); }
  if ( s_buf[0]!=0 ) { cl.print(F("\nPage sent in ")); cl.print(millis()-time0); cl.println(F(" ms.")); }
}
