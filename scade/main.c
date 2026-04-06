#include "main.h"
#include "ultrasonic.h"
#include "radio.h"
#include "data_pack.h"
#include "tim.h"
#include "usart.h"

// 微秒延时声明，放在底层定时器实现
void HAL_Delay_us(uint32_t us);

int main(void)
{
    // 底层HAL初始化
    HAL_Init();
    SystemClock_Config();

    // CubeMX自动生成外设初始化
    MX_GPIO_Init();
    MX_TIM2_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();

    // 1. 逐个模块初始化
    Ultrasonic_Init();
    Radio_Init();

    US_Radio_Frame_t send_frame;
    float us_dis;

    while (1)
    {
        // 2. 读取超声波
        us_dis = Ultrasonic_Get_Distance();

        // 3. 打包成通信帧
        Data_Pack_US(&send_frame, 0x01, us_dis);

        // 4. 通过数传电台发送
        Radio_Send_Data((uint8_t*)&send_frame, sizeof(US_Radio_Frame_t));

        HAL_Delay(100);  // 10Hz上传频率，可调
    }
}
