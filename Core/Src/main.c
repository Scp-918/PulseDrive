/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "hrtim.h"
#include "i2c.h"
#include "spi.h"
#include "usart.h"
#include "usb_device.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usbd_cdc_if.h" // [添加 1] 引入CDC接口头文件，用于调用 CDC_Transmit_FS
#include "AD4007.h"      // [新增] 引入 AD4007 模块
#include "TMUX1108.h"
#include <string.h>      // [添加 1] 用于 strlen 函数
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
uint8_t CDC_Transmit_Wait(uint8_t* Buf, uint16_t Len); // [新增] 阻塞式发送原型
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define US_DELAY_COUNT 2400
static void Delay_us_50(void)
{
    // 使用 volatile 和 __NOP() 确保编译器不会优化掉循环，以实现准确的忙等。
    for (volatile uint32_t i = 0; i < US_DELAY_COUNT; i++)
    {
        __NOP();
    }
}


/**
  * @brief  阻塞式 CDC 发送函数，直到 USB 缓冲区空闲并接受数据为止
  * @param  Buf: 待发送数据缓冲区
  * @param  Len: 数据长度
  * @retval USBD_StatusTypeDef 状态 (USBD_OK, USBD_FAIL 等)
  */
uint8_t CDC_Transmit_Wait(uint8_t* Buf, uint16_t Len)
{
    uint8_t status;
    do 
    {
        // 尝试将数据提交给 USB 硬件
        status = CDC_Transmit_FS(Buf, Len);
        
        // 如果 USB 忙碌，执行 50us 忙等
        if (status == USBD_BUSY) 
        {
            // [修改] 使用 50us 忙等代替 HAL_Delay(1)
            Delay_us_50(); 
        }
        
    } while (status == USBD_BUSY); // 一直重试，直到不是忙碌状态
    
    return status;
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
  // MX_HRTIM1_Init();
  // MX_I2C3_Init();
  // MX_SPI1_Init();
  MX_SPI3_Init();
  // MX_USART1_UART_Init();
  MX_USB_Device_Init();
/* USER CODE BEGIN 2 */
  // [初始化] TMUX GPIO (确保 PA8 等引脚被正确配置为输出)
  TMUX_Global_Init();

  /* USER CODE BEGIN 2 */
  // 1. 电源上电序列
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_7, GPIO_PIN_SET);  // E5V
  HAL_Delay(50);
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8, GPIO_PIN_SET);  // E3.3V
  HAL_Delay(50);
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, GPIO_PIN_SET); // E4V
  HAL_Delay(5000); // 等待电源稳定

  // 2. AD4007 初始化
  char msg[64];
  if (AD4007_Init_Safe() == HAL_OK) {
      strcpy(msg, "System Ready: AD4007 OK\r\n");
  } else {
      strcpy(msg, "System Ready: AD4007 FAIL\r\n");
  }
  HAL_Delay(1000); // 等待USB连接稳定
  CDC_Transmit_FS((uint8_t*)msg, strlen(msg));

  // [静态配置]
  // KH: 常态连接 S3 (Channel 3: A2=0, A1=1, A0=0)
  TMUX_KH_SetChannel(TMUX_CH_S2);
  
  // KL: 常态连接 S2 (Channel 2: A2=0, A1=0, A0=1)
  TMUX_KL_SetChannel(TMUX_CH_S3);
  
  // KB: 初始状态设为断开
  TMUX_KB_SetChannel(TMUX_CH_NONE);
  
  /* USER CODE END 2 */

  // 定义时间戳和状态变量
  uint32_t last_kb_tick = 0;
  uint32_t last_adc_tick = 0;
  
  // KB 状态: 0 -> Disabled (EN=0), 1 -> Enabled (EN=1, S7)
  uint8_t kb_enable_state = 0;

  /* USER CODE BEGIN WHILE */
  while (1)
  {
      uint32_t current_tick = HAL_GetTick();

      // [任务1] KB EN_BRIDGE 信号每 1000ms 切换
      if (current_tick - last_kb_tick >= 1000)
      {
          last_kb_tick = current_tick;
          
          if (kb_enable_state == 0)
          {
              // 状态翻转：导通
              // 固定选择 S7 (A2=1, A1=1, A0=0)，并拉高 EN
              TMUX_KB_SetChannel(TMUX_CH_S7);
              kb_enable_state = 1;
              
              // (可选) 调试输出
              // CDC_Transmit_FS((uint8_t*)"KB: ON (S7)\r\n", 13);
          }
          else
          {
              // 状态翻转：关断
              // 拉低 EN，地址线状态保持(或不重要)
              TMUX_KB_SetChannel(TMUX_CH_NONE);
              kb_enable_state = 0;
              
              // (可选) 调试输出
              // CDC_Transmit_FS((uint8_t*)"KB: OFF\r\n", 9);
          }
      }

      // [任务2] ADC 每 500ms 采样并输出
      if (current_tick - last_adc_tick >= 500)
      {
          last_adc_tick = current_tick;

          int32_t code = AD4007_Read_Single();
          float voltage = AD4007_ConvertToVoltage(code);
          
          // 打印当前 KB 状态和电压值
          char *kb_status_str = (kb_enable_state == 1) ? "ON(S7)" : "OFF";
          sprintf(msg, "KB:%s | ADC:%.4f V\r\n", kb_status_str, voltage);
          
          CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
      }
      
      // 简单防死循环延时
      HAL_Delay(50); 
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

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 18;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV6;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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
