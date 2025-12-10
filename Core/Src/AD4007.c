#include "AD4007.h"
#include "spi.h" 
#include <stdio.h>

extern SPI_HandleTypeDef hspi3;

// 简单的微秒延时
static void AD4007_Delay_Short(void)
{
    for(volatile int i=0; i<1000; i++) { __NOP(); }
}

static void AD4007_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET); 

    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

HAL_StatusTypeDef AD4007_Init_Safe(void)
{
    uint8_t cfg_write_cmd[2];
    uint8_t rx_buffer[3] = {0};
    uint8_t tx_dummy[3] = {0xFF, 0xFF, 0xFF}; // 发送全1，确保 MOSI 为高
    uint8_t retry_count = 0;
    const uint8_t MAX_RETRIES = 20;

    AD4007_GPIO_Init();

    // 唤醒序列
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
    AD4007_Delay_Short();
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
    AD4007_Delay_Short();
    
    // 清空总线 (使用 TransmitReceive 发送 FF)
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(&hspi3, tx_dummy, rx_buffer, 3, 10); 
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET); // 此时 MOSI 为高，保持 CS 模式
    AD4007_Delay_Short();

    // 配置命令：Bit 0 必须为 1
    cfg_write_cmd[0] = AD4007_CMD_WRITE_CFG; 
    cfg_write_cmd[1] = 0x11; // 0x11: Status Enable + SDI Keep High

    for (retry_count = 0; retry_count < MAX_RETRIES; retry_count++)
    {
        // 1. 写入配置
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
        if (HAL_SPI_Transmit(&hspi3, cfg_write_cmd, 2, 10) != HAL_OK)
        {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
            continue;
        }
        // 关键：cfg_write_cmd[1] 最后一位是 1，CNV 上升时 SDI 为高 -> 锁定 CS 模式
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET); 

        AD4007_Delay_Short();

        // 2. 启动验证转换
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
        __NOP(); __NOP(); __NOP(); 
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
        
        AD4007_Delay_Short(); 

        // 3. 读取验证 (发送 0xFF)
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
        
        // 使用 TransmitReceive 替代 Receive
        if (HAL_SPI_TransmitReceive(&hspi3, tx_dummy, rx_buffer, 3, 10) == HAL_OK)
        {
            // 验证数据非空 (Status bit 5 应该为 1，即 rx_buffer[2] & 0x20)
            if ((rx_buffer[0] != 0xFF) && (rx_buffer[0] != 0x00))
            {
                // 结束读取，此时 MOSI 为高 (因为发了 0xFF)，保持 CS 模式
                HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
                return HAL_OK; 
            }
        }
        
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
        HAL_Delay(10);
    }

    return HAL_ERROR;
}

int32_t AD4007_Read_Single(void)
{
    uint8_t rx_data[3] = {0};
    uint8_t tx_dummy[3] = {0xFF, 0xFF, 0xFF}; // 关键：发送全1
    int32_t adc_val = 0;

    // 1. 启动转换 (CNV 上升沿)
    // 此时 MOSI 应该是高 (由上一次读取的 0xFF 保持)
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET); 
    
    // 2. 转换等待
    for(volatile int i=0; i<500; i++) { __NOP(); } 

    // 3. 读取数据
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
    
    // [修正] 使用 TransmitReceive 发送 0xFF，强迫 MOSI 拉高
    if (HAL_SPI_TransmitReceive(&hspi3, tx_dummy, rx_data, 3, 5) != HAL_OK)
    {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
        return 0;
    }
    
    // 4. 结束读取 (CNV 上升沿)
    // 因为刚才发送了 0xFF，此时 MOSI 为高。
    // AD4007 检测到 SDI=1，继续保持 CS 模式，下一次转换正常。
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);

    // 5. 数据处理
    uint32_t raw = ((uint32_t)rx_data[0] << 16) | ((uint32_t)rx_data[1] << 8) | rx_data[2];
    
    // 18位数据在高位，右移6位对齐 (假设读取24位，后6位为状态)
    raw = raw >> 6; 
    raw &= 0x3FFFF;

    if (raw & 0x20000) 
    {
        adc_val = raw | 0xFFFC0000;
    }
    else
    {
        adc_val = raw;
    }

    return adc_val;
}

float AD4007_ConvertToVoltage(int32_t code)
{
    return ((float)code * AD4007_VREF) / 131072.0f;
}

/* 读取函数：假设 CNV 已经由 HRTIM 触发并完成转换 */
int32_t AD4007_Read_SPI_Only(void)
{
    uint8_t rx_data[3] = {0};
    uint8_t tx_dummy[3] = {0xFF, 0xFF, 0xFF}; // 发送全1保持 MOSI 高电平 (SDI=1 -> CS Mode)
    int32_t adc_val = 0;

    // 直接进行 SPI 传输读取 24 位
    // 注意：这里不需要操作 GPIO_PIN_9 (CNV)，因为它正被 HRTIM 控制
    // 确保此时 CNV 为高（HRTIM 脉冲极短，读取时已经是高电平了）
    
    if (HAL_SPI_TransmitReceive(&hspi3, tx_dummy, rx_data, 3, 2) != HAL_OK)
    {
        return 0;
    }

    // 数据解码 (AD4007 18-bit, 补码)
    uint32_t raw = ((uint32_t)rx_data[0] << 16) | ((uint32_t)rx_data[1] << 8) | rx_data[2];
    
    // 对齐处理
    raw = raw >> 6; 
    raw &= 0x3FFFF;

    if (raw & 0x20000) 
    {
        adc_val = raw | 0xFFFC0000; // 符号扩展
    }
    else
    {
        adc_val = raw;
    }

    return adc_val;
}