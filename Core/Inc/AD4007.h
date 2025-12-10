#ifndef __AD4007_H__
#define __AD4007_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

// 定义基准电压 (V)
#define AD4007_VREF 4.096f

// AD4007 命令定义
#define AD4007_CMD_WRITE_CFG  0x54  // 写配置寄存器

// 函数声明
/* AD4007_GPIO_Init is an internal helper — define it as static in AD4007.c
   to keep it private to the implementation and avoid unused/undefined
   static declarations in headers. */
HAL_StatusTypeDef AD4007_Init_Safe(void); // 改名为 Safe，强调无死循环
int32_t AD4007_Read_Single(void);
float AD4007_ConvertToVoltage(int32_t code);
int32_t AD4007_Read_SPI_Only(void);

#ifdef __cplusplus
}
#endif

#endif /* __AD4007_H__ */