/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    hrtim.c
  * @brief   This file provides code for the configuration
  *          of the HRTIM instances.
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
#include "hrtim.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

HRTIM_HandleTypeDef hhrtim1;

/* HRTIM1 init function */
void MX_HRTIM1_Init(void)
{
  HRTIM_TimeBaseCfgTypeDef pTimeBaseCfg = {0};
  HRTIM_TimerCfgTypeDef pTimerCfg = {0};
  HRTIM_CompareCfgTypeDef pCompareCfg = {0};
  HRTIM_OutputCfgTypeDef pOutputCfg = {0};

  hhrtim1.Instance = HRTIM1;
  hhrtim1.Init.HRTIMInterruptResquests = HRTIM_IT_NONE;
  hhrtim1.Init.SyncOptions = HRTIM_SYNCOPTION_NONE;
  if (HAL_HRTIM_Init(&hhrtim1) != HAL_OK)
  {
    Error_Handler();
  }
  // DLL 校准 (必须保留)
  if (HAL_HRTIM_DLLCalibrationStart(&hhrtim1, HRTIM_CALIBRATIONRATE_3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_PollForDLLCalibration(&hhrtim1, 10) != HAL_OK)
  {
    Error_Handler();
  }

  // --- 1. 时基配置 (Time Base) ---
  // 将周期设为 300.5us (43272 ticks)
  // 这样 300.5us 时会自动产生 PER 事件，用于复位引脚
  pTimeBaseCfg.Period = 43272; 
  pTimeBaseCfg.RepetitionCounter = 0x00;
  pTimeBaseCfg.PrescalerRatio = HRTIM_PRESCALERRATIO_DIV1; // 144MHz
  pTimeBaseCfg.Mode = HRTIM_MODE_SINGLESHOT_RETRIGGERABLE;
  if (HAL_HRTIM_TimeBaseConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, &pTimeBaseCfg) != HAL_OK)
  {
    Error_Handler();
  }

  // --- 2. Timer 配置 ---
  // 启用 CMP2 (3.5us) 和 REP/PER (300.5us) 中断
  pTimerCfg.InterruptRequests = HRTIM_TIM_IT_CMP2 | HRTIM_TIM_IT_REP; 
  pTimerCfg.DMARequests = HRTIM_TIM_DMA_NONE;
  pTimerCfg.PushPull = HRTIM_TIMPUSHPULLMODE_DISABLED;
  pTimerCfg.FaultEnable = HRTIM_TIMFAULTENABLE_NONE;
  pTimerCfg.FaultLock = HRTIM_TIMFAULTLOCK_READWRITE;
  pTimerCfg.DeadTimeInsertion = HRTIM_TIMDEADTIMEINSERTION_DISABLED;
  pTimerCfg.DelayedProtectionMode = HRTIM_TIMER_A_B_C_DELAYEDPROTECTION_DISABLED;
  pTimerCfg.UpdateTrigger = HRTIM_TIMUPDATETRIGGER_NONE;
  pTimerCfg.ResetTrigger = HRTIM_TIMRESETTRIGGER_NONE;
  pTimerCfg.ResetUpdate = HRTIM_TIMUPDATEONRESET_DISABLED;
  if (HAL_HRTIM_WaveformTimerConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, &pTimerCfg) != HAL_OK)
  {
    Error_Handler();
  }

  // --- 3. 比较单元配置 (Event Set) ---
  
  // CMP1: 3us (CNV 第1次上升)
  pCompareCfg.CompareValue = 432;
  HAL_HRTIM_WaveformCompareConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_COMPAREUNIT_1, &pCompareCfg);

  // CMP2: 3.5us (CNV 第1次下降)
  pCompareCfg.CompareValue = 504;
  HAL_HRTIM_WaveformCompareConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_COMPAREUNIT_2, &pCompareCfg);

  // CMP3: 300us (CNV 第2次上升)
  pCompareCfg.CompareValue = 43200;
  HAL_HRTIM_WaveformCompareConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_COMPAREUNIT_3, &pCompareCfg);

  // CMP4: 0.14us (EN 开始上升) -> 设为 20 ticks
  // 用来在计时刚开始时立刻拉高 EN
  pCompareCfg.CompareValue = 20; 
  HAL_HRTIM_WaveformCompareConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_COMPAREUNIT_4, &pCompareCfg);

  // --- 4. 输出配置 (核心逻辑) ---

  // TA1 (EN_BRIDGE): 
  // Set: CMP4 (0.14us)
  // Reset: PER (300.5us 周期结束)
  pOutputCfg.Polarity = HRTIM_OUTPUTPOLARITY_HIGH;
  pOutputCfg.SetSource = HRTIM_OUTPUTSET_TIMCMP4; 
  pOutputCfg.ResetSource = HRTIM_OUTPUTRESET_TIMPER; 
  pOutputCfg.IdleMode = HRTIM_OUTPUTIDLEMODE_NONE;
  pOutputCfg.IdleLevel = HRTIM_OUTPUTIDLELEVEL_INACTIVE;
  pOutputCfg.FaultLevel = HRTIM_OUTPUTFAULTLEVEL_NONE;
  pOutputCfg.ChopperModeEnable = HRTIM_OUTPUTCHOPPERMODE_DISABLED;
  pOutputCfg.BurstModeEntryDelayed = HRTIM_OUTPUTBURSTMODEENTRY_REGULAR;
  if (HAL_HRTIM_WaveformOutputConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_OUTPUT_TA1, &pOutputCfg) != HAL_OK)
  {
    Error_Handler();
  }

  // TA2 (CNV): 
  // Set: CMP1 (3us) 或 CMP3 (300us)
  // Reset: CMP2 (3.5us) 或 PER (300.5us 周期结束)
  pOutputCfg.SetSource = HRTIM_OUTPUTSET_TIMCMP1 | HRTIM_OUTPUTSET_TIMCMP3;
  pOutputCfg.ResetSource = HRTIM_OUTPUTRESET_TIMCMP2 | HRTIM_OUTPUTRESET_TIMPER;
  if (HAL_HRTIM_WaveformOutputConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_OUTPUT_TA2, &pOutputCfg) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_HRTIM_MspPostInit(&hhrtim1);
}

void HAL_HRTIM_MspPostInit(HRTIM_HandleTypeDef* hrtimHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(hrtimHandle->Instance==HRTIM1)
  {
  /* USER CODE BEGIN HRTIM1_MspPostInit 0 */

  /* USER CODE END HRTIM1_MspPostInit 0 */

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**HRTIM1 GPIO Configuration
    PA8     ------> HRTIM1_CHA1
    PA9     ------> HRTIM1_CHA2
    */
    GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF13_HRTIM1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN HRTIM1_MspPostInit 1 */

  /* USER CODE END HRTIM1_MspPostInit 1 */
  }

}

void HAL_HRTIM_MspDeInit(HRTIM_HandleTypeDef* hrtimHandle)
{

  if(hrtimHandle->Instance==HRTIM1)
  {
  /* USER CODE BEGIN HRTIM1_MspDeInit 0 */

  /* USER CODE END HRTIM1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_HRTIM1_CLK_DISABLE();

    /* HRTIM1 interrupt Deinit */
    HAL_NVIC_DisableIRQ(HRTIM1_TIMA_IRQn);
  /* USER CODE BEGIN HRTIM1_MspDeInit 1 */

  /* USER CODE END HRTIM1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */
void HAL_HRTIM_MspInit(HRTIM_HandleTypeDef* hrtimHandle)
{
  if(hrtimHandle->Instance==HRTIM1)
  {
  /* USER CODE BEGIN HRTIM1_MspInit 0 */

  /* USER CODE END HRTIM1_MspInit 0 */
    /* Peripheral clock enable */
    __HAL_RCC_HRTIM1_CLK_ENABLE(); // <--- 关键：开启 HRTIM 时钟
  /* USER CODE BEGIN HRTIM1_MspInit 1 */

  /* USER CODE END HRTIM1_MspInit 1 */
  }
}
/* USER CODE END 1 */
