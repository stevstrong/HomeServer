#ifndef _SYS_CFG_H_
#define _SYS_CFG_H_


#define _DEBUG_	1 // set to 0 for release version, 1 for basic output, 2 for detailed output

#define WDG_RST  asm("wdr")

#define RS485_SET_DIR_TO_OUTPUT	asm("sbi 0x17,6")	// PB6
#define RS485_ENABLE_TX		asm("sbi 0x18,6")	// PB6
#define RS485_ENABLE_RX		asm("cbi 0x18,6")	// PB6

typedef unsigned char uint8_t;
typedef unsigned int uint16_t;

#endif


