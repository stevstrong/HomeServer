#ifndef _SYS_CFG_H_
#define _SYS_CFG_H_


#define _DEBUG_	1 // set to 0 for release version, 1 for basic output, 2 for detailed output

#define WDG_RST  asm("wdr")

#define USE_RS485

#ifdef USE_RS485
	#define EC_ID	1
	#define VITO_ID	2
	#define RS485_SET_DIR_TO_OUTPUT	( DDRD |= _BV(DDRD4) )		// PD4 as output
	#define RS485_ENABLE_TX				( PORTD |= _BV(PORTD4) )	// set PD4
	#define RS485_ENABLE_RX				( PORTD &= ~_BV(PORTD4) )	// clear PD4
#endif

typedef unsigned char uint8_t;
typedef unsigned int uint16_t;

#endif

