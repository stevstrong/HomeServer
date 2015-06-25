#include "sys_cfg.h"
//#include <Arduino.h>
//#include <Time.h>
#include "time_client.h"
#include "file_client.h"

//////////////////////////////////////////////////////////
// Configure the time server type (TCP or UDP/NTP). Enable only one of the lines below
#define TIME_SERVER_UDP
//#define TIME_SERVER_TCP
/////////////////////////////////////////////////////////


#ifdef TIME_SERVER_UDP
  // A UDP instance to let us send and receive packets over UDP
  EthernetUDP Udp;
  #define NTP_PACKET_SIZE  48
  char packetBuffer[NTP_PACKET_SIZE];
#elif defined TIME_SERVER_TCP
  EthernetClient time_client;
  #define TIME_BUF_SIZE  80
  char time_buf[TIME_BUF_SIZE];
  static byte ind;
  void ParseTCP(void);
#else
  #error "Undefined TIME_SERVER!"
#endif

uint32_t ctime;
byte counter;

char date_str[9];  // 8 chars + ending 0
char time_str[9];  // 8 chars + ending 0
char file_str[13]; // 12 chars + ending 0

void Time_ParseData(char chr);
void ParseHTML(void);

/****************************************************************************/
/****************************************************************************/
void TimeClient_Init(void)
{
  counter = 0;
//  next_time = 20000;  // next check intervall for getting time, will be adjusted dynamically
//  time_char = 0;
  // get the time as early as possible
  while (1) {
    TimeClient_Ping();
    if  ( timeStatus()!=timeNotSet ) break;
    if ( (++counter)>=30 ) while (1);  // watchdog reset
    delay(1000);
  }
}
/****************************************************************************/
#ifdef TIME_SERVER_TCP
  #error "Time server is TCP!"
/****************************************************************************/
int ConnectToTCPServer(void)
{
#define TIMEOUT_REPLY  3000  // ms
int repl;
static int nist_port = 13;
static char * nist_server = "nist.netservicesgroup.com";

#if _DEBUG_>0
	if ( time_client.connected() )
		Serial.println(F("Client was not disconnected!"));

    Serial.print(F("Connecting to TCP time server ... "));
#endif
    // if you get a connection, report back via serial:
    ctime = millis()+TIMEOUT_REPLY;  // time within to get reply
    while (1) {
      if ( millis()>ctime ) {  // if no answer received within the prescribed time
        //time_client.stop();
#if _DEBUG_>0
        Serial.print(F("timed out..."));
#endif
        break;
      }
        WDG_RST;
	repl = time_client.connect(nist_server, nist_port);  // NIST time server
        if ( repl>0 ) break;
        else  delay(100);
    }

    if ( repl<=0) {
#if _DEBUG_>0
      // if you didn't get a connection to the server:
      Serial.print(F("failed: "));
      if ( repl==-1 ) Serial.println(F("timed out."));
      else if ( repl==-2 ) Serial.println(F("invalid server."));
      else if ( repl==-3 ) Serial.println(F("truncated."));
      else if ( repl==-4 ) Serial.println(F("invalid response"));
      else Serial.println(repl);
#endif
      // monitor failures and reset TCP port if conection failed more than 10 consequtive times
      if ( (counter++)>10 )  {
        File_LogError(PSTR("TCP time server connection failure!"), NEW_ENTRY | ADD_NL | P_MEM); 
        while (1);  // let the watch-dog do his job
      }
      return repl;
    }
    counter = 0;  // reset connect failure counter
#if _DEBUG_>0
    // connection OK.
    Serial.println(F("done."));
//    if ( !time_client.connected() )  Serial.println(F("... but not really ..."));
#endif
//  delay(20);  // give time to get data if sent as result to establishing the connection
//  if ( !time_client.available() )
    // do here communication with server: send any data to get reply
    time_client.println(counter);
  return repl;
}
/****************************************************************************/
/****************************************************************************/
void ParseTCP(void)
{
  // data format:   "\n56998 14-12-07 16:13:51 00 0 0 928.4 UTC(NIST) *\n"

  if (time_buf[0]==0 ) return;  // invalid data, it is the first \n, so don't do yet anything, wait for the next '\n'

  //Serial.println(F("Received line: ")); Serial.println(time_buf);
  // check for validity
  if ( time_buf[17]==':' && time_buf[20]==':' ) {//&& strncmp_P(time_buf+37, time_id, sizeof(time_id))==0 ) { // valid time info
    //Serial.println(time_buf);
    //setTime(hr,min,sec,day,mnth,yr);
    setTime(atoi(time_buf+15), atoi(time_buf+18), atoi(time_buf+21), atoi(time_buf+12), atoi(time_buf+9), atoi(time_buf+6));
    adjustTime(3600);  // add 1 more hour -> adjust to UTC+1 time zone
    //    Serial.print(F("\nSeconds: ")); Serial.println(second());
    delay(2);  // delay in order to read correct hour
    millis();  // dummy call to update correct time
//    Time_ClientUpdateFileString();
    //file_str[12] = 0;  // mark end of string
  }
  ind = 0;
  time_buf[0] = 0;  // reset first byte to indicate invalid data
}
/****************************************************************************/
/****************************************************************************/
void Time_ParseTCPData(char chr)
{
  if ( chr=='\r' || chr=='\n' ) {  // end of TCP time string
    if ( ind>=TIME_BUF_SIZE ) ind = TIME_BUF_SIZE-1;  // adjust buffer end border
    time_buf[ind] = 0;  // indicate end of string
    // complete line has been received, parse the line
    ParseTCP();
  } else {
    if ( ind<TIME_BUF_SIZE )  time_buf[ind++] = chr;
  }
}
/****************************************************************************/
/****************************************************************************/
void Time_CheckTCP(void)
{
  ind = 0;
  time_buf[0] = 0;
  if ( ConnectToTCPServer()<=0 ) return;    // connect here to time server
    // connection successful, get reply now
  ctime = millis() + TIMEOUT_REPLY;  // time within to get reply
  while ( time_client.connected() )
  {
    WDG_RST;
    if ( millis()>ctime ) {  // if no answer received within the prescribed time
#if _DEBUG_>0
      Serial.println(F("Client reply timed out, giving up."));
#endif
      time_client.stop();
      return;
    }
    while ( time_client.available() ) {
      // Serial.print(F("---> time client reply received."));
      // parse time information
      Time_ParseTCPData( time_client.read() );
    }
  }
  time_client.stop();
}
/****************************************************************************/
#endif
#ifdef TIME_SERVER_UDP
/****************************************************************************/
/****************************************************************************/
void SendNTPpacket(IPAddress address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
/****************************************************************************/
void AdjustDSTTime(void)
{
// DST time adjustment:
// In Europe DST starts on March's last Sunday at 02:00 AM and clocks sets one hour backward at 03:00 AM on October's last Sunday.
// first, check for DST start date
tmElements_t te;
time_t tNow = now();

te.Second = 0;
te.Minute = 0;
te.Hour = 2;  // DST starting hour
//te.Wday = ?;  // looking for Sunday
te.Day = 31;  // last day of March and October
te.Month = 3;  // DST starting month
te.Year = CalendarYrToTm( (int)year() );  // set to current year
// get UNIX time
time_t time = makeTime(te);
// get the day of week
breakTime(time, te);
// get the last Sunday
#define dstSt ( time - (uint32_t)SECS_PER_DAY * (te.Wday - dowSunday) )
if ( tNow<dstSt ) return;  // standard time, nothing to do
// now check for DST end date
te.Hour = 3;  // DST ending hour
//te.Wday = ?;  // looking for Sunday
te.Day = 31;  // last day of October
te.Month = 10;  // DST ending month
//te.Year = CalendarYrToTm( (int)year() );  // set to current year
// get UNIX time
time_t tim = makeTime(te);
// get the day of week
breakTime( tim, te);
// get the last Sunday
#define dstEnd ( tim - (uint32_t)SECS_PER_DAY * (te.Wday - dowSunday) )
if ( tNow>dstEnd ) return;  // standard time, nothing to do
// daylight savings time (DST)! so add one more hour to the current time
  adjustTime( SECS_PER_HOUR );
}
/****************************************************************************/
void Time_CheckUDP(void)
{
typedef struct {
  byte byte0;
  byte byte1;
  byte byte2;
  byte byte3;
} bytes4_t;  // 4 bytes in little endian order of uint32_t
typedef union {
  uint32_t secs;
  bytes4_t sbytes;
} unixTime_t;
  int size;
  byte timeout;
  const int timeZone = 1;     // Central European Time
  int localPort = 8888;       // local port to listen for UDP packets
  //char timeServer[] = "192.168.100.1"; // time.nist.gov NTP server
  IPAddress timeServer(192, 168, 100, 1);  // Fritz.Box from intranet
  Udp.begin(localPort);
//  delay(1000);
  for ( counter=0; counter<10; counter++)
  {
    while ( Udp.parsePacket() > 0 ) Udp.read(); // discard any previously received packets
    Serial.print(F("Transmitting NTP Request..."));
    SendNTPpacket(timeServer); // send an NTP packet to a time server
    timeout = 0;
    ctime = millis() + 1500;  // wait up to 1.5 seconds for the answer
    while ( (size=Udp.parsePacket())<NTP_PACKET_SIZE && timeout==0 ) {
      if ( ctime < millis() ) timeout = 1;
    }
    if ( timeout ) continue;  // try again
    // received at least 48 bytes
    Serial.print(F("received NTP time: "));
    Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
    // convert four bytes starting at location 40 to a long integer
#if 0
    uint32_t secsSince1900;
    secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
    secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
    secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
    secsSince1900 |= (unsigned long)packetBuffer[43];
    return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
#else
    unixTime_t unixTime;
    unixTime.sbytes.byte3 = packetBuffer[40];
    unixTime.sbytes.byte2 = packetBuffer[41];
    unixTime.sbytes.byte1 = packetBuffer[42];
    unixTime.sbytes.byte0 = packetBuffer[43];
    unixTime.secs = unixTime.secs - 2208988800UL + timeZone * SECS_PER_HOUR;
    Serial.println(unixTime.secs);
    setTime(unixTime.secs);
    AdjustDSTTime();
    break;
#endif
  }
  if ( timeout )  Serial.println("No NTP Response :-(");
  Udp.stop();
//  return 0; // return 0 if unable to get the time
}
#endif
/****************************************************************************/
/****************************************************************************/
void TimeClient_UpdateFileString(void)
{
    time_t t = now(); // store the current time in time variable t
    sprintf_P(time_str, PSTR("%02i:%02i:%02i"), hour(t), minute(t), second(t));
    sprintf_P(date_str, PSTR("%02i-%02i-%02i"), year(t)%100, month(t), day(t));
    sprintf_P(file_str, PSTR("%s.txt"), date_str);
#if _DEBUG_>0
    Serial.print(F("Current date and time: ")); Serial.print(date_str); Serial.print(' '); Serial.println(time_str);
#endif
}
/****************************************************************************/
/****************************************************************************/
void TimeClient_Ping(void)
{
#ifdef TIME_SERVER_UDP
  Time_CheckUDP();
#elif defined TIME_SERVER_TCP
  Time_CheckTCP();
#endif
  TimeClient_UpdateFileString();
}
/****************************************************************************/
/****************************************************************************/
byte Time_NewDay(void)
{
  return File_CheckMissingRecordFile();
}
