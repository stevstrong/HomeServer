#ifndef ENERGY_CAM_H
#define ENERGY_CAM_H


#include "Arduino.h"
#include "sys_cfg.h"

//void Modbus_Init(void);
void EC_Init(void);
void EC_NewDay(void);
uint8_t EC_ReadValue(void);


#endif


