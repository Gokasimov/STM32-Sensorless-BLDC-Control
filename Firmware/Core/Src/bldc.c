/*
 * bldc.c
 *
 *  Created on: Mar 8, 2026
 *      Author: fgok5
 */

#include "bldc.h"

float PI_KP = 0.35f;
float PI_KI = 0.025f;

volatile BLDC_Controller_t bldc = {0};

extern TIM_HandleTypeDef htim1;


void BLDC_Init(void) {
    bldc.state = BLDC_STATE_IDLE;
    bldc.current_step = BLDC_STEP_1;
    bldc.current_duty = 0;

    bldc.speed_sum = 0;
        for(int i=0; i<MAF_SIZE; i++) {
            bldc.speed_history[i] = 20;
            bldc.speed_sum += 20;
        }
        bldc.speed_index = 0;

    BLDC_SetDutyCycle(0);
}

void BLDC_SetDutyCycle(uint16_t duty) {
    if (duty > BLDC_MAX_DUTY_CYCLE) duty = BLDC_MAX_DUTY_CYCLE;

    bldc.current_duty = duty;


    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, bldc.current_duty);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, bldc.current_duty);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, bldc.current_duty);
}



// Injected ADC Interrupt Function
void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
    {
        bldc.adc_vdc = HAL_ADCEx_InjectedGetValue(hadc, BLDC_ADC_VDC_RANK);
        bldc.adc_vs1 = HAL_ADCEx_InjectedGetValue(hadc, BLDC_ADC_VS1_RANK);
        bldc.adc_vs2 = HAL_ADCEx_InjectedGetValue(hadc, BLDC_ADC_VS2_RANK);
        bldc.adc_vs3 = HAL_ADCEx_InjectedGetValue(hadc, BLDC_ADC_VS3_RANK);

        bldc.virtual_neutral = (bldc.adc_vs1 + bldc.adc_vs2 + bldc.adc_vs3) / 3;

        switch (bldc.state)
        {
            case BLDC_STATE_IDLE:
        	    BLDC_SetDutyCycle(0);
   	            TIM1->CCER &= ~(TIM_CCER_CC1E | TIM_CCER_CC1NE |
            	                TIM_CCER_CC2E | TIM_CCER_CC2NE |
            	                TIM_CCER_CC3E | TIM_CCER_CC3NE);
                break;

            case BLDC_STATE_ALIGN:
                // Rotor Align
            	// The ISR just waits for the mechanical rotor to align with the stator.
                break;

            case BLDC_STATE_RAMPUP:
            	bldc.timer_ticks++;

            	                if (bldc.timer_ticks >= bldc.commutation_delay)
            	                {
            	                    bldc.timer_ticks = 0;

            	                    BLDC_CommutationStep_t next_step = (bldc.current_step % 6) + 1;
            	                    BLDC_Commutate(next_step);

            	                    if (bldc.commutation_delay > 25) {
            	                        bldc.commutation_delay--;
            	                    }
            	                    else {

            	                        bldc.state = BLDC_STATE_CLOSED_LOOP;
            	                        bldc.blanking_counter = BLDC_ZCD_BLANKING_COUNT;
            	                    }
            	                }
                break;

            case BLDC_STATE_CLOSED_LOOP:
            	bldc.stall_counter++;
            	bldc.step_timer_ticks++;

            					if (bldc.stall_counter > 2000)
            	                {
            	                    bldc.state = BLDC_STATE_IDLE;
            	                    BLDC_SetDutyCycle(0);
            	                    bldc.stall_counter = 0;
            	                    break;
            	                }

            	                if (bldc.blanking_counter > 0) {
            	                    bldc.blanking_counter--;
            	                } else {
            	                    BLDC_CheckZCD();
            	                }
            	                bldc.pi_ready = 1;
                break;

            case BLDC_STATE_FAULT:
                BLDC_SetDutyCycle(0);
                HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
                HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_2);
                HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_3);
                break;
        }
    }
}

// 6-Step Commutate

void BLDC_Commutate(BLDC_CommutationStep_t step)
{

	bldc.last_step_time = bldc.step_timer_ticks;
	bldc.step_timer_ticks = 0;

	bldc.current_step = step;
	bldc.stall_counter = 0;
    uint32_t blank = bldc.last_step_time / 4;
    bldc.blanking_counter = (uint16_t)((blank < 1) ? 1 : blank);

    TIM1->CCER &= ~(TIM_CCER_CC1E | TIM_CCER_CC1NE |
                    TIM_CCER_CC2E | TIM_CCER_CC2NE |
                    TIM_CCER_CC3E | TIM_CCER_CC3NE);

    switch (step)
    {
        case BLDC_STEP_1:
            // Faz A: PWM (High), Faz B: GND (Low), Faz C: Float
            TIM1->CCR1 = bldc.current_duty;
            TIM1->CCR2 = 0;
            TIM1->CCER |= (TIM_CCER_CC1E | TIM_CCER_CC1NE | TIM_CCER_CC2E | TIM_CCER_CC2NE);
            break;

        case BLDC_STEP_2:
            // Faz A: PWM (High), Faz C: GND (Low), Faz B: Float
            TIM1->CCR1 = bldc.current_duty;
            TIM1->CCR3 = 0;
            TIM1->CCER |= (TIM_CCER_CC1E | TIM_CCER_CC1NE | TIM_CCER_CC3E | TIM_CCER_CC3NE);
            break;

        case BLDC_STEP_3:
            // Faz B: PWM (High), Faz C: GND (Low), Faz A: Float
            TIM1->CCR2 = bldc.current_duty;
            TIM1->CCR3 = 0;
            TIM1->CCER |= (TIM_CCER_CC2E | TIM_CCER_CC2NE | TIM_CCER_CC3E | TIM_CCER_CC3NE);
            break;

        case BLDC_STEP_4:
            // Faz B: PWM (High), Faz A: GND (Low), Faz C: Float
            TIM1->CCR2 = bldc.current_duty;
            TIM1->CCR1 = 0;
            TIM1->CCER |= (TIM_CCER_CC2E | TIM_CCER_CC2NE | TIM_CCER_CC1E | TIM_CCER_CC1NE);
            break;

        case BLDC_STEP_5:
            // Faz C: PWM (High), Faz A: GND (Low), Faz B: Float
            TIM1->CCR3 = bldc.current_duty;
            TIM1->CCR1 = 0;
            TIM1->CCER |= (TIM_CCER_CC3E | TIM_CCER_CC3NE | TIM_CCER_CC1E | TIM_CCER_CC1NE);
            break;

        case BLDC_STEP_6:
            // Faz C: PWM (High), Faz B: GND (Low), Faz A: Float
            TIM1->CCR3 = bldc.current_duty;
            TIM1->CCR2 = 0;
            TIM1->CCER |= (TIM_CCER_CC3E | TIM_CCER_CC3NE | TIM_CCER_CC2E | TIM_CCER_CC2NE);
            break;
    }

    //COM (Commutation) Event trigger
    TIM1->EGR |= TIM_EGR_COMG;
}

// ZCD Function
void BLDC_CheckZCD(void)
{
    uint8_t zero_crossing_event = 0;

    switch (bldc.current_step)
    {
        case BLDC_STEP_1:
            // Faz C Float, BEMF Falling
            if (bldc.adc_vs3 < bldc.virtual_neutral) zero_crossing_event = 1;
            break;

        case BLDC_STEP_2:
            // Faz B Float, BEMF Rising
            if (bldc.adc_vs2 > bldc.virtual_neutral) zero_crossing_event = 1;
            break;

        case BLDC_STEP_3:
            // Faz A Float, BEMF Falling
            if (bldc.adc_vs1 < bldc.virtual_neutral) zero_crossing_event = 1;
            break;

        case BLDC_STEP_4:
            // Faz C Float, BEMF Rising
            if (bldc.adc_vs3 > bldc.virtual_neutral) zero_crossing_event = 1;
            break;

        case BLDC_STEP_5:
            // Faz B Float, BEMFFalling
            if (bldc.adc_vs2 < bldc.virtual_neutral) zero_crossing_event = 1;
            break;

        case BLDC_STEP_6:
            // Faz A Float, BEMF Rising
            if (bldc.adc_vs1 > bldc.virtual_neutral) zero_crossing_event = 1;
            break;
    }

    if (zero_crossing_event)
    {
        // Note: At ideal system there should be 30 degree delay.
    	// But our card has RC filters at ADC legs.
    	// I trusted them.

        BLDC_CommutationStep_t next_step = (bldc.current_step % 6) + 1;
        BLDC_Commutate(next_step);
   }
}


void BLDC_Speed_PI_Controller(void)
{
	if (bldc.state != BLDC_STATE_CLOSED_LOOP) return;

	    // MAF
	    bldc.speed_sum -= bldc.speed_history[bldc.speed_index];
	    bldc.speed_history[bldc.speed_index] = bldc.last_step_time;
	    bldc.speed_sum += bldc.speed_history[bldc.speed_index];

	    bldc.speed_index = (bldc.speed_index + 1) % MAF_SIZE;

	    bldc.target_speed_sum = bldc.target_speed * MAF_SIZE;

	    // Each step = 1/6 electrical revolution
	    // step_time in ticks, each tick = 50us
	    // RPM = 60 / (6 * step_time * 50e-6 * pole_pairs)
	    // With 7 pole pairs: RPM = 60 / (6 * ticks * 50e-6 * 7)
	    // Simplified: RPM = 28571 / average_tick

	    if (bldc.speed_sum > 0) {
	        uint32_t avg_tick = bldc.speed_sum / MAF_SIZE;
	        bldc.actual_rpm = 28571 / avg_tick;
	    } else {
	        bldc.actual_rpm = 0;
	    }

	    bldc.pi_error = (float)(bldc.speed_sum) - (float)(bldc.target_speed_sum);

	    bldc.pi_integral += (bldc.pi_error * PI_KI);
	    if (bldc.pi_integral > BLDC_MAX_DUTY_CYCLE) bldc.pi_integral = BLDC_MAX_DUTY_CYCLE;
	    if (bldc.pi_integral < 0.0f) bldc.pi_integral = 0.0f;

	    bldc.pi_output = (bldc.pi_error * PI_KP) + bldc.pi_integral;

	    if (bldc.pi_output > BLDC_MAX_DUTY_CYCLE) bldc.pi_output = BLDC_MAX_DUTY_CYCLE;
	    if (bldc.pi_output < BLDC_MIN_DUTY_CYCLE) bldc.pi_output = BLDC_MIN_DUTY_CYCLE;

	    BLDC_SetDutyCycle((uint16_t)bldc.pi_output);
}
