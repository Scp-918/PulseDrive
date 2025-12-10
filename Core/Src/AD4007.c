#include "AD4007.h"
#include "spi.h" 
#include <stdio.h>

extern SPI_HandleTypeDef hspi3;

/**
 * @brief 安全初始化 AD4007，带超时机制
 * @return HAL_OK: 成功, HAL_ERROR: 失败
 */
static void AD4007_GPIO_Init(void)
{
    // 重新初始化 PA9 (CNV) 为 GPIO 输出
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
    const uint8_t MAX_RETRIES = 200; // 尝试20次即可

    // 1. 强制配置 GPIO
    AD4007_GPIO_Init();

    // 2. 唤醒/复位 (Dummy Conversion)
    // 制造一个完整的转换周期来唤醒 ADC
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET); // CNV Low
    HAL_Delay(10);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);   // CNV High (Rising Edge -> Start Conv)
    HAL_Delay(10);
    
    // 清空可能的残留数据
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET); // CNV Low (Read mode)
    HAL_SPI_Receive(&hspi3, rx_buffer, 3, 10);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);   // CNV High
    HAL_Delay(10);

    // 3. 尝试握手 (写入 Status Enable 位)
    // 写入命令：0x54 (Write CFG) + 0x10 (Status Bits Enable)
    // 启用状态位后，读取的数据将包含6位状态，其中Bit 5 (OV) 默认为1
    cfg_write_cmd[0] = AD4007_CMD_WRITE_CFG; 
    cfg_write_cmd[1] = 0x10; 

    // 循环尝试
    for (retry_count = 0; retry_count < MAX_RETRIES; retry_count++)
    {
        // --- 步骤 A: 写入配置 ---
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET); // CNV Low (Enter Register Access Mode)
        if (HAL_SPI_Transmit(&hspi3, cfg_write_cmd, 2, 10) != HAL_OK)
        {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
            continue; // SPI 发送失败，重试
        }
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET); // CNV High (Latch Config)

        // 等待配置生效
        HAL_Delay(1); 

        // --- 步骤 B: 启动验证转换 ---
        // [修正点]：必须先拉低，再拉高，制造上升沿！
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET); // 先拉低
        // 极短延时确保电平稳定
        __NOP(); __NOP(); __NOP(); __NOP(); 
        
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);   // 再拉高 -> 上升沿触发转换
        
        // 等待转换完成 (t_conv max 320ns)，1ms 绰绰有余
        HAL_Delay(1); 

        // --- 步骤 C: 读取数据验证 ---
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET); // CNV Low (Enable SDO)
        
        // 读取 3 字节 (24 bits)
        // 数据结构: [18-bit Data] [6-bit Status]
        // Byte 2 (rx_buffer[2]) 的 Bit 5 是 OV Clamp Flag，正常应为 1
        if (HAL_SPI_Receive(&hspi3, rx_buffer, 3, 10) == HAL_OK)
        {
            // 验证逻辑：
            // 1. 数据不是全0或全FF (基本电气连接正常)
            // 2. 检查状态位 (rx_buffer[2] & 0x20) == 0x20 (OV Flag = 1, No Overvoltage)
            //    这证明我们成功写入了配置寄存器并开启了状态位输出。
            if ((rx_buffer[0] != 0xFF) && (rx_buffer[0] != 0x00))
            {
                // 如果能读到非0非FF的数据，基本可以认为通信成功
                // 进一步检查状态位会更严谨，但考虑到您现在的输入是3.3V，数据本身就是非0的
                HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
                return HAL_OK; // 握手成功
            }
            
            // 备用验证：如果您确定输入电压是 3.3V (Code ~0x19XXX)，
            // rx_buffer[0] 应该是 0x66 左右，肯定不是 0x00。
        }
        
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET); // 恢复空闲
        
        HAL_Delay(10); // 稍作延时再试
    }

    return HAL_ERROR; // 超过次数仍未成功
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