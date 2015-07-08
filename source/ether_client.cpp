
#include "sys_cfg.h"
#include "ether_client.h"
#include "ether_server.h"
#include "dreambox.h"

ether_client_id_t clientID;	// should be set by caller before running EtherClient_ReceiveData()

byte header, chunked, flush;
struct {
	int size;
	byte index;
	byte head[4];
} chunk;
#define cl_buf	s_buf
#define cl_ind	s_ind
/*****************************************************************************/
void ParseHeaderLine(void)
{
	if ( flush>0 ) return;
#if _DEBUG_>0
	Serial.print(F("(received header line: ")); Serial.println(cl_buf);
#endif
	if ( strstr_P(cl_buf, PSTR("chunked"))>0 ) {
		Serial.println(F("*** chunked payload detected ! ***"));
		chunked = 1;
		flush = 1;	// no more header data needed
	}
	switch ( clientID ) {
		case CLIENT_DREAMBOX:
			flush = Dreambox_ParseHeaderLine();
			break;
/* put other clients here:
		case CLIENT_2:
			flush = Client2_ParseHeaderLine();
			break;*/
	}
}
/*****************************************************************************/
void ReceiveHeader(char c)
{
	if ( c=='\n' ) {
		// end of header line
		cl_buf[cl_ind] = 0;	// mark end of string
		if ( cl_ind==0 ) {
			header = 0; // detected second consecutive '\n' -> end of HTTP header
			flush = 0;
		} else {
			ParseHeaderLine();	// call here routine to parse line data
			cl_ind = 0;	// reset buffer pointer
		}
	} else if ( flush==0 && cl_ind<(SERVER_BUFFER_MAX_SIZE-1) )	// receive as long as buffer does not overflow
				cl_buf[cl_ind++] = c;	// store character and increment buffer pointer
}
/*****************************************************************************/
void ParsePayloadLine(void)
{
	if ( flush>0 ) return;
#if _DEBUG_>0
	Serial.print(F("(payload line: ")); Serial.println(cl_buf);
#endif
	switch ( clientID ) {
		case CLIENT_DREAMBOX:
			flush = Dreambox_ParsePayloadLine();
			break;
	}
}
/*****************************************************************************/
void ReceiveNormalPayload(char c)
{	// not chunked payload
	if ( c=='\n' ) {
		// end of header line
		cl_buf[cl_ind] = 0;	// mark end of string
		ParsePayloadLine();	// call here routine to parse line data
		cl_ind = 0;	// reset buffer pointer
	} else if ( flush==0 && cl_ind<(SERVER_BUFFER_MAX_SIZE-1) )	// receive as long as buffer does not overflow
				cl_buf[cl_ind++] = c;	// store character and increment buffer pointer
}
/*****************************************************************************/
void ParseChunkedLine(void)
{
#if _DEBUG_>0
	Serial.print(F("(chunked payload line: ")); Serial.println(cl_buf);
#endif
	ParsePayloadLine();
}
/*****************************************************************************/
void ParseChunkedString(void)
{
#if _DEBUG_>0
	Serial.println(F("(chunked payload string:\n*****"));
	Serial.println(cl_buf);
	Serial.println(F("*****"));
#endif
	ParseChunkedLine();
}
/*****************************************************************************/
byte HexChr2Byte(byte in)
{
	if ( in>='0' && in<='9')		in = in - '0';
	else if ( in>='a' && in<='f')	in = in + 10 - 'a';
	else if ( in>='A' && in<='F')	in = in + 10 - 'A';
	return in;
}
/*****************************************************************************/
void HexStr2Int(void)
{
	chunk.size = 0;
	for ( chunk.index = 0; chunk.index<4 ; chunk.index++) {
		byte x = chunk.head[chunk.index];
		if ( x<'0' ) break;
		chunk.size =  chunk.size * 16 + HexChr2Byte(x);
	}
	chunk.index = 4;	// end of chunk head reception, switch to chunk data reception
}
/*****************************************************************************/
void ReceiveChunkedPayload(char c)
{
	// special handling of chunked payload.
	// Format:
	//		nr_bytes_1 (one or more characters in ASCII HEX) + \r\n
	//		[data_bytes_1] + \r\n
	//		...
	//		nr_bytes_n (one or more characters in ASCII HEX) + \r\n
	//		[data_bytes_n] + \r\n
	//		"0" (ASCII 0) + \r\n (3 bytes in total)
	//		\r\n	(end of payload)

	if ( chunk.index<4 )
	{	// chunk header
		if ( chunk.index==0 && c<'0' )	// wrong characters received, do nothing
			return;
		chunk.head[chunk.index++] = c;	// store here the chunk header bytes
		if ( c=='\n' )	// end of chunk header
			HexStr2Int();	// chunk.size will be set inside this function
	}
	else
	{	// chunk data
		if ( (chunk.size--)>0 ) {
			// receive chunked data bytes as long as buffer does not overflow.
			if ( c=='\n' ) {
				cl_buf[cl_ind] = 0;	// mark end of payload line
				ParseChunkedLine();	// parse the temporary string
			} else
				if ( c>=' ' && cl_ind<(SERVER_BUFFER_MAX_SIZE-1) )	// don't store CR NL TAB
					cl_buf[cl_ind++] = c;	// store character and increment buffer pointer
		} else {
			// chunk size = 0, no more chunk bytes to receive, expected is '\n'
			if ( c!='\n' ) {
				Serial.print(F("Chunk receiving error: extra character received: ")); Serial.println(c);
				return;
			}
			cl_buf[cl_ind] = 0;	// mark end of payload string
			// check end of entire HTTP payload
			if ( chunk.head[0]=='0' && chunk.head[1]=='\n' ) {	// end of payload
				ParseChunkedString();
				cl_ind = 0;
			}
			chunk.index = 0;	// end of chunk data. switch to receive chunk header
			*(long *)chunk.head = 0;	// clear chunk header data
		}
	}
}
/*****************************************************************************/
void EtherClient_ReceiveData(EthernetClient cli, ether_client_id_t clID)
{
	clientID = clID;
	cl_ind = 0;
	header = 1;
	chunked = 0; flush = 0;
	chunk.index = 0; chunk.size = 0;
	*(long *)&chunk.head = 0;
	// read and store the first header line only
	while ( cli.connected() ) 
	{
		while ( cli.available() )
		{
			char c = cli.read();
			if ( c=='\r' )	continue;  // don't consider CR
			if ( header )
				ReceiveHeader(c);
			else
				if ( chunked )
					ReceiveChunkedPayload(c);
				else
					ReceiveNormalPayload(c);
		}
	}
}
