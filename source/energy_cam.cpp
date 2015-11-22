/******************************************************************************
	EnergyCam
****************************************
Supported access types			ID
----------------------------------------
Input register		Read multiple	0x04
Holding register	Read multiple	0x03
						Write single	0x06
						Write multiple	0x10

List of input registers			Address
----------------------------------------
Time (UNIX)   (hi & lo)			0x09 - 0x0A
Test register (hi & lo)			0x16 - 0x17
Status register					0x1F
Result status OCR					0x21
Result value OCR (5 chars)		0x24 - 0x29
Result value OCR (chr frac)	0x2A
Result value OCR (long)			0x43
Result value OCR (int frac)	0x45

List of holding registers		Address
----------------------------------------
Time (UNIX)    WO (hi & lo)	0x00 - 0x01
Test register  RW (hi & lo)	0x09 - 0x0A	(default: 0xFA51 - 0xFFDD)
Test register  RO (hi & lo)	0x07 - 0x08	(default: 0xDEAD - 0xBEEF)
Test register  RW (hi & lo)	0x09 - 0x0A	(default: 0xFA51 - 0xFFDD)
Start OCR      WO					0x21		(write "1")
Power Down     WO					0x24		(write "1")
******************************************************************************/
#include "energy_cam.h"
#ifndef USE_RS485
 #include <Ethernet.h>
#endif
#include "file_client.h"
#include "vito.h"

#define EC_INPUT_REG_READ_MULTIPLE		0x04
#define EC_HOLDING_REG_READ_MULTIPLE	0x03
#define EC_HOLDING_REG_WRITE_SINGLE		0x06
#define EC_HOLDING_REG_WRITE_MULTIPLE	0x10

//#define	EC_STATUS_ACTION_ONGOING	0xFFFD
#define	EC_RESULT_OCR_OK1	1
#define	EC_RESULT_OCR_OK3	3

typedef struct {
	uint32_t value0; // first reading of the day, serves as reference for diff
	uint32_t value;  // current (last) reading
	uint8_t diff;    // relative value compared to the first reading of the day
} ocrReading_t;

static ocrReading_t ocrReading;

// EnergyCam state machine states
typedef enum { EC_OK, CONNECTION_TIMEOUT, MODBUS_ERROR, OCR_TIMEOUT } ec_state_t;
ec_state_t ec_state;
// modbus state machine states
typedef enum { MB_OK, RECEIVING_DATA, WRONG_ID, WRONG_FUNCTION, WRONG_DATA_LEN, WRONG_CRC, BUFFER_OVERFLOW, WRONG_FRAME_LEN, REPLY_TIMEOUT } mb_state_t;
mb_state_t mb_state;

#define MODBUS_EC_ADDRESS	1

////////// Variable definitions //////////
typedef struct {
	unsigned char id;
	unsigned char function;
	unsigned int address;
	unsigned int regs;
} modbus_head_t;

const modbus_head_t ec_frames[] PROGMEM = {
// all integer values are byte-swapped because of storing in little-endianess
{MODBUS_EC_ADDRESS, EC_INPUT_REG_READ_MULTIPLE,  0x1f00, 0x0100},	// read status register
{MODBUS_EC_ADDRESS, EC_HOLDING_REG_WRITE_SINGLE, 0x2100, 0x0100},	// start OCR                  1 6 0 0x21 0 1 0x18 0
{MODBUS_EC_ADDRESS, EC_INPUT_REG_READ_MULTIPLE,  0x2100, 0x0100},	// read OCR status
{MODBUS_EC_ADDRESS, EC_INPUT_REG_READ_MULTIPLE,  0x4300, 0x0300},	// read OCR result, 2 words:  01 04 00 0x43 00 02 0x80 0x1F
};
enum { EC_FRAME_READ_STATUS_REG, EC_FRAME_START_OCR, EC_FRAME_READ_OCR_STATUS, EC_FRAME_READ_OCR_RESULT };  // frame_index;

#define BUFFER_SIZE 16
// frame[] is used to receive and transmit packages. 
// The maximum number of bytes in a modbus packet is 256 bytes
// This is limited to the serial buffer of 64 bytes
static uint8_t frame[BUFFER_SIZE]; 
static uint8_t buffer;
static uint32_t timeout; // timeout interval
static uint32_t startDelay; // init variable for frame and timeout delay
// Modbus states that a baud rate higher than 19200 must use a fixed 750 us 
// for inter character time out and 1.75 ms for a frame delay	
#define T1_5   750 // inter character time out in microseconds
#define T3_5  1750 // inter frame time out in microseconds
static byte ec_new_day;

////////// Function definitions //////////
//void idle(void);
//void constructPacket(void);
//unsigned char construct_F16(void);
void Modbus_WaitForReply(void);
//void WaitForReply(void);
void ProcessReply(void);
void ProcessError(mb_state_t err_code);
void ProcessSuccess(void);
void CalculateCRC(unsigned char bufferSize);
void SendFrame(unsigned char fr_nr);

#ifdef USE_RS485
	#define ec_client	Serial1
#else
// Initialize the Ethernet client library with the IP address and port of the server
// that you want to connect to (port 23 is default for telnet);
EthernetClient ec_client;
#endif

/*****************************************************************************/
/*****************************************************************************/
void EC_NewDay(void)
{
  ec_new_day = 1;
}
/*****************************************************************************/
void EC_Connect(void)
{
#ifdef USE_RS485
 #if _DEBUG_>0
	Serial.println(F("Connected to EnergyCam via RS485."));
 #endif
	RS485_ENABLE_TX;
	ec_state = EC_OK;
#else

#define EC_CONNECT_TIMEOUT	5000	// millis
	int reply;
	byte ec_ip[] = {192,168,100,48};
	int ec_port = 8088;
//  int8_t repl = 0;
#if _DEBUG_>0
	Serial.print(F("Connecting to EnergyCam ... "));
#endif
	// if you get a connection, report back via serial:
	timeout = millis();  // time within to get reply
	while (1) {
		if ( (millis()-timeout)>EC_CONNECT_TIMEOUT ) {  // if no answer received within the prescribed time
			//time_client.stop();
#if _DEBUG_>0
			Serial.print(F("timed out..."));
#endif
			break;
		}
		WDG_RST;
		reply = ec_client.connect(ec_ip, ec_port);
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
		ec_state = CONNECTION_TIMEOUT;  // to avoid error code 0
	} else {
    // connection was successful.
#if _DEBUG_>0
		Serial.println(F("done."));
#endif
		ec_state = EC_OK;
	}
	
#endif	// #ifdef USE_RS485
}
/*****************************************************************************/
void Modbus_Init(void)
{
	mb_state = MB_OK;
	frame[0] = 0;
}
/*****************************************************************************/
void EC_Init(void)
{
#ifdef USE_RS485
	Serial1.begin(115200);
	RS485_SET_DIR_TO_OUTPUT;
	RS485_ENABLE_TX;
#endif
	Modbus_Init();
	ec_state = EC_OK;
	ec_new_day = 0;
	ocrReading.value0 = 0;
	ocrReading.value = 0;
	ocrReading.diff = 0;
	startDelay = millis();
}
/*****************************************************************************/
void SendFrame(uint8_t fr_nr)
{
	// init frame to send
	memcpy_P(&frame[0], &ec_frames[fr_nr], sizeof(modbus_head_t));
	CalculateCRC(6);  // add crc bytes to the end of frame starting form byte position 6
#if _DEBUG_>1
	Serial.print(F("Sending EnergyCam frame:"));
	for (byte i=0; i<8; i++) {
		Serial.print(' ');
		byte tmp = frame[i];
		if ( tmp<16 ) Serial.print(0);
		Serial.print(tmp, HEX);
	}
	Serial.println();
#endif
	while ( (millis()-startDelay)<2 ) WDG_RST;	// wait for frame delay
	// prepare to send
#ifdef USE_RS485
	RS485_ENABLE_TX;
	delayMicroseconds(100);
	// write RS485 header
	Serial1.write(EC_ID);
	Serial1.write(0xFF^EC_ID);
	Serial1.write(frame, 8);
	// prepare to receive the answer
	Serial1.flush();	// wait till all data was sent
	delayMicroseconds(300);
	RS485_ENABLE_RX;
#else
	ec_client.write((byte)0);	// wake-up from sleep mode
	delay(2);	// frame delay
	//frameSize = 8;
	ec_client.write(frame, 8);
	// wait till last byte was sent
	//	delayMicroseconds(frameDelay);
	delay(2);	// frame delay
#endif
	Modbus_WaitForReply();	// wait one second long till complete reply frame is received
//	return error_code;
}
/*****************************************************************************/
extern char param_readings[];
/*****************************************************************************/
uint8_t EC_Error(void)
{
#ifndef USE_RS485
	ec_client.stop();
#endif
	// add the two error code to the param_readings
	//sprintf(&param_readings[strlen(param_readings)], ",%u,%u", ec_state, mb_state);
	sprintf_P(&param_readings[strlen(param_readings)], PSTR(",,"));  // add blank values - avoid distorted plotting
	return ec_state;
}
/*****************************************************************************/
void EC_ReadOCRStatus(void)
{
#define EC_STATUS_TIMEOUT	10000	// millis
	timeout = millis();	// up to 10 seconds to wait till completion
	while (1) {
		WDG_RST;
		SendFrame(EC_FRAME_READ_OCR_STATUS);
		if ( mb_state==MB_OK ) {
			if ( frame[4]==EC_RESULT_OCR_OK1 || frame[4]==EC_RESULT_OCR_OK3 )
			break;
		}
  		if ( (millis()-timeout)>EC_STATUS_TIMEOUT ) {
  			ec_state = OCR_TIMEOUT;
  			break;
  		}
	}
}
/*****************************************************************************/
void EC_ReadOCRResult(void)
{
//	EC_ReadOCRStatus();
//	if ( ec_state>EC_OK )	EC_Error();
	uint8_t retry = 0;
	while ( (retry++)<20 ) {
		SendFrame(EC_FRAME_READ_OCR_RESULT);
		if ( ec_state==EC_OK)	break;
		delay(10);
	}
}
/*****************************************************************************/
void EC_StoreResult(void)
{
#ifndef USE_RS485
	ec_client.stop();  // close client conection
#endif
	// swap bytes because of endianess
	byte tmp = frame[3];
	frame[3] = frame[6];
	frame[6] = tmp;
	tmp = frame[4];
	frame[4] = frame[5];
	frame[5] = tmp;
	//    long temp = *(long *)&frame[3];//)*10 + frame[6];
	ocrReading.value = (*(uint32_t *)&frame[3])*10 + frame[8];
	// load parameter name to read from record
	strcpy_P(param_name, PSTR("Strom-H_abs"));
	// feasibility tests
	if ( ocrReading.value<256 ) {
		// read last value from the log file
		char * ptr = File_GetRecordedParameter(-1);  // last line, the 8-th parameter
		if ( ptr>0 )
			ocrReading.value = atol(ptr);
	}
	if ( ec_new_day ) {
		ec_new_day = 0;
		ocrReading.value0 = ocrReading.value;
	}
	// take the first recorded value of the day from the record file as reference
	char * ptr = File_GetRecordedParameter(1);  // first line, the 8-th parameter
	if ( ptr>0 )
		ocrReading.value0 = atol(ptr);

	if ( ocrReading.value0<256 )
		ocrReading.value0 = ocrReading.value;
	// get diff
	ocrReading.diff = ocrReading.value - ocrReading.value0;
	// add OCR result to the param_readings
	sprintf_P(&param_readings[strlen(param_readings)], PSTR(",%lu"), ocrReading.value);
	sprintf_P(&param_readings[strlen(param_readings)], PSTR(",%u"), ocrReading.diff);
#if _DEBUG_>0
	Serial.print(F("OCR value: "));
	Serial.print(ocrReading.value);
	Serial.print(F(", OCR value0: "));
	Serial.print(ocrReading.value0);
	Serial.print(F(", OCR diff value: "));
	Serial.println(ocrReading.diff);
#endif  
}
/*****************************************************************************/
uint8_t EC_ReadValue(void)
{
	EC_Connect();
	if ( ec_state>EC_OK )	return EC_Error();
	EC_ReadOCRResult();
	if ( ec_state>EC_OK )	return EC_Error();
	EC_StoreResult();
}
/*****************************************************************************/
// get the serial data from the buffer
/*****************************************************************************/
void Modbus_WaitForReply(void)
{
#define MODBUS_TIMEOUT	1000	// millis
	mb_state = RECEIVING_DATA;
	buffer = 0;
	timeout = millis();	// wait up to one second for reply
//	if ( (*ModbusPort).available() ) // is there something to check?
	while ( (millis()-timeout)<MODBUS_TIMEOUT && mb_state==RECEIVING_DATA )
	{
		while ( ec_client.available() )
		{
			WDG_RST;  // avoid reset
			// The maximum number of bytes is limited to the serial buffer size of BUFFER_SIZE.
			// If more bytes is received than BUFFER_SIZE, the overflow flag will be set and
			// the serial buffer will be flushed as long as the slave is still responding.
			uint8_t mb_data = ec_client.read();
			//Serial.println(mb_data,HEX);	// debug
			if ( mb_state==RECEIVING_DATA )	{
				if ( buffer==0 && mb_data==0 )	continue;	// workaround for leading '0'
				// read and check data byte
				if ( buffer>=BUFFER_SIZE )
					ProcessError(BUFFER_OVERFLOW);	// buffer size overflow
				else if ( buffer==0 && mb_data!=frame[0] )
					ProcessError(WRONG_ID);	// wrong slave ID
				else if ( buffer==1 && (mb_data!=frame[1] || (0x80&frame[1])) )
					ProcessError(WRONG_FUNCTION);	// wrong function code
				else if ( buffer==2 && mb_data!=2*frame[5] )
					ProcessError(WRONG_DATA_LEN);	// wrong data lenght
			}
			if ( mb_state==RECEIVING_DATA )	// store data byte only if no error detected so far
			{
				frame[buffer++] = mb_data;  // store data and increment index
				 // check frame length and CRC
				if ( buffer>2 && (frame[2]+5)==buffer )	// last byte of frame received
				{
					*(uint16_t *)&frame[buffer] = *(uint16_t *)&frame[buffer-2];	// save received CRC
					CalculateCRC(buffer-2);	// overwrite received CRC with calculated value in the frame buffer
					if ( *(uint16_t *)&frame[buffer] != *(uint16_t *)&frame[buffer-2] )
						ProcessError(WRONG_CRC);	// wrong CRCs
					else
						ProcessSuccess();	// complete and correct reply frame received
				}
			}
			// insert inter character time out if no new data received yet
			if ( !ec_client.available() )
				delayMicroseconds(T1_5);
		}

		////////// END OF FRAME RECPTION //////////
		WDG_RST;  // avoid reset

		// The minimum buffer size from a slave can be an exception response of 5 bytes.
		// If the buffer was partially filled set a frame_error.
		if ( buffer>0 )
			ProcessError(WRONG_FRAME_LEN);	// too few bytes received
	}
	WDG_RST;  // avoid reset
	if ( buffer==0 )
		ProcessError(REPLY_TIMEOUT);	// timeout error, no data received
	startDelay = millis(); // starting delay
}
/*****************************************************************************/
void ProcessError(mb_state_t err_code)
{
	if ( mb_state!=RECEIVING_DATA ) return;  // preserve previous error
	mb_state = err_code;
	ec_state = MODBUS_ERROR;
#if _DEBUG_>0
	Serial.print(F("Modbus error: "));
	Serial.println(err_code);
#endif
}
/*****************************************************************************/
void ProcessSuccess()
{
	mb_state = MB_OK;
	ec_state = EC_OK;
#if _DEBUG_>1
	Serial.print(F("Modbus received: "));
	for (byte i=0; i<10; i++) {
		byte tmp = frame[i];
		if ( tmp<16 ) Serial.print(0);
		Serial.print(tmp, HEX);
	}
	Serial.println();
#endif
}
/*****************************************************************************/
void CalculateCRC(uint8_t bufferSize) 
{
	uint16_t crc;
	crc = 0xFFFF;
   byte i;
	for (i = 0; i < bufferSize; i++) {
		crc ^= frame[i];
		for (uint8_t j = 0; j < 8; j++)
		{
			if ( crc & 0x0001 ) {
				crc >>= 1;
				crc ^= 0xA001;
			} else {
				crc >>= 1;
			}
		}
	}
  // crcLo byte should be sent first & crcHi byte is last.
  // this will just do the trick because of storing integers in little-endianess
  *(uint16_t *)&frame[i] = crc;
}
