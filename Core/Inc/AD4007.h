#ifndef __AD4007_H__
#define __AD4007_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

// 定义基准电压 (V)
#define AD4007_VREF 4.096f

/**
 * @brief 初始化 AD4007 控制引脚 (用于软件触发调试)
 * 注意：此函数会将 PA9 强制配置为 GPIO 输出模式，覆盖 HRTIM 配置
 */
void AD4007_Init_Manual(void);

/**
 * @brief 读取一次 AD4007 数据
 * @return int32_t 转换后的 18-bit 有符号数值
 */
int32_t AD4007_Read_Single(void);

/**
 * @brief 将 ADC 原始数值转换为电压值
 * @param code 原始 ADC 数值
 * @return float 计算出的电压 (V)
 */
float AD4007_ConvertToVoltage(int32_t code);

#ifdef __cplusplus
}
#endif

#endif /* __AD4007_H__ */