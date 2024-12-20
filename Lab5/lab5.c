#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "clock.h"
#include "wait.h"
#include "tm4c123gh6pm.h"


#define SENSOR  (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 2*4)))   // PF2

#define WATER   (*((volatile uint32_t *)(0x42000000 + (0x400063FC-0x40000000)*32 + 4*4)))   // PC4
#define FOOD    (*((volatile uint32_t *)(0x42000000 + (0x400063FC-0x40000000)*32 + 5*4)))   // PC5
#define SPEAKER (*((volatile uint32_t *)(0x42000000 + (0x400073FC-0x40000000)*32 + 0*4)))   // PD0
#define DISH    (*((volatile uint32_t *)(0x42000000 + (0x400073FC-0x40000000)*32 + 1*4)))   // PD1


#define SENSOR_MASK 4       // 2^2

#define WATER_MASK 16       // 2^4
#define FOOD_MASK 32        // 2^5
#define SPEAKER_MASK 1      // 2^0
#define DISH_MASK   2       // 2^1


void initHw() {
    // Initialize system clock to 40 MHz
    initSystemClockTo40Mhz();

    // Enable clocks
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R5 |SYSCTL_RCGCGPIO_R3 | SYSCTL_RCGCGPIO_R2;
    _delay_cycles(3);

    // Configure respective pins
    GPIO_PORTF_DIR_R &= ~SENSOR_MASK;

    GPIO_PORTC_DIR_R |= WATER_MASK | FOOD_MASK;
    GPIO_PORTD_DIR_R |= SPEAKER_MASK | DISH_MASK;

    // Enable pins
    GPIO_PORTF_DEN_R |= SENSOR_MASK;

    GPIO_PORTC_DEN_R |= WATER_MASK | FOOD_MASK;
    GPIO_PORTD_DEN_R |= SPEAKER_MASK | DISH_MASK;
}


int main(void) {

    initHw();

    WATER = 0;
    FOOD = 0;
    SPEAKER = 0;
    DISH = 0;

    FOOD = 1;

    waitMicrosecond(2000000);

    FOOD = 0;

    WATER = 1;

    waitMicrosecond(5000000);

    WATER = 0;

    while(true) {
        SPEAKER = 1;

        waitMicrosecond(185);

        SPEAKER = 0;

        waitMicrosecond(185);
    }

    return 0;
}
