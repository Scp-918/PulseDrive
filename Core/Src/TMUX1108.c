/*
 * TMUX1108.c
 *
 * Created on: Dec 10, 2025
 * Author: User
 */

#include "TMUX1108.h"
#include "gpio.h" 

/**
  * @brief 初始化所有TMUX相关的GPIO管脚
  * [cite_start]根据连接表 [cite: 1690, 1691] 配置：
  * KB: EN(PA8), A0(PB15), A1(PC6), A2(PC7)
  * KH: A0(PB12), A1(PB13), A2(PB14)
  * KL: A0(PD8), A1(PD9), A2(PD10)
  */
void TMUX_Global_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // 开启时钟
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    // 1. 配置 KB (Bridge) EN 引脚 (PA8)
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET); // 默认关闭
    GPIO_InitStruct.Pin = GPIO_PIN_8;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // KB 的地址线 PB15, PC6, PC7 以及 KL 的地址线 PD8-10 
    // 通常在 gpio.c 的 MX_GPIO_Init 中已配置，此处可复用，无需重复初始化。
    
    // 2. 配置 KH (High) 相关引脚 (PB12-14)
    // 原计划可能为HRTIM复用功能，此处强制覆盖为GPIO输出以支持手动控制
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/**
  * @brief 设置 KB (Bridge) 多路开关
  * @param channel: 
  * TMUX_CH_S1 ~ S8: 设置地址线并拉高 EN (导通)
  * TMUX_CH_NONE: 拉低 EN (断开所有通道)
  */
void TMUX_KB_SetChannel(TMUX_Channel_t channel)
{
    // KB Enable Pin: PA8 [cite: 1691]
    // if (channel == TMUX_CH_NONE)
    // {
    //     HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET); // Disable (所有开关断开)
    //     return;
    // }

    // 设置地址线: S7对应 A2=1, A1=1, A0=0 [cite: 1172]
    // A0: PB15, A1: PC6, A2: PC7
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, (channel & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6,  (channel & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7,  (channel & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    // 使能芯片
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);   // Enable
}

/**
  * @brief 设置 KH (High) 多路开关 (无EN引脚控制，常开)
  * A0(PB12), A1(PB13), A2(PB14)
  */
void TMUX_KH_SetChannel(TMUX_Channel_t channel)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, (channel & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, (channel & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, (channel & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
  * @brief 设置 KL (Low) 多路开关 (无EN引脚控制，常开)
  * A0(PD8), A1(PD9), A2(PD10)
  */
void TMUX_KL_SetChannel(TMUX_Channel_t channel)
{
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_8,  (channel & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_9,  (channel & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_10, (channel & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}