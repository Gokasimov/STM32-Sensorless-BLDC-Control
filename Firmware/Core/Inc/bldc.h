/*
 * bldc.h
 *
 *  Created on: Mar 8, 2026
 *      Author: fgok5
 */

#ifndef INC_BLDC_H_
#define INC_BLDC_H_


#include "stm32g4xx_hal.h"

#include "main.h"

#include <stdint.h>

/* ========================================================================= */
/*                       Macros and Constants                                */
/* ========================================================================= */

// PWM
#define BLDC_PWM_PERIOD          3749  // TIM1 ARR
#define BLDC_MAX_DUTY_CYCLE      3560  // ~%95 Duty (Bootstrap Limit)
#define BLDC_MIN_DUTY_CYCLE      375   // ~%10 Duty (Minimum rate )

// ADC Injected Rank Indexes
#define BLDC_ADC_VDC_RANK        ADC_INJECTED_RANK_1
#define BLDC_ADC_VS1_RANK        ADC_INJECTED_RANK_2
#define BLDC_ADC_VS2_RANK        ADC_INJECTED_RANK_3
#define BLDC_ADC_VS3_RANK        ADC_INJECTED_RANK_4

// ZCD Constants
#define BLDC_ZCD_BLANKING_COUNT  5     // PWM cycle


#define MAF_SIZE 12

extern float PI_KP;
extern float PI_KI;


/* ========================================================================= */
/*                              Data Structures                              */
/* ========================================================================= */

// Motor States
typedef enum {
    BLDC_STATE_IDLE = 0,
    BLDC_STATE_ALIGN,
    BLDC_STATE_RAMPUP,
    BLDC_STATE_CLOSED_LOOP,
    BLDC_STATE_FAULT
} BLDC_State_t;

// 6-Step Comm
typedef enum {
    BLDC_STEP_1 = 1, // A-High, B-Low, C-Float (ZCD on C fall)
    BLDC_STEP_2 = 2, // A-High, C-Low, B-Float (ZCD on B rise)
    BLDC_STEP_3 = 3, // B-High, C-Low, A-Float (ZCD on A fall)
    BLDC_STEP_4 = 4, // B-High, A-Low, C-Float (ZCD on C rise)
    BLDC_STEP_5 = 5, // C-High, A-Low, B-Float (ZCD on B fall)
    BLDC_STEP_6 = 6  // C-High, B-Low, A-Float (ZCD on A rise)
} BLDC_CommutationStep_t;


typedef struct {
    BLDC_State_t           state;
    BLDC_CommutationStep_t current_step;


    // ADC
    uint16_t adc_vdc;
    uint16_t adc_vs1;
    uint16_t adc_vs2;
    uint16_t adc_vs3;
    uint16_t virtual_neutral;

    // Duty
    uint16_t current_duty;

    // ZCD
    uint16_t blanking_counter;

    // Timing
    uint32_t timer_ticks;
    uint32_t commutation_delay;


    uint32_t stall_counter;

    // Speed Meas
    uint32_t step_timer_ticks; // Time between two comm
    uint32_t last_step_time;
    uint32_t actual_speed;
    int32_t target_speed;

    // PI
    float pi_error;
    float pi_integral;
    float pi_output;
    uint32_t actual_rpm;
    volatile uint8_t pi_ready;

    // MAF
    uint32_t speed_history[MAF_SIZE];
    uint8_t  speed_index;
    uint32_t speed_sum;
    uint32_t target_speed_sum;

} BLDC_Controller_t;

/* ========================================================================= */
/*                            GLOBAL Variables                               */
/* ========================================================================= */
extern volatile BLDC_Controller_t bldc;

/* ========================================================================= */
/*                          Function Prototypes                              */
/* ========================================================================= */

void BLDC_Init(void);
void BLDC_SetDutyCycle(uint16_t duty);
void BLDC_Commutate(BLDC_CommutationStep_t step);
void BLDC_CheckZCD(void);
void BLDC_Speed_PI_Controller(void);

#endif /* INC_BLDC_H_ */
