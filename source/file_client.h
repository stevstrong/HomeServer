
#ifndef _FILE_CLIENT_H_
#define _FILE_CLIENT_H_

#include <Ethernet.h>
#include <SdFat.h>

void FileClient_Check(void);
void FileClient_Init(int chip_select_pin);
//void File_PrintFile(void);
void File_WriteDataToFile(void);
void File_LoadFileLine(void);
void File_GetFileLine(int line_nr);
void File_GetRecordLine(int line_nr);
char * File_GetRecordedParameter(int line_nr);
void File_CheckPostRequest(EthernetClient cl);
void File_CheckDirFile(EthernetClient client);
void File_SendFile(EthernetClient cl);
void File_SetDateTime(uint16_t * date, uint16_t * time);
uint8_t File_CheckMissingRecordFile(void);
void File_NewDay(void);

// logging errors
#define NEW_ENTRY  0x01
#define ADD_NL     0x02
#define P_MEM      0x04
void File_LogClient(const char * txt);
void File_LogError(const char * txt, byte ctrl);
void File_LogMessage(const char * txt, byte ctrl);
#define File_Log File_LogError

extern SdFile file, root;


#endif


