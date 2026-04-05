/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

// 寄存器地址定义
#define BMI088_ACC_CHIP_ID_ADDR    0x00
#define BMI088_ACC_PWR_CONF_ADDR   0x7C
#define BMI088_ACC_PWR_CTRL_ADDR   0x7D
#define BMI088_ACC_DATA_ADDR       0x12

#define BMI088_GYRO_CHIP_ID_ADDR   0x00
#define BMI088_GYRO_DATA_ADDR      0x02

// 片选控制宏
/* 修改后的片选宏定义 */
// PH13 -> 加速度计片选 (CS1)
#define ACC_CS_L()  HAL_GPIO_WritePin(GPIOH, GPIO_PIN_13, GPIO_PIN_RESET)
#define ACC_CS_H()  HAL_GPIO_WritePin(GPIOH, GPIO_PIN_13, GPIO_PIN_SET)

// PH14 -> 陀螺仪片选 (CS2)
#define GYRO_CS_L() HAL_GPIO_WritePin(GPIOH, GPIO_PIN_14, GPIO_PIN_RESET)
#define GYRO_CS_H() HAL_GPIO_WritePin(GPIOH, GPIO_PIN_14, GPIO_PIN_SET)

/* BMM150 寄存器定义 */
#define BMM150_ADDR         (0x13 << 1) // 注意左移
#define BMM150_CHIP_ID_ADDR 0x40
#define BMM150_POWER_REG    0x4B
#define BMM150_OP_MODE_REG  0x4C
#define BMM150_DATA_X_LSB   0x42

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */



/* 适配 H7 的串口重定向 */
int __io_putchar(int ch) {
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}



// 加速度计读取（含 Dummy Byte 处理）
void BMI088_ReadAccReg(uint8_t reg, uint8_t *pData, uint16_t len) {
    uint8_t addr = reg | 0x80; // 读操作最高位置1
    uint8_t dummy;
    for(volatile int i=0; i<200; i++); // 短暂延时确保加速度计准备好数据
    ACC_CS_L();
    // 增加一个极短的延时（约几百纳秒）
    for(volatile int i=0; i<200; i++);
    HAL_SPI_Transmit(&hspi1, &addr, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(&hspi1, &dummy, 1, HAL_MAX_DELAY); // 关键：加速度计读数据前必须先收一个废字节
    HAL_SPI_Receive(&hspi1, pData, len, HAL_MAX_DELAY);
    ACC_CS_H();
    for(volatile int i=0; i<500; i++);
    // 重新开启 Cache 后，必须手动告诉 CPU 内存数据已变，不要用旧缓存
    SCB_InvalidateDCache_by_Addr((uint32_t *)pData, len);

}

// 陀螺仪读取（不含 Dummy Byte）
void BMI088_ReadGyroReg(uint8_t reg, uint8_t *pData, uint16_t len) {
    uint8_t addr = reg | 0x80;
    for(volatile int i=0; i<100; i++); // 短暂延时确保陀螺仪准备好数据
    GYRO_CS_L();
    // 增加一个极短的延时（约几百纳秒）
    for(volatile int i=0; i<50; i++);
    HAL_SPI_Transmit(&hspi1, &addr, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(&hspi1, pData, len, HAL_MAX_DELAY);
    GYRO_CS_H();
    // 重新开启 Cache 后，必须手动告诉 CPU 内存数据已变，不要用旧缓存
    SCB_InvalidateDCache_by_Addr((uint32_t *)pData, len);
    
}

// 通用写寄存器
void BMI088_WriteReg(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, uint8_t reg, uint8_t data) {
    uint8_t buf[2] = { reg & 0x7F, data }; // 写操作最高位置0
    HAL_GPIO_WritePin(GPIOx, GPIO_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, buf, 2, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(GPIOx, GPIO_Pin, GPIO_PIN_SET);
}

// 写入 BMM150 寄存器
void BMM150_WriteReg(uint8_t reg, uint8_t data) {
    HAL_I2C_Mem_Write(&hi2c1, BMM150_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
}

// 读取 BMM150 寄存器
void BMM150_ReadReg(uint8_t reg, uint8_t *pData, uint16_t len) {
    HAL_I2C_Mem_Read(&hi2c1, BMM150_ADDR, reg, I2C_MEMADD_SIZE_8BIT, pData, len, HAL_MAX_DELAY);
}

// BMM150 初始化序列
void BMM150_Init(void) {
    uint8_t id = 0;
    
    // 1. 软复位 (Soft Reset)
    // 即使读不到 ID，也要往 0x4B 写 0x01
    BMM150_WriteReg(0x4B, 0x01); 
    HAL_Delay(50); // 给芯片时间重启
    
    // 2. 尝试多次读取 ID，直到读到 0x40 或超时
    for(int retry=0; retry<5; retry++) {
        BMM150_ReadReg(0x40, &id, 1);
        if(id == 0x40) break;
        HAL_Delay(20);
    }
    printf("BMM150 Chip ID: 0x%02X\n", id);

    // 3. 开启电源模式 (Power Control)
    // 必须先让芯片退出 Suspend 进入 Sleep
    BMM150_WriteReg(0x4B, 0x01); 
    HAL_Delay(10);

    // 设置 ODR (数据频率)
    // 0x4C 的位 [5:3]： 000=10Hz, 101=20Hz, 110=25Hz, 111=30Hz
    // 建议设为 30Hz: 0x38 (0011 1000)
    BMM150_WriteReg(0x4C, 0x38); 
    
    // 配置重复次数（Repetitions）以获得更稳定的数据
    // 0x51 寄存器控制 XY 轴重复次数，0x52 控制 Z 轴
    BMM150_WriteReg(0x51, 0x04); // XY轴重复 9 次 (公式: 1+2*n)
    BMM150_WriteReg(0x52, 0x0F); // Z轴重复 15 次
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
  uint8_t id_acc = 0, id_gyro = 0;
  
  //0. BMI088 复位（必须）
  BMI088_WriteReg(GPIOH, GPIO_PIN_13, 0x7E, 0xB6); // 往 0x7E 写入 0xB6 进行复位
  HAL_Delay(50); // 复位后必须等待至少 50ms，确保 BMI088 完全重启

  // 1. 读取 ID 验证通信
  HAL_Delay(50);
  BMI088_ReadAccReg(BMI088_ACC_CHIP_ID_ADDR, &id_acc, 1);
  BMI088_ReadGyroReg(BMI088_GYRO_CHIP_ID_ADDR, &id_gyro, 1);
  printf("BMI088 Acc ID: 0x%02X (Expect 0x1E)\n", id_acc);
  printf("BMI088 Gyro ID: 0x%02X (Expect 0x0F)\n", id_gyro);

  // 2. 加速度计唤醒序列（必须严格遵守）
  BMI088_WriteReg(GPIOH, GPIO_PIN_13, BMI088_ACC_PWR_CTRL_ADDR, 0x04); 
  HAL_Delay(10);
  BMI088_WriteReg(GPIOH, GPIO_PIN_13, BMI088_ACC_PWR_CONF_ADDR, 0x00);
  HAL_Delay(50);

  // 3. 【新增】配置采样率和带宽（非常重要）
  // 写入 0x40 寄存器：设置 ODR 为 100Hz, 带宽为 Normal (0xA)
  BMI088_WriteReg(GPIOH, GPIO_PIN_13, 0x40, 0xA8); 
  HAL_Delay(50);


  // 1. 设置陀螺仪量程 (寄存器 0x0F)
  // 写入 0x00 代表 ±2000 °/s, 0x01 代表 ±1000 °/s ...
  // 船只晃动较慢，建议用 0x02 (±500 °/s) 或 0x03 (±250 °/s) 提高精度
  BMI088_WriteReg(GPIOH, GPIO_PIN_14, 0x0F, 0x02); 

  // 2. 设置陀螺仪带宽 (寄存器 0x10)
  // 写入 0x07 代表 ODR 200Hz, Bandwidth 64Hz (比较常用)
  BMI088_WriteReg(GPIOH, GPIO_PIN_14, 0x10, 0x07);

  uint8_t raw_data[6];
  int16_t ax, ay, az;

  uint8_t gyro_raw[6];
  int16_t gx, gy, gz;


  BMM150_Init();

  uint8_t mag_raw[8]; // BMM150 数据包含 X,Y,Z 和 RHALL
  int16_t mx, my, mz;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    
    // 读取加速度计 X,Y,Z (共6字节)
    BMI088_ReadAccReg(BMI088_ACC_DATA_ADDR, raw_data, 6);
    
    // 合并字节
    ax = (int16_t)((raw_data[1] << 8) | raw_data[0]);
    ay = (int16_t)((raw_data[3] << 8) | raw_data[2]);
    az = (int16_t)((raw_data[5] << 8) | raw_data[4]);
      
    // 读取陀螺仪 X, Y, Z (从 0x02 寄存器开始，共6字节)
    BMI088_ReadGyroReg(BMI088_GYRO_DATA_ADDR, gyro_raw, 6);

    // 合并字节 (注意：BMI088 寄存器通常是小端模式，低字节在前)
    gx = (int16_t)((gyro_raw[1] << 8) | gyro_raw[0]);
    gy = (int16_t)((gyro_raw[3] << 8) | gyro_raw[2]);
    gz = (int16_t)((gyro_raw[5] << 8) | gyro_raw[4]);

    // --- 2. 转换并放大 1000 倍 ---
    // 加速度计 (单位: mg, 即 1/1000 g)
    int32_t ax_mg = (int32_t)(ax / 32768.0f * 6.0f * 1000.0f);
    int32_t ay_mg = (int32_t)(ay / 32768.0f * 6.0f * 1000.0f);
    int32_t az_mg = (int32_t)(az / 32768.0f * 6.0f * 1000.0f);

    // 陀螺仪 (单位: 0.001 dps)
    int32_t gx_mdps = (int32_t)(gx / 32768.0f * 500.0f * 1000.0f);
    int32_t gy_mdps = (int32_t)(gy / 32768.0f * 500.0f * 1000.0f);
    int32_t gz_mdps = (int32_t)(gz / 32768.0f * 500.0f * 1000.0f);

    // --- 新增：读取 BMM150 磁力计数据 ---
    BMM150_ReadReg(BMM150_DATA_X_LSB, mag_raw, 8);
    
    // --- 修正后的解析代码 ---

    // 1. 解析 X 轴 (13位数据: 8位高字节 + 5位低字节)
    // 低字节 mag_raw[0] 的 Bit[7:3] 是有效位
    mx = (int16_t)((int8_t)mag_raw[1] << 5) | (mag_raw[0] >> 3);
    // 如果是负数，需要符号扩展（针对13位）
    if (mx > 4095) mx -= 8192; 

    // 2. 解析 Y 轴 (13位数据)
    my = (int16_t)((int8_t)mag_raw[3] << 5) | (mag_raw[2] >> 3);
    if (my > 4095) my -= 8192;

    // 3. 解析 Z 轴 (15位数据: 8位高字节 + 7位低字节)
    // 低字节 mag_raw[4] 的 Bit[7:1] 是有效位
    mz = (int16_t)((int8_t)mag_raw[5] << 7) | (mag_raw[4] >> 1);
    if (mz > 16383) mz -= 32768;

    // --- 修改：串口输出格式 ---
    // 为了让上位机识别，我们把磁力计数据也传上去
    printf("DATA:%ld,%ld,%ld,%ld,%ld,%ld,%d,%d,%d\r\n", 
            ax_mg, ay_mg, az_mg, gx_mdps, gy_mdps, gz_mdps, mx, my, mz);

    HAL_Delay(10); // 维持 100Hz 左右采样

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 60;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 5;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
