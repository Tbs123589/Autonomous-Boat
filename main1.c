#include "main.h"
#include "ultrasonic.h"
#include "radio.h"
#include "data_pack.h"
#include "tim.h"
#include "usart.h"

// ΢����ʱ���������ڵײ㶨ʱ��ʵ��
void HAL_Delay_us(uint32_t us);

int main(void)
{
    // �ײ�HAL��ʼ��
    HAL_Init();
    SystemClock_Config();

    // CubeMX�Զ����������ʼ��
    MX_GPIO_Init();
    MX_TIM2_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();

    // 1. ���ģ���ʼ��
    Ultrasonic_Init();
    Radio_Init();

    US_Radio_Frame_t send_frame;
    float us_dis;

    while (1)
    {
        // 2. ��ȡ������
        us_dis = Ultrasonic_Get_Distance();

        // 3. �����ͨ��֡
        Data_Pack_US(&send_frame, 0x01, us_dis);

        // 4. ͨ��������̨����
        Radio_Send_Data((uint8_t*)&send_frame, sizeof(US_Radio_Frame_t));

        HAL_Delay(1);  // 10Hz�ϴ�Ƶ�ʣ��ɵ�
    }
}
