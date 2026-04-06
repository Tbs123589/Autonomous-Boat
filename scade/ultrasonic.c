#include "ultrasonic.h"

static float dis_buf[US_FILTER_NUM];
static uint8_t buf_idx = 0;

// 微秒延时 依赖基础定时器(自己在CubeMX开TIM2)
extern void HAL_Delay_us(uint32_t us);

void Ultrasonic_Init(void)
{
    GPIO_InitTypeDef gpio_conf = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    // Trig 输出
    gpio_conf.Pin   = TRIG_GPIO_PIN;
    gpio_conf.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio_conf.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(TRIG_GPIO_PORT, &gpio_conf);

    // Echo 输入
    gpio_conf.Pin   = ECHO_GPIO_PIN;
    gpio_conf.Mode  = GPIO_MODE_INPUT;
    gpio_conf.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(ECHO_GPIO_PORT, &gpio_conf);

    HAL_GPIO_WritePin(TRIG_GPIO_PORT, TRIG_GPIO_PIN, GPIO_PIN_RESET);
}

// 单次测距
static float US_Get_Single(void)
{
    uint32_t t_start,t_end;
    float dis;

    HAL_GPIO_WritePin(TRIG_GPIO_PORT, TRIG_GPIO_PIN, GPIO_PIN_SET);
    HAL_Delay_us(10);
    HAL_GPIO_WritePin(TRIG_GPIO_PORT, TRIG_GPIO_PIN, GPIO_PIN_RESET);

    uint32_t timeout = HAL_GetTick() + 200;
    while(HAL_GPIO_ReadPin(ECHO_GPIO_PORT,ECHO_GPIO_PIN)==0)
    {
        if(HAL_GetTick()>timeout) return 0;
    }
    t_start = HAL_GetTick();

    while(HAL_GPIO_ReadPin(ECHO_GPIO_PORT,ECHO_GPIO_PIN)==1)
    {
        if(HAL_GetTick()>timeout) return 0;
    }
    t_end = HAL_GetTick();

    dis = (t_end - t_start) * 1000 * 0.017f;

    if(dis < US_MIN_DIST_CM || dis > US_MAX_DIST_CM) dis = 0;
    return dis;
}

// 滑动平均滤波对外接口
float Ultrasonic_Get_Distance(void)
{
    float sum = 0;
    uint8_t cnt = 0;

    dis_buf[buf_idx++] = US_Get_Single();
    if(buf_idx >= US_FILTER_NUM) buf_idx = 0;

    for(int i=0;i<US_FILTER_NUM;i++)
    {
        if(dis_buf[i] > 0)
        {
            sum += dis_buf[i];
            cnt++;
        }
    }
    if(cnt == 0) return 0;
    return sum / cnt;
}
