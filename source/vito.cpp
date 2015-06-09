/*****************************************************************************/
// list of VITO data
/*****************************************************************************/
#include "sys_cfg.h"
#include "vito.h"

const char temp_aussen[] 		PROGMEM = "Temp-Aussen";
const char temp_luft_ein[]		PROGMEM = "Temp-Luft-Ein";
const char temp_luft_aus[]		PROGMEM = "Temp-Luft-Aus";
const char temp_hzg_vl[]		PROGMEM = "Temp-Hzg-VL";
const char temp_hzg_rl[]		PROGMEM = "Temp-Hzg-RL";
const char temp_ww[]			PROGMEM = "Temp-WW";
const char status_verdichter[]		PROGMEM = "Status-Verdichter";
const char status_pumpe_hzg[]		PROGMEM = "Status-Pumpe-Hzg";
const char status_pumpe_ww[]		PROGMEM = "Status-Pumpe-WW";
const char verdichter_betriebstd[]	PROGMEM = "Verdichter-BetriebStd";
const char verdichter_nrstarts[]	PROGMEM = "Verdichter-NrStarts";
const char strom_wp[]			PROGMEM = "Strom-WP";
const char strom_h[]			PROGMEM = "Strom-H";
const char strom_datum[]		PROGMEM = "Strom-Datum";
const char wp_uhrzeit[]			PROGMEM = "WP-Uhrzeit";
const char wp_betriebsart[]		PROGMEM = "Betriebsart";
/*
const char strom_datum[]		PROGMEM = "Strom-Datum";
const char strom_datum[]		PROGMEM = "Strom-Datum";
*/

const vito_param_t vito_params[] PROGMEM = {
//	'Geraet-Info'	{ 'addr' => 0x00F8, 'bytes' => 2, 'divider' =>  1, 'unp' => 's' },
{temp_aussen, 			0x0101, 2, 10, 'i'},
{temp_luft_ein,		   	0x0103, 2, 10, 'i'},
{temp_luft_aus,			0x0104, 2, 10, 'i'},
{temp_hzg_vl,			0x0105, 2, 10, 'i'},
{temp_hzg_rl,			0x0106, 2, 10, 'i'},
{temp_ww,			0x010D, 2, 10, 'i'},
{status_verdichter,		0x0400, 1,  1, 'b'},
{status_pumpe_hzg,		0x040D, 1,  1, 'b'},
{status_pumpe_ww,		0x0414, 1,  1, 'b'},
//
{verdichter_betriebstd,		0x0580, 4, 10, 'i'},
{verdichter_nrstarts,		0x0500, 4,  1, 'i'},
{wp_uhrzeit,			0x08E0, 8,  1, 's'},
{wp_betriebsart,		0xB000, 1,  1, 'i'},
//
{strom_wp,			0xFFFE, 2,  1, 'i'},
{strom_h,			0xFFFF, 2,  1, 'i'},
{strom_datum,			0xFFFF, 4,  1, 's'},
//
};

const char * const read_params[] PROGMEM = {
	temp_aussen, temp_hzg_vl, temp_hzg_rl, temp_ww,
	status_verdichter, status_pumpe_hzg, status_pumpe_ww,
//	strom_h, strom_wp
};

byte send_frame[100];	// reserve for output stream, must be long for string formatted data (ex. date)
byte rec_frame[100];	// reserve for input stream, must be long for string formatted data (ex. date)

char param_readings[PARAM_READINGS_SIZE];  // last read-out parameter values in string format
vito_param_t vito_param;  // currently accessed parameter

#define VITO_REC_START      0
#define VITO_REC_TRAIN_LEN  1

static int8_t rec_len, ind;
//byte crc;
uint8_t ParseWrValue(uint8_t in);//, void * val);
uint8_t Parse2Bytes(void);
uint8_t Parse4Bytes(void);
uint8_t Build2Bytes(void);//int16_t dt);
uint8_t Build4Bytes(void);//int32_t dt);
uint8_t BuildStringValue(void);//char * dt);

/*****************************************************************************/
/*****************************************************************************/
char * Vito_GetParamName(byte in)
{
  static char param_name[30];
	strcpy_P(param_name, (const char *)pgm_read_word(&vito_params[in].name));
  return param_name;
}
/*****************************************************************************/
/*****************************************************************************/
void Vito_GetParam(byte in)
{
  memcpy_P(&vito_param, &vito_params[in], sizeof(vito_param_t));
}
/*****************************************************************************/
/*****************************************************************************/
int8_t GetKeyIndex(const char * key)
{
  //check if key exists
  for (byte i=0; i<sizeof(vito_params)/sizeof(vito_param_t); i++) {
    if ( key==(const char *)pgm_read_word(&vito_params[i].name) )
      return i;
  }
//  Serial.print(F("ERROR: GetKeyIndex: parameter key ")); Serial.print((int)key,HEX); Serial.println(F(" not found!"));
  return -1;
}
/*****************************************************************************/
/*****************************************************************************/
void Vito_ReceiveInit(void)
{
	ind = 0;
	rec_len = 10;
}
/*****************************************************************************/
/*****************************************************************************/
byte CRC8(void)
{
	byte crc = 0;
	byte i = send_frame[1]+1;
//  Serial.print(F("CRC8: len is ")); Serial.println(i,HEX);
	while ( i>0 ) {
		crc += send_frame[i--];
	}
//  Serial.println(F("\nCRC8 end."));
	return ( crc );	// only LSB is needed
}
/*****************************************************************************/
/*****************************************************************************/
byte Vito_BuildCommand(unsigned char in, char cmd)//, void * value)
{
//Serial.println(F("Vito_BuildCommand started..."));
	// variable "in" points to the array index
	Vito_GetParam(in);
//	send_frame = {0x41, length, 0x00, command, adrr_high, addr_low, nr_bytes, CRC/data};	// CRC in read mode
	send_frame[0] = 0x41;
	send_frame[1] = 0x05;
	send_frame[2] = 0x00;
	
	if ( cmd == 'r') {
		// example: 41 05 00 01 00 F8 02 00
		send_frame[3] = 0x01;	// command "READ"
		send_frame[4] = vito_param.addr>>8;
		send_frame[5] = vito_param.addr%256;
		send_frame[6] = vito_param.len;
	}
	else if (cmd == 'w')
	{
		send_frame[1] += vito_param.len;	// adjust nr bytes to send
		send_frame[3] = 0x02;	// command "WRITE"
		send_frame[4] = vito_param.addr>>8;
		send_frame[5] = vito_param.addr%256;
		send_frame[6] = vito_param.len;
		if ( vito_param.typ!='s' ) ParseWrValue(in);//, value);	// insert here the parsed data to write
		//if (i==0) { Serial.println(F("Error: data cold not be parsed")); return 0;}
	}
	else
	{
#if _DEBUG_>0
        Serial.print("ERROR: Vito_SendCommand: undefined command "); Serial.println(cmd);
#endif
		return 0;
	}
	//$out = chr(0x41) . $send_frame . chr(CRC8($send_frame));	// add cmdID as first and crc as last element
	send_frame[send_frame[1]+2] = CRC8();	// add CRC as last element
#if _DEBUG_>1
//Serial.println(F("Vito_BuildCommand done."));
#endif
	return (send_frame[1]+3);	// number of bytes to send
}
/*****************************************************************************/
/*****************************************************************************/
byte Vito_ReceiveData(byte char0)
{
// example stream: 06 41 07 01 01 55 25 02 07 01 8D
//	static char char0;
	if ( ind==VITO_REC_START )	// is first byte?
	{	// check if the first byte is 06
		if ( char0==0x06 ) {	// only next byte should be stored
			return 0;		// get next byte
		}
		else if ( char0!=0x41 ) {
/*			Serial.print(F("WARNING: ReceiveData: unexpected first byte received: "));
                        if ( char0<16) Serial.print('0');
                        Serial.println(char0);
*/			return 0;	//continue;
		}
	} else if ( ind==VITO_REC_TRAIN_LEN )	// is second byte?
	{	// check size byte
		rec_len = char0+2;	// get remaining bytes to be received
		//echo "Data size is: $char0<br>";
	}

	rec_frame[ind++] = char0;	// store answer string here

//	if (i==0) { Serial.println(F("ERROR: ReceiveData: could not received first byte!")); return 0; }
	//$crc %= 256;
	if ( (--rec_len)>0 )  return 0;

	// check CRC error after last received byte
	//if ( crc!=char0 )	Serial.println(F("ERROR: Vito_ReceiveData: Got CRC error for the incoming string!"));
	Vito_ReceiveInit();  // end of reception, re-initialize status
	return 1;
}
/*****************************************************************************/
// return number of parsed bytes
/*****************************************************************************/
byte Vito_ParseRecData(void)
{
// example received stream: [06] 41 07 01 01 55 25 02 07 01 8D

	// check error byte
	if ( rec_frame[2]==0x03 ) {
#if _DEBUG_>1
          Serial.println(F("ERROR: Vito sent error!"));
#endif
          return 0;
        }
	// check CRC
	uint8_t len = rec_frame[1]+2;
	uint8_t crc = 0;
	for (uint8_t i=1; i<len; i++)
		crc += rec_frame[i];
	if ( rec_frame[len]!=crc ) {
#if _DEBUG_>1
          Serial.println(F("ERROR: frame CRC error!"));
#endif
          return 0;
        }
	// check received parameters with the requested ones
	// swap high-low bytes for address
	uint16_t ad = rec_frame[4];
	ad = ad<<8;
	ad += rec_frame[5];
//        Serial.print(F("parsed address: ")); Serial.println(ad, HEX);
	if ( ad!=vito_param.addr ) {
#if _DEBUG_>1
          Serial.println(F("ERROR: frame address mismatch!"));
#endif
          return 0;
        }
	if ( rec_frame[6]!=vito_param.len ) {
#if _DEBUG_>1
          Serial.println(F("ERROR: frame parameter length mismatch!"));
#endif
          return 0;
        }

	if ( vito_param.typ=='i' )	// integer
	{
		if ( vito_param.len==2 )	return Parse2Bytes();	// divide with the divider
		else if ( vito_param.len==4 )	return Parse4Bytes();	// divide with the divider
	}
	else if ( vito_param.typ=='b' )	// binary
	{
		if ( rec_frame[7]==0x00 )   rec_frame[0] = '0';
		else			    rec_frame[0] = '1';
		rec_frame[1] = 0;
//  Serial.print(F("parsed byte: ")); Serial.println((char*)rec_frame);
		return 1;
	}
	else if ( vito_param.typ=='s' )	// string
	{
//todo		return ParseString();
          return 1;
	}
#if _DEBUG_>1
	Serial.print(F("ERROR: parsed unknown format!"));
#endif
	return 0;
}
/*****************************************************************************/
/* example of byte swap
static inline uint16_t bswap_16_inline(uint16_t x)
{
        union {
                uint16_t x;
                struct {
                        uint8_t a;
                        uint8_t b;
                } s;
        } in, out;
        in.x = x;
        out.s.a = in.s.b;
        out.s.b = in.s.a;
        return out.x;
}*/
/*****************************************************************************/
/*****************************************************************************/
byte Parse2Bytes(void)
{	// AB -> BA
	byte sign;

	int16_t x = *(int16_t*)(rec_frame+7);
//	x = abs(*(int16_t*)(rec_frame+7));
	if ( x&0x8000 )	{
		x = ~x + 1;	// two's complement on bit level == signed -> unsigned
		sign = 1;
	} else	sign = 0;
//        Serial.print(F("parsing value: ")); Serial.println(in.x, HEX);
	if (vito_param.divid==10) {
		// special handling to avoid float usage
		// shrink the integer part to the high byte, the low byte stores the decimal part
		int8_t a = x/10;	// store the integer part in the high byte
		//if (sign) out.s.a = -out.s.a;	// adjust sign
		uint8_t b = x%10;	// store the remainder in the low byte
		if (sign)  sprintf((char*)rec_frame, "-%1i.%1u", a, b);
		else      sprintf((char*)rec_frame, "%02i.%1u", a, b);
	} else if (vito_param.divid==1) {
		//if (sign) x = -x;	// adjust sign
		sprintf((char*)rec_frame, "%02i", x);
	}
	else {
#if _DEBUG_>1
          Serial.print(F("ERROR: Parse2Bytes: parsed bytes has unknown divider!"));
#endif
          return 0;
        }
//  Serial.println(F("parsed word: ")); Serial.println((char*)rec_frame);
	return 2;
}
/*****************************************************************************/
/* example of byte swap
static inline uint32_t bswap_32_inline(uint32_t x)
{
        union {
                uint32_t x;
                struct {
                        uint8_t a;
                        uint8_t b;
                        uint8_t c;
                        uint8_t d;
                } s;
        } in, out;
        in.x = x;
        out.s.a = in.s.d;
        out.s.b = in.s.c;
        out.s.c = in.s.b;
        out.s.d = in.s.a;
        return out.x;
}*/
/*****************************************************************************/
uint8_t Parse4Bytes(void)
{	// ABCD -> DCBA
	byte sign = rec_frame[10]&0x80;
	int32_t x = *(uint32_t*)(rec_frame+7);
	//if ( $a & 0x80000000 ) $a = $a - 0x100000000;	// resolve sign
	if ( sign )	x = ~x + 1;	// convert to unsigned
	if (vito_param.divid==10) {
		// special handling to avoid float usage
		// shrink the integer part to upper 3 bytes, the lower byte stores the decimal part
		uint8_t temp = x%10;		// store only in the low byte
		x = x/10;
		//in.x = in.x << 8;	// shift to upper 3 bytes
		//in.x = in.x | temp;			// merge with the decimal byte
		if ( sign ) x = ~x + 1;	// recover sign
		sprintf((char*)rec_frame, "%i.%1i", x, temp);
	} else if (vito_param.divid==1)
		sprintf((char*)rec_frame, "%i", x);
	else {
#if _DEBUG_>1
          Serial.print(F("ERROR: Parse4Bytes: parsed parameter has unkown divider!"));
#endif
          return 0;
	}
	return 4;
}
/*****************************************************************************/
/*****************************************************************************/
uint8_t ParseWrValue(uint8_t in)//, void * val)
{
//	echo "\$addr = $addr; \$bytes = $bytes; \$divider = $divider; \$unp = $unp\n";
	Vito_GetParam(in);
	
	if ( vito_param.typ=='i' )	// integer
	{
		// multiply by the divider
		if ( vito_param.len==2 )	return Build2Bytes();//*(int16_t *)val);
		else if ( vito_param.len==4 )	return Build4Bytes();//*(int32_t *)val);
	}
	else if ( vito_param.typ=='b' )	// binary
	{
		if ( send_frame[7]>0 )	send_frame[7] = 0x01;
		//else		send_frame[7] = 0x00;
		return 1;	// parsed one byte
	}
	else if ( vito_param.typ=='s' )	// string
	{
		return BuildStringValue();//(char*)val);
	}
#if _DEBUG_>1
	Serial.print(F("ERROR: ParseWrValue: parameter has unknown format!"));
#endif
	return 0;
}
/*****************************************************************************/
/*****************************************************************************/
byte Build2Bytes(void)//int16_t dt)
{
	int16_t dt = *(int16_t*)&send_frame[7];
	if (vito_param.divid==10)
	{	// special handling to avoid float usage
		// upper byte shrunken as the integer part, low byte stores the decimal part
		if ( dt&0x8000 )	dt = (int16_t)(dt>>8)*10 - (dt&0xFF);
		else				dt = (int16_t)(dt>>8)*10 + (dt&0xFF);
	} 
	else if (vito_param.divid==1)
	{
		*(int16_t*)(send_frame+7) = dt;
	} else {
#if _DEBUG_>1
		Serial.print(F("ERROR: Build2Bytes: parameter has unknown format!"));
#endif
		return 0;
	}
	return 2;	// written bytes
}
/*****************************************************************************/
/*****************************************************************************/
byte Build4Bytes(void)//int32_t dt)
{
int32_t dt = *(int32_t*)&send_frame[7];
	if (vito_param.divid==10)
	{	// special handling to avoid float usage
		// upper 3 bytes shrunken as the integer part, low byte stores the decimal part
		dt = (uint32_t)(dt>>8)*10 + (dt&0xFF);
	}
	else if (vito_param.divid==1)
	{
		byte i = 0;
		do {
			send_frame[7+i] = dt&0xFF;	// send low byte first
			dt >>= 8;
		} while (++i<4);
	} else {
#if _DEBUG_>1
		Serial.print(F("ERROR: Build4Bytes: parameter has unknown format!"));
#endif
		return 0;
	}
	return 4;	// written bytes
}
/*****************************************************************************/
/*****************************************************************************/
byte BuildStringValue(void)//char * dt)
{	// input is string, convert it to hex
	byte * dt = send_frame+7;
	byte dtlen = strlen((char*)dt);
/*	uint8_t ret = '';
	for (uint8_t i=0; i<dtlen/2; i++)
		send_frame[7+i] = atoi((const char*)dt[2*i])<<4 + atoi((const char*)dt[2*i+1]);
*/
	return dtlen;///2;
}

