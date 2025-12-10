#include "AD4007.h"
#include "spi.h" 
#include <stdio.h>

extern SPI_HandleTypeDef hspi3;

/**
 * @brief 初始化 AD4007 引脚配置
 */
static void AD4007_GPIO_Init(void)
{
    // 重新初始化 PA9 (CNV) 为 GPIO 输出，防止被 HRTIM 占用
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();

    // 默认拉高 CNV (CNV High = Conversion Phase/Idle)
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET); 

    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/**
 * @brief 安全初始化 AD4007，带超时机制
 * @return HAL_OK: 成功, HAL_ERROR: 失败
 */
HAL_StatusTypeDef AD4007_Init_Safe(void)
{
    uint8_t cfg_write_cmd[2];
    uint8_t rx_buffer[3] = {0};
    uint8_t retry_count = 0;
    const uint8_t MAX_RETRIES = 200; // 最大尝试10次，防止死机

    // 1. 强制配置 GPIO
    AD4007_GPIO_Init();

    // 2. 唤醒/复位 (Dummy Conversion)
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
    // 这里的接收是为了清空之前的状态，不检查返回值
    HAL_SPI_Receive(&hspi3, rx_buffer, 3, 10);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
    HAL_Delay(1);

    // 3. 尝试握手 (写入 Status Enable 位)
    // 写入命令：0x54 (Write CFG) + 0x10 (Status Bits Enable)
    cfg_write_cmd[0] = AD4007_CMD_WRITE_CFG; 
    cfg_write_cmd[1] = 0x10; 

    // 循环尝试有限次
    for (retry_count = 0; retry_count < MAX_RETRIES; retry_count++)
    {
        // --- 写入配置 ---
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET); // CNV Low
        if (HAL_SPI_Transmit(&hspi3, cfg_write_cmd, 2, 10) != HAL_OK)
        {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
            continue; // SPI 发送失败，重试
        }
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET); // CNV High (Latch)

        HAL_Delay(1); // 等待转换/生效

        // --- 读取验证 ---
        // 启动转换
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
        // 简单延时
        for(volatile int i=0; i<100; i++) { __NOP(); }

        // 读取数据
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
        if (HAL_SPI_Receive(&hspi3, rx_buffer, 3, 10) == HAL_OK)
        {
            // 如果读到的数据不是全0或全FF，认为芯片有响应
            // (AD4007 如果未连接，MISO通常被拉高读到0xFF，或悬空读到0x00)
            if (rx_buffer[0] != 0xFF && rx_buffer[0] != 0x00)
            {
                HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
                return HAL_OK; // 握手成功
            }
        }
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
        
        HAL_Delay(10); // 稍作延时再试
    }

    return HAL_ERROR; // 超过次数仍未成功，返回错误但不卡死
}

int32_t AD4007_Read_Single(void)
{
    uint8_t rx_data[3] = {0};
    int32_t adc_val = 0;

    // 1. 启动转换
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET); 
    
    // 2. 转换延时 (t_conv max 320ns)
    // 144MHz下 100个NOP足够安全
    for(volatile int i=0; i<1000; i++) { __NOP(); } 

    // 3. 读取数据
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
    if (HAL_SPI_Receive(&hspi3, rx_data, 3, 5) != HAL_OK)
    {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
        return 0;
    }
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);

    // 4. 数据处理 (18-bit 补码)
    // AD4007 SPI模式读取：MSB在前
    // 假设读取3字节：[D17-D10], [D9-D2], [D1-D0, 0,0,0,0,0,0]
    uint32_t raw = ((uint32_t)rx_data[0] << 16) | ((uint32_t)rx_data[1] << 8) | rx_data[2];
    
    // 向右移位对齐 (根据实际读取位数调整，如果读24位，数据在高18位)
    // 0xAA BB CC -> AABBCC
    // D17在bit23, D0在bit6
    raw = raw >> 6; 
    
    // 掩码保留低18位
    raw &= 0x3FFFF;

    // 符号扩展 (18位转32位有符号)
    if (raw & 0x20000) // 检查Bit 17
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
    // V = (Code * Vref) / 2^17 (因差分输入，满量程为 2^18，但公式通常简化)
    // 范围: -131072 ~ +131071 对应 -Vref ~ +Vref
    return ((float)code * AD4007_VREF) / 131072.0f;
}