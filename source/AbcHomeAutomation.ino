
#include "sys_cfg.h"  // configure debug if necessary

#include <SPI.h>
#include <Ethernet.h>
#include <SdFat.h>
#include <Time.h>
#include "ether_server.h"
#include "time_client.h"
#include "file_client.h"
#include "serial1.h"
#include "vito_client.h"
#include "energy_cam.h"

// Pin mapping:
// ---------------------------
// Ethernet SPI CS: PB0
// Ethernet reset:  PE6
// SD-Card SPI CS:  PB4
// RS485 direction: PB6

//#define CS_ETHERNET  8  // default pin: SS
#define CS_SD_CARD 12  // PB4 on custom ATmega128

//byte wdg_counter = 0;
byte minute_old;
////////////////////////////////////////////////////////////////////////////////
void setup()
{
	// Open serial communications and wait for port to open:
#if _DEBUG_>0
	Serial.begin(115200);
	Serial.println(F("\n\n***** Home Automation application started *****\n"));
	WDG_EvaluateStatus();  // write LOG file
#endif
	Ethernet_Init();
	WDG_Init();
	EtherServer_Init();
	TimeClient_Init();
	FileClient_Init(CS_SD_CARD);
	UART1_Init();
	VitoClient_Init();
	EC_Init();
	minute_old = 60;
#if _DEBUG_>0
	Serial.println(F("setup end."));
#endif
}
/***********************************************************************************/
/***********************************************************************************/
#if _DEBUG_>0
void WDG_EvaluateStatus(void)
{ // reset status bits for ATmega128A
	if ( MCUCSR&JTRF )  Serial.println(F("WDG: JTAG reset occured!"));
	if ( MCUCSR&WDRF )  Serial.println(F("WDG: watchdog reset occured!"));
	if ( MCUCSR&BORF )  Serial.println(F("WDG: brown-out reset occured!"));
	if ( MCUCSR&EXTRF )  Serial.println(F("WDG: external reset occured!"));
	if ( MCUCSR&PORF )  Serial.println(F("WDG: power-on reset occured!"));
}
#endif
/***********************************************************************************/
/***********************************************************************************/
void WDG_Init(void)
{
	WDG_RST;
	// Start timed sequence
	WDTCR = _BV(WDCE) | _BV(WDE);
	// Set new prescaler(time-out) value = 2048K cycles (~2s)
	WDTCR = _BV(WDE) | _BV(WDP2) | _BV(WDP1) | _BV(WDP0);
	// clear previous status flags
	MCUCSR = 0;
}
/***********************************************************************************/
/***********************************************************************************/
void Ethernet_Init(void)
{
byte mymac[] = { 0x10,0x69,0x69,0x2D,0x30,0x40 };
IPAddress myip(192,168,100, 58);
IPAddress gateway(192,168,100, 1);
IPAddress subnet(255, 255, 255, 0);
//byte ETHERNET_RESET_PIN = 0;
#define ETHERNET_RESET_PIN_MODE_SET_TO_OUTPUT  asm("sbi 0x02, 6");  // set PE6 to output
#define ETHERNET_RESET_PIN_SET_TO_LOW  asm("cbi 0x03, 6")
#define ETHERNET_RESET_PIN_SET_TO_HIGH  asm("sbi 0x03, 6")
	// start the Ethernet connection:
#if _DEBUG_>0
	Serial.print(F("Getting IP address using DHCP ... "));
#endif
	// reset Ethernet interface
	ETHERNET_RESET_PIN_MODE_SET_TO_OUTPUT;  //      pinMode(ETHERNET_RESET_PIN, OUTPUT);
	ETHERNET_RESET_PIN_SET_TO_LOW;  //    digitalWrite(ETHERNET_RESET_PIN,LOW);
	delay(1);
	ETHERNET_RESET_PIN_SET_TO_HIGH;  //    digitalWrite(ETHERNET_RESET_PIN,HIGH);
	delay(100);

	SPI.setClockDivider(SPI_CLOCK_DIV2 );  // set SPI clock to maximum speed (8 MHz), still working OK with 3.3 V

	//  if ( Ethernet.begin(mymac, myip, gateway, subnet) == 0 ) {
	if ( Ethernet.begin(mymac)==0 ) {
#if _DEBUG_>0
		Serial.print(F("failed! Setting static IP address ... "));
#endif
		// initialize the ethernet device not using DHCP:
		Ethernet.begin(mymac, myip, gateway, subnet);
	}
#if _DEBUG_>0
	Serial.print(F("done."));
	// print your local IP address:
	Serial.print(F(" My IP address: "));
	Serial.println(Ethernet.localIP());
#endif
}
///////////////////////////////////////////////////////////////////
int freeRam (void)
{
	extern int __heap_start, *__brkval;
	int v;
	return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}
///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////
void loop()
{
	WDG_RST;
	// intervall check
	//time_t t = now();
	byte minute_now = minute();
	// check incoming serial character
#if _DEBUG_>0
	if ( Serial.available() )
	{
		// get the new byte:
		char inChar = Serial.read();
		//if (inChar == 'f')    File_PrintFile();  // display file content
		if (inChar == 't')	TimeClient_UpdateFileString();  // update date and time strings
		//if (inChar == 'n')    Vito_ClientNewDay();
		//if (inChar == 'v')    Vito_ClientSetVitoTime();
	}
#endif
	// execute next functions only once in each minute
	if ( minute_now!=minute_old )
	{
		TimeClient_UpdateFileString();  // update date and time
		// new day events
		if ( Time_NewDay() ) {
			File_NewDay();
			VitoClient_NewDay();
			EC_NewDay();
		}
		// read Vito parameters each even minute
		if ( (minute_now%2)==0 ) {
			VitoClient_ReadParameters();
			EC_ReadValue();
			File_WriteDataToFile();
			// control hot water
			VitoClient_CheckDHW();
		}
		// do time update each 5 minutes
		if ( (minute_now%5)==0 )  TimeClient_Ping();
	}
	// do here server client tasks, listen to requests and send reply
	EtherServer_CheckForClient();

	minute_old = minute_now;
	delay(20);
}


