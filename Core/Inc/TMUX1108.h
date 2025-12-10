/*
 * TMUX1108.h
 *
 * Created on: Dec 10, 2025
 * Author: User
 */

#ifndef INC_TMUX1108_H_
#define INC_TMUX1108_H_

#include "main.h"

// 通道定义 (对应数据手册真值表)
// S1: 000, S2: 001, ..., S8: 111
typedef enum {
    TMUX_CH_S1 = 0, // A2=0, A1=0, A0=0
    TMUX_CH_S2 = 1, // A2=0, A1=0, A0=1
    TMUX_CH_S3 = 2, // A2=0, A1=1, A0=0
    TMUX_CH_S4 = 3, // A2=0, A1=1, A0=1
    TMUX_CH_S5 = 4, // A2=1, A1=0, A0=0
    TMUX_CH_S6 = 5, // A2=1, A1=0, A0=1
    TMUX_CH_S7 = 6, // A2=1, A1=1, A0=0
    TMUX_CH_S8 = 7, // A2=1, A1=1, A0=1
    TMUX_CH_NONE = 0xFF // 所有开关断开 (仅用于带EN引脚的设备)
} TMUX_Channel_t;

// 函数声明
void TMUX_Global_Init(void);
void TMUX_KB_SetChannel(TMUX_Channel_t channel); // 控制 Bridge Mux
void TMUX_KH_SetChannel(TMUX_Channel_t channel); // 控制 High Mux
void TMUX_KL_SetChannel(TMUX_Channel_t channel); // 控制 Low Mux

#endif /* INC_TMUX1108_H_ */