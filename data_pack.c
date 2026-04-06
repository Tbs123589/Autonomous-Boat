#include "data_pack.h"
#include "radio.h"
#include "stm32f1xx_hal.h"

void Data_Pack_US(US_Radio_Frame_t *frame, uint8_t id, float dis)
{
    frame->head[0]    = 0xAA;
    frame->head[1]    = 0xBB;
    frame->sensor_id  = id;
    frame->distance_cm= dis;
    frame->time_ms    = HAL_GetTick();

    // 校验不含check字节
    uint8_t *p = (uint8_t*)frame;
    frame->check = Radio_CheckSum(p, sizeof(US_Radio_Frame_t)-1);
}
