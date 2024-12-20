#include <clock.h>
#include <stdint.h>
#include <stdbool.h>
#include "tm4c123gh6pm.h"

// Bitband
#define GREEN_LED (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 3*4)))

// Mask
#define GREEN_LED_MASK 8

void wait3seconds(void) {

    __asm("N .field 17142857                              ");   // N = 17,142,857
    __asm("        LDR R1, N       ;  1                   ");
    __asm("LOOP:   SUB R1, R1, #1  ;  N                   ");
    __asm("        CBZ R1, END     ; (N-1) + 3            ");   // No branch(N-1), branch(3)
    __asm("        NOP             ; (N-1)                ");
    __asm("        NOP             ; (N-1)                ");
    __asm("        NOP             ; (N-1)                ");   // No conditional
    __asm("        B LOOP          ; 2(N-1)               ");   // branch 2(N-1)
    __asm("END:    BX LR           ; ------               ");
    __asm("                        ; 7N-2 = 120,000,000   ");
}

void initHw() {
    // Initialize system clock to 40 MHz
    initSystemClockTo40Mhz();

    // Enable clock
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R5;
    _delay_cycles(3);

    // Configure LED
    GPIO_PORTF_DIR_R |= GREEN_LED_MASK;     // Input
    GPIO_PORTF_DEN_R |= GREEN_LED_MASK;     // Enable LED
}

int main(void) {

    initHw();

    // Toggle green LED every 3 secs
    while(true)
    {
      GREEN_LED ^= 1;
      wait3seconds();
    }
}
