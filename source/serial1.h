#ifndef UART1_H
#define UART1_H
/************************************************************************
Title:    Interrupt UART library with receive/transmit circular buffers
Hardware: any AVR with built-in UART
************************************************************************/

/** 
*  @brief Interrupt UART library using the built-in UART with transmit and receive circular buffers. 
 *  Based on Atmel Application Note AVR306
 *
 *  This library can be used to transmit and receive data through the built in UART. 
 *
 *  An interrupt is generated when the UART has finished transmitting or
 *  receiving a byte. The interrupt handling routines use circular buffers
 *  for buffering received and transmitted data.
 *
 *  The UART_RX_BUFFER_SIZE and UART_TX_BUFFER_SIZE constants define
 *  the size of the circular buffers in bytes. Note that these constants must be a power of 2.
 *  You may need to adapt this constants to your target and your application by adding 
 *  CDEFS += -DUART_RX_BUFFER_SIZE=nn -DUART_RX_BUFFER_SIZE=nn to your Makefile.
 */
 
#if (__GNUC__ * 100 + __GNUC_MINOR__) < 304
#error "This library requires AVR-GCC 3.4 or later, update to newer AVR-GCC compiler !"
#endif

/*
** constants and macros
*/
#define UART1_BUFFER_SIZE 16
/** Size of the circular receive buffer, must be power of 2 */
#ifndef UART1_RX_BUFFER_SIZE
#define UART1_RX_BUFFER_SIZE UART1_BUFFER_SIZE
#endif
/** Size of the circular transmit buffer, must be power of 2 */
#ifndef UART1_TX_BUFFER_SIZE
#define UART1_TX_BUFFER_SIZE UART1_BUFFER_SIZE
#endif
/* size of RX/TX buffer mask */
#define UART1_RX_BUFFER_MASK ( UART1_RX_BUFFER_SIZE - 1)
#define UART1_TX_BUFFER_MASK ( UART1_TX_BUFFER_SIZE - 1)

/* test if the size of the circular buffers fits into SRAM */
#if ( (UART1_RX_BUFFER_SIZE+UART1_TX_BUFFER_SIZE) >= (RAMEND-0x60 ) )
#error "size of UART1_RX_BUFFER_SIZE + UART1_TX_BUFFER_SIZE larger than size of SRAM"
#endif

#define UART1_DISABLE	{ UCSR1B = 0; }
#define UART1_ENABLE_RX	{ UCSR1B = _BV(RXCIE1) | _BV(RXEN1); }
#define UART1_ENABLE_TX	{ UCSR1B = _BV(UDRIE1) | _BV(TXEN1); }
#define UART1_RST_TXC	{ UCSR1A |= _BV(TXC1); }
/*
** function prototypes
*/
/**
   @brief   Initialize UART and set baudrate 
   @param   baudrate Specify baudrate using macro UART_BAUD_SELECT()
   @return  none
*/
extern void UART1_Init(void);

/**
 *  @brief   Get received byte from ringbuffer
 *
 * Returns the received character
 *
 *  @param   void
 *  @return  received byte from ringbuffer
 */
extern uint8_t UART1_GetChar(void);
extern void UART1_GetChars(uint8_t *s, uint8_t len);

/**
 *  @brief   Put byte to ringbuffer for transmitting via UART
 *  @param   data byte to be transmitted
 *  @return  none
 */
extern void UART1_PutChar(uint8_t data);
extern void UART1_PutChars(uint8_t * s, uint8_t len);

/**
 *  @brief   Put string to ringbuffer for transmitting via UART
 *
 *  The string is buffered by the uart library in a circular buffer
 *  and one character at a time is transmitted to the UART using interrupts.
 *  Blocks if it can not write the whole string into the circular buffer.
 * 
 *  @param   s string to be transmitted
 *  @return  none
 */
extern void UART1_PutString(uint8_t *s);

/**
 * @brief    Put string from program memory to ringbuffer for transmitting via UART.
 *
 * The string is buffered by the uart library in a circular buffer
 * and one character at a time is transmitted to the UART using interrupts.
 * Blocks if it can not write the whole string into the circular buffer.
 *
 * @param    s program memory string to be transmitted
 * @return   none
 * @see      uart_puts_P
 */
extern void UART_PutString_p(const uint8_t *s );

/**
 * @brief    Macro to automatically put a string constant into program memory
 */
#define UART_Puts_P(__s)       UART_Puts_p(PSTR(__s))

/**
 *  @brief   Return number of bytes waiting in the receive buffer
 *  @param   none
 *  @return  bytes waiting in the receive buffer
 */
extern uint8_t UART1_RxDataAvailable(void);

/**
 *  @brief   Flush bytes waiting in Tx and Rx buffer
 *  @param   none
 *  @return  none
 */
extern void UART1_Flush(void);

/**@}*/


#endif // UART1_H


