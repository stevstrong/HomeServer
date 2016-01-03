/*************************************************************************/
/*
  STM32 based RS485 to UART bridge
  ================================
  The RS485 adapter is connected to UART1, the slave is on UART2.
   
 */
/*************************************************************************/
//#include <libmaple/usart.h>
//#define _DEBUG_

#define LED_PIN			PC13
#define RS485_DIR_PIN	PA8
#define SET_RS485_DIR_TX	digitalWrite(RS485_DIR_PIN,HIGH)
#define SET_RS485_DIR_RX	digitalWrite(RS485_DIR_PIN,LOW)

static byte buff1[20], buff2[20], buff3[40];
static byte len1, len2, len3, ocr_data;
static uint32_t time2;
bool ser3_send_to_ser1, test;
byte * tx1buf;
byte tx1len;

#define INTERVAL2 33000 // millis, 33 second between 2 consecutive OCR readings
/*************************************************************************/
void setup()
{
	// initialize LED pin.
	pinMode(LED_PIN, OUTPUT);
	digitalWrite(LED_PIN, HIGH);	// turn the LED off
	// initialize RS485 direction pins
	pinMode(RS485_DIR_PIN, OUTPUT);
	SET_RS485_DIR_RX;	// set RS485 to Rx
	// init serial ports
	Serial.begin(115200); // USB - used as debug monitor
	Serial1.begin(115200); // RS485
	Serial2.begin(115200, SERIAL_8E1);  // EnergyCam
	Serial3.begin(4800, SERIAL_8E2);  // Vito direct connection
	//Serial3.begin(115200);  // Vito + ATtiny4313 as Baudrate converter
#ifdef _DEBUG_
	delay(5000);  // wait for PC COMx port to be active, remove for release version !!!
	Serial.println(F("\n*** RS485-UART bridge ***\n"));
//  Serial.print(F("USART1->regs->CR1: ")); Serial.println(USART1->regs->CR1,HEX);
//  Serial.print(F("USART2->regs->CR1: ")); Serial.println(USART2->regs->CR1,HEX);
#endif
	time2 = 0;
	len1 = 0; len2 = 0; ocr_data = 0;
	ser3_send_to_ser1 = false;
//	test = true;
	test = false;
	tx1buf = 0;
	tx1len = 0;
}
/*************************************************************************/
void BlinkOnce()
{
	digitalWrite(LED_PIN, LOW);   // turn the LED on
	delay(100);                   // wait a bit
	digitalWrite(LED_PIN, HIGH);  // turn the LED off
}
/*************************************************************************/
void PrintOut(void)
{
#ifdef _DEBUG_
	if (len1>0) {
		Serial.print(F("Serial 1 received ")); Serial.print(len1); Serial.print(F(" bytes: "));
		byte indx = 0;
		for (byte i=0; i<len1; i++) {
			Serial.print(' ');
			byte c = buff1[indx++];
			if (c<16) Serial.print('0');
			Serial.print(c,HEX);
		}
		Serial.println();
		BlinkOnce();
	} else if (len2>0) {
		Serial.print(F("Serial 2 received ")); Serial.print(len2); Serial.print(F(" bytes: "));
		byte indx = 0;
		for (byte i=0; i<len2; i++) {
			Serial.print(' ');
			byte c = buff2[indx++];
			if (c<16) Serial.print('0');
			Serial.print(c,HEX);
		}
		Serial.println();
		BlinkOnce();
	} else if (len3>0) {
		Serial.print(F("Serial 3 received ")); Serial.print(len3); Serial.print(F(" bytes: "));
		byte indx = 0;
		for (byte i=0; i<len3; i++) {
			Serial.print(' ');
			byte c = buff3[indx++];
			if (c<16) Serial.print('0');
			Serial.print(c,HEX);
		}
		Serial.println();
		BlinkOnce();
	}
	// reply printout:
	if (tx1buf>0) {
		Serial.print(F("> replied ")); Serial.print(tx1len); Serial.print(F(" byte(s): "));
		byte indx = 0;
		for (byte i=0; i<tx1len; i++) {
			Serial.print(' ');
			byte c = tx1buf[indx++];
			if (c<16) Serial.print('0');
			Serial.print(c,HEX);
		}
		Serial.println();
		BlinkOnce();
		tx1buf = 0;
	}
#else
	BlinkOnce();
#endif
}
/*************************************************************************/
void SimulateVito(void)
{
// 01 04 06 00 01 0D 66 00 01 7E 20
byte vito[1] = {0x06};
	// write here the bytes from serial 2 (EneryCam) to serial 1
	if ( buff1[2]==0x16 ) {	// reset poll?
		TransmitSerial1(vito, 1);
	} else {
		TransmitSerial1(buff1+2, len1-2);
	}
}
/*************************************************************************/
void TransmitSerial1(byte * buf, byte len)
{
// printout:
	tx1buf = buf;
	tx1len = len;

	SET_RS485_DIR_TX;			// set RS485 dir to Tx
	delayMicroseconds(100);
	Serial1.write(buf, len);
	Serial1.flush();			// wait till all chars were sent - blocking!
	delayMicroseconds(300);	// extra waiting time for 100% empty output buffer
	SET_RS485_DIR_RX;			// set RS485 dir back to Rx
}
/*************************************************************************/
void TransmitOCRValue(void)
{
// 01 04 06 00 01 0D 66 00 01 7E 20 - example value
byte ocrVal[] = {0x01, 0x04, 0x06, 0x00, 0x01, 0x0D, 0x66, 0x00, 0x01, 0x7E, 0x20};
	if ( buff1[2]==0 ) return;	// do nothing for the wake-up '0' byte 
	// write here the bytes from serial 2 (EneryCam) to serial 1
	if ( test ) {
		TransmitSerial1(ocrVal, sizeof(ocrVal));
	} else if ( ocr_data>0 ) {	// already received any OCR data ?
		TransmitSerial1(buff2, ocr_data);
	}
}
/*************************************************************************/
void ReceiveSerial3(void)
{
#define SERIAL3_TIMEOUT 10000 // micros
	if (Serial3.available()) {
		len3 = 0;
		uint32_t t = micros();
		// receive all bytes having a gap size up to timeout
		while ( (micros()-t)<SERIAL3_TIMEOUT ) {
			if ( Serial3.available() ) {
				buff3[len3++] = Serial3.read();
				t = micros();	// reset timeout to receive next byte
			}
		}
	}
	if ( len3>0 ) {
		if ( ser3_send_to_ser1 ) {
			ser3_send_to_ser1 = false;
			// forward data to serial 1
			TransmitSerial1(buff3,len3);
		}
		PrintOut();	// debug output messages to USB interface
		len3 = 0;  // reset length to detect new reception
	}
}
/*************************************************************************/
void TransmitSerial3(void)
{
	// first, flush any received data from Vito
	while ( Serial3.available() ) Serial3.read();
	// then send received data from master (except the 2 header bytes)
	Serial3.write(buff1+2, len1-2);
	// activate direct forwarding of data from serial 3 to serial 1
	ser3_send_to_ser1 = true;
}
/*************************************************************************/
void ReceiveSerial1(void)
{
#define SERIAL1_TIMEOUT 1000 // micros
	if (Serial1.available()) {
		len1 = 0;
		uint32_t t = micros();
		// receive all bytes having a gap size up to timeout
		while ( (micros()-t)<SERIAL1_TIMEOUT ) {
			if ( Serial1.available() ) {
				byte c = Serial1.read();
				if ( len1==0 && c==0 )	continue;	// workaround for leading 0
				buff1[len1++] = c;
				t = micros();	// reset timeout to receive next byte
			}
		}
	}
	if ( len1>0 ) {
		// check the first bytes. They should be 0x01 or 0x02 followed by their XOR value
		if ( *(uint16_t*)&buff1[0]==0xFE01 ) {	// little endianess
			// send back the EnergyCam OCR value
			TransmitOCRValue();
		} else if ( *(uint16_t*)&buff1[0]==0xFD02 ) {
			if ( test ) {
				SimulateVito();
			} else {
				// forward bytes to Vito
				TransmitSerial3();
			}
		} else if (test) {
			//byte err[]={0x55, 0x55, 0x55};
			Serial.print(F("Serial 1 received ")); Serial.print(len1); Serial.print(F(" bytes: "));
			byte indx = 0;
			for (byte i=0; i<len1; i++) {
				Serial.print(' ');
				byte c = buff1[indx++];
				if (c<16) Serial.print('0');
				Serial.print(c,HEX);
			}
			Serial.println();
			TransmitSerial1(buff1, 4);
		}
		PrintOut();	// debug output messages to USB interface
		len1 = 0;  // reset length to detect new reception
	}
}
/*************************************************************************/
void TransmitSerial2(void)
{
// read OCR result, 2 words:  01 04 00 0x43 00 02 0x80 0x1F
//ocrResult[] = {MODBUS_EC_ADDRESS, EC_INPUT_REG_READ_MULTIPLE,  0x4300, 0x0300},
byte ocrResult[] = {0x01, 0x04, 0x00, 0x43, 0x00, 0x03, 0x41, 0xDf};
	Serial2.write(ocrResult, sizeof(ocrResult));
}
/*************************************************************************/
void ReceiveSerial2(void)
{
// 01 04 06 00 01 0D 66 00 01 7E 20
#define SERIAL2_TIMEOUT 1000 // micros
	if (Serial2.available()) {
		len2 = 0;
		uint32_t t = micros();
		// receive all bytes having a gap size up to timeout
		while ( (micros()-t)<SERIAL2_TIMEOUT ) {
			if ( Serial2.available() ) {
				buff2[len2++] = Serial2.read();
				t = micros();	// reset timeout to receive next byte
			}
		}
	}
	if ( len2>0 ) {
		ocr_data = len2;
		PrintOut();	// debug output messages to USB interface
		len2 = 0;  // reset length to detect new reception
	}
}
/*************************************************************************/
void loop()
{
	// read EC data each 20 seconds
	uint32_t t = millis();
	if ( (t-time2)>INTERVAL2 ) {
		time2 = t;
		TransmitSerial2();  // "read OCR data" command is sent
	}
	// check if command received from master
	ReceiveSerial1();
	// check for reply from EnergyCam
	ReceiveSerial2();
	// check for reply from Vito
	ReceiveSerial3();
}
