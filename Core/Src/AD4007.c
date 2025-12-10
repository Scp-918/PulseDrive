#include "AD4007.h"
#include "spi.h" 

// 引用 main.c 或 spi.c 中定义的 SPI 句柄
extern SPI_HandleTypeDef hspi3;

/**
 * @brief 初始化 AD4007 控制引脚 (用于软件触发调试)
 */
void AD4007_Init_Manual(void)
{
    // 重新配置 PA9 为普通 GPIO 输出 (覆盖 CubeMX 生成的 HRTIM 功能)
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // 开启 GPIOA 时钟 (通常 main.c 已开启，但为了安全再次确认)
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP; // 推挽输出
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    // 默认拉低 CNV
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
}

/**
 * @brief 读取一次 AD4007 数据
 */
int32_t AD4007_Read_Single(void)
{
    uint8_t rx_data[3] = {0};
    int32_t adc_val = 0;

    // 1. 启动转换 (CNV Rising Edge)
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET); 
    
    // 2. 等待转换完成 (t_conv ~500ns)
    for(volatile int i=0; i<10; i++) { __NOP(); } 

    // 3. 拉低 CNV，准备读取数据 (CNV Low enables SDO)
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);

    // 4. 读取 SPI 数据 (3 Bytes = 24 bits)
    // 确保 hspi3 已经初始化
    if (HAL_SPI_Receive(&hspi3, rx_data, 3, 10) != HAL_OK)
    {
        return 0; // 读取失败返回 0
    }

    // 5. 数据拼接 (MSB First)
    // Data: [B0] [B1] [B2] -> 18 bits valid
    uint32_t raw = ((uint32_t)rx_data[0] << 16) | ((uint32_t)rx_data[1] << 8) | rx_data[2];
    
    // 右移 6 位，对齐到 18-bit LSB
    raw = raw >> 6;
    
    // 保留低 18 位
    raw &= 0x3FFFF;

    // 6. 符号扩展 (18-bit Two's Complement -> 32-bit Signed Int)
    if (raw & 0x20000) // 检查第 18 位 (符号位)
    {
        adc_val = raw | 0xFFFC0000; // 负数扩展
    }
    else
    {
        adc_val = raw;
    }

    return adc_val;
}

/**
 * @brief 将 ADC 原始数值转换为电压值
 */
float AD4007_ConvertToVoltage(int32_t code)
{
    // 计算公式: V = (Code * 2 * Vref) / 2^18
    // 假设是全差分驱动，范围 ±Vref
    return (float)code * (2.0f * AD4007_VREF) / 262144.0f;
}