#include "unity_test.h"
#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include <string.h>

/* 外部变量声明（来自你的main.c） */
extern UART_HandleTypeDef g_uart1_handle;
extern uint8_t g_usart_rx_buf[];
extern uint16_t g_usart_rx_sta;

/* 每个测试前的设置 */
void setUp(void)
{
    g_usart_rx_sta = 0;
    memset(g_usart_rx_buf, 0, 256);  /* 清空接收缓冲区 */
}

/* 每个测试后的清理 */
void tearDown(void)
{
    delay_ms(10);
}

/* 测试串口通信功能 */
void test_usart_communication(void)
{
    const char *test_msg = "Unity Test Message";
    
    /* 测试1：验证串口句柄 */
    TEST_ASSERT_NOT_NULL_MESSAGE(&g_uart1_handle, "串口句柄无效");
    
    /* 测试2：验证波特率 */
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(115200, g_uart1_handle.Init.BaudRate, 
                                     "波特率应为115200");
    
    /* 测试3：测试串口发送 */
    HAL_StatusTypeDef status = HAL_UART_Transmit(&g_uart1_handle, 
                                                (uint8_t*)test_msg, 
                                                strlen(test_msg), 
                                                1000);
    TEST_ASSERT_EQUAL_MESSAGE(HAL_OK, status, "串口发送失败");
    
    printf("\r\n? 串口通信测试通过\r\n");
}

/* 运行所有测试 */
void run_unity_tests(void)
{
    printf("\r\n==================================\r\n");
    printf("   启动Unity测试\r\n");
    printf("==================================\r\n");
    
    UNITY_BEGIN();
    RUN_TEST(test_usart_communication);
    UNITY_END();
    
    printf("\r\n==================================\r\n");
    printf("   测试完成\r\n");
    printf("==================================\r\n");
}

