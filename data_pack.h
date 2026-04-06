#ifndef __DATA_PACK_H
#define __DATA_PACK_H
#include <stdint.h>

// 上传帧格式：帧头+传感器ID+距离+时间戳+校验
typedef struct
{
    uint8_t head[2];     // 0xAA 0xBB
    uint8_t sensor_id;
    float   distance_cm;
    uint32_t time_ms;
    uint8_t check;
}US_Radio_Frame_t;

void Data_Pack_US(US_Radio_Frame_t *frame, uint8_t id, float dis);

#endif
