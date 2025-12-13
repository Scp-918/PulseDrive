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

/* 全局变量用于存储 ADC 结果 */
volatile int32_t adc_raw_3us = 0;
volatile int32_t adc_raw_300us = 0;
volatile uint8_t measure_done = 0;
/* 全局变量用于存储 中断次数 */
volatile int32_t num_3us = 0;
volatile int32_t num_300us = 0;
volatile uint8_t num_done = 0;
extern volatile int32_t debug_isr_cnt;
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
/**
  * @brief  HRTIM 比较匹配回调 (处理第一次采样)
  * 触发源: CMP2 (对应 3.5us 时刻)
  */
void HAL_HRTIM_TimerCompareCallback(HRTIM_HandleTypeDef * hhrtim, uint32_t TimerIdx)
{
  if (TimerIdx == HRTIM_TIMERINDEX_TIMER_A)
  {
      // 此时是 3.5us，第一次 CNV 脉冲刚结束，ADC 转换完成
      //adc_raw_3us = AD4007_Read_SPI_Only();
      num_3us++;
  }
}

/**
  * @brief  HRTIM 重复/周期事件回调 (处理第二次采样)
  * 触发源: REP/PER (对应 300.5us 周期结束时刻)
  */
void HAL_HRTIM_RepetitionEventCallback(HRTIM_HandleTypeDef * hhrtim, uint32_t TimerIdx)
{
  if (TimerIdx == HRTIM_TIMERINDEX_TIMER_A)
  {
      // 此时是 300.5us，第二次 CNV 脉冲结束，EN_BRIDGE 自动拉低
      //adc_raw_300us = AD4007_Read_SPI_Only();
      //measure_done = 1; // 标记本轮数据准备就绪
      num_300us++;
      num_done=1;
  }
}

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
// uint8_t CDC_Transmit_Wait(uint8_t* Buf, uint16_t Len)
// {
//     uint8_t status;
//     do 
//     {
//         // 尝试将数据提交给 USB 硬件
//         status = CDC_Transmit_FS(Buf, Len);
        
//         // 如果 USB 忙碌，执行 50us 忙等
//         if (status == USBD_BUSY) 
//         {
//             // [修改] 使用 50us 忙等代替 HAL_Delay(1)
//             Delay_us_50(); 
//         }
        
//     } while (status == USBD_BUSY); // 一直重试，直到不是忙碌状态
    
//     return status;
// }
uint8_t CDC_Transmit_Wait(uint8_t* Buf, uint16_t Len)
{
    uint8_t status;
    uint32_t timeout = 20000; // 约 1秒超时 (20000 * 50us)
    
    do 
    {
        status = CDC_Transmit_FS(Buf, Len);
        if (status == USBD_BUSY) 
        {
            Delay_us_50(); 
            if (--timeout == 0) return USBD_BUSY; // 超时退出，避免死锁
        }
    } while (status == USBD_BUSY);
    
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
  // MX_GPIO_Init();
  // // MX_HRTIM1_Init();
  // // MX_I2C3_Init();
  // // MX_SPI1_Init();
  // MX_SPI3_Init();
  // MX_USART1_UART_Init();
  // MX_USB_Device_Init();
  /* USER CODE BEGIN 2 */
  //初始化GPIO与通信
  MX_GPIO_Init();
  MX_SPI3_Init();
  //MX_USART1_UART_Init();
  MX_USB_Device_Init();

  //初始化TMUX GPIO
  TMUX_Global_Init();

  // 1. 电源上电序列
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_7, GPIO_PIN_SET);  // E5V
  HAL_Delay(50);
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8, GPIO_PIN_SET);  // E3.3V
  HAL_Delay(50);
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, GPIO_PIN_SET); // E4V
  HAL_Delay(1000); // 等待电源稳定

  // 2. AD4007 初始化
  char msg[64];
  if (AD4007_Init_Safe() == HAL_OK) {
      strcpy(msg, "System Ready: AD4007 OK\r\n");
  } else {
      strcpy(msg, "System Ready: AD4007 FAIL\r\n");
  }
  HAL_Delay(1000); // 等待USB连接稳定
  CDC_Transmit_Wait((uint8_t*)msg, strlen(msg));

  // 3. 初始读取一次 ADC，验证功能
  int32_t code = AD4007_Read_Single();
  float voltage = AD4007_ConvertToVoltage(code);
  sprintf(msg, "ADC:%.4f V\r\n", voltage);
  CDC_Transmit_Wait((uint8_t*)msg, strlen(msg));

  // [静态配置]
  // KH: 常态连接 S3 (Channel 3: A2=0, A1=1, A0=0)
  TMUX_KH_SetChannel(TMUX_CH_S2);
  // KL: 常态连接 S2 (Channel 2: A2=0, A1=0, A0=1)
  TMUX_KL_SetChannel(TMUX_CH_S3);
  // KB: 初始状态设为断开
  TMUX_KB_SetChannel(TMUX_CH_S7);

  // 发送成功消息
  sprintf(msg, "TMUX SUCCESS\r\n");
  CDC_Transmit_Wait((uint8_t*)msg, strlen(msg));

  // [强制修复 GPIO] 确保 PA8/PA9 复用为 HRTIM
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  __HAL_RCC_GPIOA_CLK_ENABLE();
  
  // PA8 -> HRTIM_CHA1, PA9 -> HRTIM_CHA2
  GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;       // 复用推挽输出
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF13_HRTIM1; // 必须是 AF13
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // 4. HRTIM 初始化
  MX_HRTIM1_Init();

  // 5. 启动 HRTIM 波形输出
  if (HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2) != HAL_OK)
  {
    Error_Handler();
    sprintf(msg, "wrong\r\n");
    CDC_Transmit_Wait((uint8_t*)msg, strlen(msg));
  }
  // 相当于 Set Output Enable Register
  hhrtim1.Instance->sCommonRegs.OENR |= (HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2);

  HAL_Delay(100); 
  // 开启中断
  // 显式开启 HRTIM 中断 (这步不能省)
  // 优先级设为 1，既保证实时性，又不至于卡死 SysTick (优先级0)
  HAL_NVIC_SetPriority(HRTIM1_TIMA_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(HRTIM1_TIMA_IRQn);
  // 宏原型通常是: __HAL_HRTIM_TIMER_ENABLE_IT(__HANDLE__, __TIMER__, __INTERRUPT__)
  __HAL_HRTIM_TIMER_ENABLE_IT(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_TIM_IT_CMP2 | HRTIM_TIM_IT_REP);
  uint32_t last_print_tick = 0;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {   
      uint32_t current_tick = HAL_GetTick();
      // [核心循环] 每 10ms 触发一次高精度序列
      // 序列：EN高 -> 3us采样 -> 300us采样 -> EN低(自动)
      
      // 1. 清除标志 (注意：现在要清除 CMP2 和 REP)
      // 1. 清除标志位 (特别是 CMP 和 REP，防止上次的中断标志残留)
      // __HAL_HRTIM_TIMER_CLEAR_IT(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, 
      //                           HRTIM_TIM_IT_CMP1 | HRTIM_TIM_IT_CMP2 | 
      //                           HRTIM_TIM_IT_CMP3 | HRTIM_TIM_IT_CMP4 | 
      //                           HRTIM_TIM_IT_REP  | HRTIM_TIM_IT_UPD);
      __HAL_HRTIM_TIMER_CLEAR_IT(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, 
                                HRTIM_TIM_IT_CMP2 | HRTIM_TIM_IT_REP);

      // 2. 启动 HRTIM
      if (HAL_HRTIM_WaveformCountStart(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A) != HAL_OK)
      {
          // 错误处理
          sprintf(msg, "HRTIM wrong\r\n");
          CDC_Transmit_Wait((uint8_t*)msg, strlen(msg));
      }
      HAL_Delay(1);
      // [新增] 读取 HRTIM Timer A 的中断状态寄存器 (ISR)
      // Bit 1 = CMP2, Bit 4 = REP
      uint32_t isr_reg = hhrtim1.Instance->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].TIMxISR;
      // 3. 等待本轮测量完成 (350us 内)
      // 实际应用中可以去处理其他事情，这里简单延时确保不重入
      
      // // [打印任务] 每 1000ms 打印一次最近的数据
      if (current_tick - last_print_tick >= 1000)
      {
          last_print_tick = current_tick;
          
          // float v_3us = AD4007_ConvertToVoltage(adc_raw_3us);
          // float v_300us = AD4007_ConvertToVoltage(adc_raw_300us);
          // sprintf(msg, "T=3us: %.4f V | T=300us: %.4f V\r\n", v_3us, v_300us);
          sprintf(msg, "num_3us: %u | num_300us: %u\r\n", num_3us, num_300us); //输出中断次数
          CDC_Transmit_Wait((uint8_t*)msg, strlen(msg));
          // HW_ISR: 硬件标志位 (如果 != 0，说明硬件确实产生了中断信号)
          // DBG_CNT: 探针计数 (如果 HW_ISR!=0 但 DBG_CNT=0，说明 NVIC 表配置错了)
          // USR_CNT: 用户计数 (如果 DBG_CNT涨 但 USR_CNT=0，说明 HAL 回调函数名写错了)
          sprintf(msg, "ISR_REG: 0x%u | DBG_CNT: %u | USR_CNT: %u\r\n", 
                  isr_reg, debug_isr_cnt, num_3us);
          CDC_Transmit_Wait((uint8_t*)msg, strlen(msg));
      }

      // 确保循环周期至少 10ms
      sprintf(msg, "TMUX SUCCESS222\r\n");
      CDC_Transmit_Wait((uint8_t*)msg, strlen(msg));
      HAL_Delay(500);
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
