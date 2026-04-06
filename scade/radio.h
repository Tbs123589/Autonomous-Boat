#ifndef __RADIO_H
#define __RADIO_H
#include "stm32f1xx_hal.h"

// Êý´«´®¿Ú£ºUSART1
#define RADIO_UART_HANDLE huart1
#define RADIO_BUFF_LEN    64

void Radio_Init(void);
void Radio_Send_Data(uint8_t *data,uint16_t len);
uint8_t Radio_CheckSum(uint8_t *buf,uint16_t len);

#endif
