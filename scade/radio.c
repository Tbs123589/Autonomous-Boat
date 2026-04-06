#include "radio.h"
#include "usart.h"

void Radio_Init(void)
{
    // 开启串口空闲中断(可选，收地面指令用)
    __HAL_UART_ENABLE_IT(&RADIO_UART_HANDLE, UART_IT_IDLE);
}

// 直接串口透传发送
void Radio_Send_Data(uint8_t *data,uint16_t len)
{
    HAL_UART_Transmit(&RADIO_UART_HANDLE, data, len, 100);
}

// 简单异或校验
uint8_t Radio_CheckSum(uint8_t *buf,uint16_t len)
{
    uint8_t sum = 0;
    for(uint16_t i=0;i<len;i++) sum ^= buf[i];
    return sum;
}
