
#ifndef _ETHER_SERVER_H_
#define _ETHER_SERVER_H_

#define SERVER_BUFFER_MAX_SIZE  140
extern char s_buf[SERVER_BUFFER_MAX_SIZE];
extern byte s_ind;

void EtherServer_CheckForClient(void);
void EtherServer_Init(void);
void Ether_BufInit(void);
void Ether_BufAdd_P(const char * str);
void Ether_BufAddChar(char chr);
void Ether_BufAddInt(word val, byte digits);

extern EthernetServer my_server;


#endif

