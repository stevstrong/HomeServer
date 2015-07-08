
#ifndef _ETHER_CLIENT_H_
#define _ETHER_CLIENT_H_


#include <Ethernet.h>

typedef enum {CLIENT_DREAMBOX} ether_client_id_t;	// add here further clients

void EtherClient_ReceiveData(EthernetClient cli, ether_client_id_t clID);


#endif
