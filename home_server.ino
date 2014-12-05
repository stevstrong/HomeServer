#include <SPI.h>
#include <UIPEthernet.h>

// Ethernet
static byte mymac[] = { 0x10,0x69,0x69,0x2D,0x30,0x31 };
//static byte myip[] = {192,168,100,57};
static byte host_ip[] = {192,168,100,40};

IPAddress myip(192,168,100, 57);
IPAddress gateway(192,168,100, 1);
IPAddress subnet(255, 255, 0, 0);
#define timeServer gateway

EthernetServer my_server(80);
boolean gotAMessage = false; // whether or not you got a message from the client yet
// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 23 is default for telnet;
// if you're using Processing's ChatServer, use port 10002):
EthernetClient my_client;

// NTP conversion
#define SECONDS_IN_DAY 86400
#define START_YEAR 1900
#define TIME_ZONE +1
#define NTP_PACKET_SIZE 48 // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
// A UDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

static unsigned long time;
char date_str[9]; // 8 chars + ending 0
char time_str[9]; // 8 chars + ending 0
char file_str[13]; // 12 chars + ending 0
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void setup()
{
  // Open serial communications and wait for port to open:
  Serial.begin(57600);
  time = millis();
  // start the Ethernet connection:
  Serial.print(F("\n\nTrying to get an IP address using DHCP ... "));
  if (Ethernet.begin(mymac) == 0) {
    Serial.print(F("failed! Setting static IP address ... "));
    // initialize the ethernet device not using DHCP:
    Ethernet.begin(mymac, myip, gateway, subnet);
  }
  Serial.println(F("done."));
  // start listening for clients
  my_server.begin();
  // print your local IP address:
  Serial.print(F("My IP address: "));
  Serial.println(Ethernet.localIP());
  ConnectToHost();
}
/////////////////////////////////////////////////////////////////
void ConnectToHost(void)
{
  static int repl = 0;
  Serial.print("connecting to host ... ");
  // if you get a connection, report back via serial:
  while ( repl==0 )
  repl = my_client.connect(host_ip, 8088);
  if ( repl>0)
    Serial.println("done.");
  else {
    // if you didn't get a connection to the server:
    Serial.print(F("failed: "));
    if ( repl==-1 ) Serial.println(F("timed out."));
    else if ( repl==-2 ) Serial.println(F("invalid server."));
    else if ( repl==-3 ) Serial.println(F("truncated."));
    else if ( repl==-4 ) Serial.println(F("invalid response"));
    else Serial.println(repl);
  }
}
///////////////////////////////////////////////////////////////////////////////
// send an NTP request to the time server at the given address
///////////////////////////////////////////////////////////////////////////////
unsigned long sendNTPpacket(IPAddress& address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011; // LI, Version, Mode
  packetBuffer[1] = 0; // Stratum, or type of clock
  packetBuffer[2] = 6; // Polling Interval
  packetBuffer[3] = 0xEC; // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now you can send a packet requesting a timestamp:
  
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer,NTP_PACKET_SIZE);
  Udp.endPacket();
}
///////////////////////////////////////////////////////////////////
boolean isLeapYear(unsigned int year) { return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)); }
///////////////////////////////////////////////////////////////////
void PrintDateTime(uint32_t timeStamp)
{
  timeStamp += 3600 * TIME_ZONE;
  static int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  static unsigned int year;
  static byte month, day;
  static uint32_t seconds;
  year = START_YEAR;
  while(1) {
    if( isLeapYear(year) ) seconds = SECONDS_IN_DAY * 366;
    else seconds = SECONDS_IN_DAY * 365;
    if( timeStamp >= seconds ) {
      timeStamp -= seconds;
      year++;
    } else break;
  }
  month = 0;
  while(1) {
    seconds = SECONDS_IN_DAY * days_in_month[month];
    if(isLeapYear(year) && month == 1) seconds = SECONDS_IN_DAY * 29;
    if(timeStamp >= seconds) {
      timeStamp -= seconds;
      month++;
    } else break;
  }
  month++;
  day = 1;
  while(1) {
    if(timeStamp >= SECONDS_IN_DAY) {
      timeStamp -= SECONDS_IN_DAY;
      day++;
    } else break;
  }
  unsigned int hour = timeStamp / 3600;
  unsigned int minute = (timeStamp - (uint32_t)hour * 3600) / 60;
  unsigned int second = (timeStamp - (uint32_t)hour * 3600) - minute * 60;
  sprintf(date_str, "%4u%2u%2u", year, month, day);
  strcpy(file_str, date_str);
  strcpy_P(&file_str[9], PSTR(".log"));
  //Serial.println(date_str);
  // print result
  Serial.print(F("Current date and time: "));
  Serial.print(year);
  Serial.print("/");
  if(month < 10) Serial.print("0");
  Serial.print(month);
  Serial.print("/");
  if(day < 10) Serial.print("0");
  Serial.print(day);
  Serial.print(", ");
  if(hour < 10) Serial.print("0");
  Serial.print(hour);
  Serial.print(":");
  if(minute < 10) Serial.print("0");
  Serial.print(minute);
  Serial.print(":");
  if(second < 10) Serial.print("0");
  Serial.println(second);
  // Serial.println();
}
///////////////////////////////////////////////////////////////////////////
void GetTime(void)
{
  unsigned int lPort = 8888;
  Udp.begin(lPort);
  delay(100);
  sendNTPpacket(timeServer); // send an NTP packet to a time server
  // wait to see if a reply is available
  //delay(1000);
  
  while ( !Udp.parsePacket() ) delay(10);
  // We've received a packet, read the data from it
  Udp.read(packetBuffer,NTP_PACKET_SIZE); // read the packet into the buffer
  Udp.stop();
  //the timestamp starts at byte 40 of the received packet and is four bytes, or two words, long. First, esxtract the two words:
  unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
  unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
  
  // combine the four bytes (two words) into a long integer
  // this is NTP time (seconds since Jan 1 1900):
  unsigned long secsSince1900 = highWord << 16 | lowWord;
  PrintDateTime(secsSince1900);
}
///////////////////////////////////////////////////////////////////
EthernetClient s_client;
///////////////////////////////////////////////////////////////////
void loop()
{
  // wait for a new client:
  // when the client sends the first byte, say hello:
  s_client = my_server.available();
  if ( s_client )
  {
  Serial.println(F("*************************************"));
  while ( s_client.connected() ) {
    while ( s_client.available() ) {
    char chr = s_client.read();
    Serial.print(chr);
    }
    Serial.println(F("*************************************"));
    // echo the bytes back to the client:
    //my_server.write(thisChar);
    // echo the bytes to the server as well:
    //Serial.println(F("Answering to client:"));
    if (!gotAMessage) {
      //my_server.println(F("HTTP/1.0 404 Not Found\r\n\r\nNot Found"));
      s_client.println(F("HTTP/1.1 200 OK"));
      s_client.println(F("Content-Type: text/html"));
      s_client.println(F("Connection: close")); // the connection will be closed after completion of the response
      s_client.println();
      s_client.println(F("Hello, client!"));
      gotAMessage = true;
      Serial.println(F("Sent long reply."));
    } else {
      Serial.println(F("Sent short reply."));
      s_client.println("OK");
    }
    break;
  }
  s_client.stop();
  Serial.println(F("-------------------------------------"));
  }

  // do here client tasks, send command and listen to answer
  if ( my_client.available() ) {
    while ( my_client.available() ) {
      byte data = my_client.read();
      if (data Serial.print(data, HEX);
      delay(5);
    }
    Serial.println();
  }
  // intervall check
  if ( millis()>(time+10000) ) {
    time = millis(); // update time reference
    // call here the ocasional functions
    GetTime();
  }
}
///////////////////////////////////////////////////////////////////////
