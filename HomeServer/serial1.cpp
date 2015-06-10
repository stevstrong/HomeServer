/*************************************************************************
Title:    Interrupt UART library with receive/transmit circular buffers
Hardware: ATmega128A - UART1
	
DESCRIPTION:
	An interrupt is generated when the UART has finished transmitting or
	receiving a byte. The interrupt handling routines use circular buffers
	for buffering received and transmitted data.

	The UART1_RX_BUFFER_SIZE and UART1_TX_BUFFER_SIZE variables define the 
	buffer size in bytes. Note that these variables must be a power of 2.

*************************************************************************/
#include <Arduino.h>
/*
//#define __AVR_ATmega128__

#include <inttypes.h>
#include "iom128.h"
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
*/
#include "serial1.h"
//#include "main.h"

/*****************************************************************
 *  constants and macros
 *****************************************************************/
#define __disable_interrupt()		cli()       
#define __enable_interrupt()		sei()       

#if ( UART1_RX_BUFFER_SIZE & UART1_RX_BUFFER_MASK )
	#error RX buffer size is not a power of 2
#endif
#if ( UART1_TX_BUFFER_SIZE & UART1_TX_BUFFER_MASK )
	#error TX buffer size is not a power of 2
#endif


/*****************************************************************
 *  module global variables
 *****************************************************************/
//static volatile uint8_t uart1_TxBuf[UART1_TX_BUFFER_SIZE];
//static volatile uint8_t uart1_RxBuf[UART1_RX_BUFFER_SIZE];
static volatile uint8_t uart1_Buf[UART1_BUFFER_SIZE];
#define  uart1_TxBuf  uart1_Buf
#define  uart1_RxBuf  uart1_Buf
//static volatile uint8_t uart1_TxHead;
//static volatile uint8_t uart1_TxTail;
//static volatile uint8_t uart1_RxHead;
//static volatile uint8_t uart1_RxTail;
static volatile uint8_t uart1_BufHead;
static volatile uint8_t uart1_BufTail;
#define uart1_TxHead  uart1_BufHead
#define uart1_TxTail  uart1_BufTail
#define uart1_RxHead  uart1_BufHead
#define uart1_RxTail  uart1_BufTail

/*************************************************************************
Function: UART Receive Complete interrupt
Purpose:  called when the UART has received a character
**************************************************************************/
ISR(USART1_RX_vect)
{
	// calculate buffer index
	uint8_t tmphead = (uart1_RxHead + 1) & UART1_RX_BUFFER_MASK;

	if ( tmphead != uart1_RxTail ) {
		// store new index
		uart1_RxHead = tmphead;
		// store received data in buffer
		uart1_RxBuf[tmphead] = UDR1;
	}
}
/*************************************************************************
Function: UART Data Register Empty interrupt
Purpose:  called when the UART is ready to transmit the next byte
**************************************************************************/
ISR(USART1_UDRE_vect)
{
	uint8_t tmpTail = uart1_TxTail;
        if ( uart1_TxHead != tmpTail ) {
		// calculate and store new buffer index
		tmpTail = (tmpTail + 1) & UART1_TX_BUFFER_MASK;
		uart1_TxTail = tmpTail;  // store volatile var
		// get one byte from buffer and write it to UART1
		UDR1 = uart1_TxBuf[tmpTail];  // start transmission
	} else {
		// no more data to send, disable data register empty and Tx interrupt and enable only reception.
		// previous data still in shift register will still be sent out,
		// for transmission complete the bit TxC should be checked.
		UCSR1B = _BV(RXCIE1) | _BV(RXEN1);	// enable UART Rx and receive complete interrupt
	}
}
/*************************************************************************
Function: UART1_Flush()
Purpose:  Flush bytes waiting the receive buffer.  Acutally ignores them.
Input:    None
Returns:  None
**************************************************************************/
void UART1_Flush(void)
{
	uart1_TxHead = 0;
	uart1_TxTail = 0;
	uart1_RxHead = 0;
	uart1_RxTail = 0;
}
/*************************************************************************
Function: UART1_Init()
Purpose:  initialize UART and set baudrate
Input:    baudrate using macro UART1_BAUD_SELECT()
Returns:  none
**************************************************************************/
void UART1_Init(void)
{
	// set RS485 direction to Rx
//	RS485_SET_DIR_PIN_TO_OUTPUT;

	// disable UART and global interrupts
	UART1_DISABLE;
	__disable_interrupt();

	UBRR1H = 0;	// data register high is always 0

/*	if (sel = 1) {
		// settings for Vito
		UCSR1A = _BV(TXC1);  // clear Tx complete, x1 speed for easy setting
		UBRR1L = 207;	// baud rate 4800 at 16MHz, see Table 20-12 of data sheet page 200
		// set frame format: asynchronous, 8bit data, even parity, 2stop bits
		UCSR1C = _BV(UPM11) | _BV(USBS1) | _BV(UCSZ11) | _BV(UCSZ10);
	}
	else if (sel==2) {
*/		// settings for EnergyCam
		UCSR1A = _BV(TXC1) | _BV(U2X1);  // clear Tx complete, 2x speed for higher accuracy
		UBRR1L = 16;	// baud rate 115200 at 16MHz, see Table 20-12 of data sheet page 200
		// set frame format: asynchronous, 8bit data, even parity, 1stop bit
		UCSR1C = _BV(UPM11) | _BV(UCSZ11) | _BV(UCSZ10);
//	}

	// UART Tx and UDRIE interrupt will be enabled in UART_PutChar()
	// UART Rx will be enabled in Tx ISR() when there is no more data to send

	UART1_Flush();			// reset buffer pointers
	__enable_interrupt();
}
/*************************************************************************
Function: UART1_GetChar()
Purpose:  return byte from ringbuffer. Should be called only if UART1_RxDataAvailable()>0
Returns:  byte from ringbuffer
**************************************************************************/
uint8_t UART1_GetChar(void)
{
///**/ not needed, because the function will be only called when data is available
	while ( uart1_RxHead == uart1_RxTail );

	uint8_t tmptail = (uart1_RxTail + 1) & UART1_RX_BUFFER_MASK;	// calculate Rx buffer index
	uart1_RxTail = tmptail;			// update Rx tail pointer

	return uart1_RxBuf[tmptail];	// return data from receive buffer
}
/*************************************************************************
Function: UART1_GetChars()
Purpose:  return bytes from ringbuffer  
Parameters: - pointer to buffer where to store the chars
            - number of chars to get
Returns:
**************************************************************************/
void UART1_GetChars( uint8_t *buf, uint8_t len )
{
	while (len--) {
		while ( UART1_RxDataAvailable()==0 );
				//DoMainTasks();	//	sleep_mode();
		*buf++ = UART1_GetChar();
	}
}

/*************************************************************************
Function: UART1_PutChar()
Purpose:  write character to ringbuffer for transmitting via UART
Input:    byte to be transmitted
Returns:  none
**************************************************************************/
void UART1_PutChar(uint8_t data)
{
	uint8_t tmphead = (uart1_TxHead + 1) & UART1_TX_BUFFER_MASK;

	while ( tmphead == uart1_TxTail );	// wait for free space in buffer
		//DoMainTasks();

	uart1_TxHead = tmphead;		// update Tx head pointer
	uart1_TxBuf[tmphead] = data;	// store Tx data

//	UCSR1B = _BV(RXCIE1) | _BV(RXEN1);	// enable UART Rx and receive complete interrupt
	UCSR1B = _BV(UDRIE1) | _BV(TXEN1);	// enable UDRE and Tx interrupt
}

/*************************************************************************
Function: UART1_PutBytes()
Purpose:  transmit string to UART
Input:    string to be transmitted
Returns:  none          
**************************************************************************/
void UART1_PutChars(uint8_t * s, uint8_t len)
{
	while (len--)
		UART1_PutChar(*s++);
}

/*************************************************************************
Function: UART1_PutString()
Purpose:  transmit string to UART
Input:    string to be transmitted
Returns:  none          
**************************************************************************/
#if 0	// unused
void UART1_PutString(uint8_t *s)
{
	while (*s)
		UART1_PutChar(*s++);
}
/*************************************************************************
Function: UART1_PutString_p()
Purpose:  transmit string from program memory to UART
Input:    program memory string to be transmitted
Returns:  none
**************************************************************************/
void UART1_PutString_p(const uint8_t *progmem_s )
{
	uint8_t c;
	while ( (c=pgm_read_byte(progmem_s++))>0 )
		UART1_PutChar(c);
}
#endif

/*************************************************************************
Function: UART1_RxDataAvailable()
Purpose:  Determine the number of bytes waiting in the receive buffer
Input:    None
Returns:  Integer number of bytes in the receive buffer
**************************************************************************/
uint8_t UART1_RxDataAvailable(void)
{
	return (uint8_t)(uart1_RxHead - uart1_RxTail) & UART1_RX_BUFFER_MASK;
}
/*************************************************************************
Function: UART1_TxDataAvailable()
Purpose:  Returns the number of bytes waiting in the TX buffer.
**************************************************************************/
uint8_t UART1_TxDataAvailable(void)
{
	return (uint8_t)(uart1_TxHead - uart1_TxTail) & UART1_TX_BUFFER_MASK;
}
/**************************************************************************/
/**************************************************************************/
uint8_t UART1_TxComplete(void)
{
	// wait for previous Tx complete
	return ( UCSR1A & _BV(TXC1) );
}


