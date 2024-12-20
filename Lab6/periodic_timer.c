// Periodic timer example
// Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

// Hardware configuration:
// Green LED:
//   PF3 drives an NPN transistor that powers the green LED

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "clock.h"
#include "tm4c123gh6pm.h"
#include "wait.h"
#include "uart0.h"
#include "string.h"

// Pin bitbands
#define GREEN_LED   (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 3*4)))   // PF3
#define DISH        (*((volatile uint32_t *)(0x42000000 + (0x400073FC-0x40000000)*32 + 1*4)))   // PD1
//#define C0_POS      (*((volatile uint32_t *)(0x42000000 + (0x400063FC-0x40000000)*32 + 6*4)))   // PC6
//#define C0_NEG      (*((volatile uint32_t *)(0x42000000 + (0x400063FC-0x40000000)*32 + 7*4)))   // PC7
//#define C0o         (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 0*4)))   // PF0 (output of comparator)


// PortF masks
#define GREEN_LED_MASK 8
#define DISH_MASK 2         // 2^1
#define C0_NEG_MASK 128     // 2^7
#define C0_POS_MASK 64      // 2^6

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

int free_timer = 0;
char str[100];

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Initialize Hardware
void initHw()
{
    // Initialize system clock to 40 MHz
    initSystemClockTo40Mhz();

    // Enable clocks
    SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_R1;
    SYSCTL_RCGCWTIMER_R |= SYSCTL_RCGCWTIMER_R1;

    SYSCTL_RCGCACMP_R |= SYSCTL_RCGCACMP_R0;       // Comparator 0

                            // Port C               Port D              Port F
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R2 | SYSCTL_RCGCGPIO_R3 | SYSCTL_RCGCGPIO_R5;


    _delay_cycles(3);

    // Configure LED pin
    GPIO_PORTF_DIR_R |= GREEN_LED_MASK;
    GPIO_PORTD_DIR_R |= DISH_MASK;

    GPIO_PORTC_DIR_R &= ~C0_NEG_MASK & ~C0_POS_MASK;    // Set as Inputs

    GPIO_PORTF_DEN_R |= GREEN_LED_MASK;
    GPIO_PORTD_DEN_R |= DISH_MASK;

    GPIO_PORTC_DEN_R &= ~C0_NEG_MASK & ~C0_POS_MASK;    // Dont't want digital input

    GPIO_PORTC_AFSEL_R |= C0_NEG_MASK | C0_POS_MASK;    // Enable alternative function
    GPIO_PORTC_AMSEL_R |= C0_NEG_MASK | C0_POS_MASK;    // Set as analog

    COMP_ACREFCTL_R |= COMP_ACREFCTL_EN | COMP_ACREFCTL_VREF_M | COMP_ACREFCTL_RNG;         // Enable Reference voltage and set its value (2.469V)
    COMP_ACCTL0_R   |= COMP_ACCTL0_ASRCP_REF | COMP_ACCTL0_CINV | COMP_ACCTL0_ISEN_RISE;    // Set COMP0 to use internal Vref, invert output, sense interrupt

    _delay_cycles(10);

    //COMP_ACINTEN_R  |= COMP_ACINTEN_IN0;                    // Enable interrupt
    //NVIC_EN0_R = 1 << (INT_COMP0-16);                       // Turn on interrupt 41


    // Configure Timer 1
    TIMER1_CTL_R &= ~TIMER_CTL_TAEN;                                    // turn-off timer before reconfiguring
    TIMER1_CFG_R = TIMER_CFG_32_BIT_TIMER;                              // configure as 32-bit timer (A+B)
    TIMER1_TAMR_R |= TIMER_TAMR_TACDIR | TIMER_TAMR_TAMR_PERIOD;        // count up timer
    //TIMER1_CTL_R |= TIMER_CTL_TAEN;                                     // turn-on timer

    // Configure Wide Timer
    WTIMER1_CTL_R &= ~TIMER_CTL_TAEN;               // turn-off counter before reconfiguring
    WTIMER1_TAMR_R = TIMER_TAMR_TAMR_PERIOD;         // configure for periodic counter
    WTIMER1_TAILR_R = 400000000;                     // 10 seconds interrupt
    WTIMER1_IMR_R = TIMER_IMR_TATOIM;                // turn-on interrupts
    WTIMER1_CTL_R |= TIMER_CTL_TAEN;                 // turn-on counter
    NVIC_EN3_R = 1 << (INT_WTIMER1A-16-96);          // turn-on interrupt 112 (WTIMER1A)
}

void timer1Isr(void) {

}

void wideTimer1Isr() {
    DISH = 1;
    waitMicrosecond(10);
    DISH = 0;

    TIMER1_TAV_R = 0;                               // zero counter for free running timer
    TIMER1_CTL_R |= TIMER_CTL_TAEN;                 // turn-on timer

    GREEN_LED ^= 1;
    WTIMER1_ICR_R = TIMER_ICR_TATOCINT;             // clear interrupt flag

    COMP_ACINTEN_R  |= COMP_ACINTEN_IN0;            // Enable interrupt
    NVIC_EN0_R = 1 << (INT_COMP0-16);               // Turn on interrupt 41
}

void comprt0Isr() {
    free_timer = TIMER1_TAV_R;                        // Get time from free running timer;

    TIMER1_CTL_R &= ~TIMER_CTL_TAEN;            // Turn-off free running timer

    // Range attempt... not accurate and too annoying
//    if(free_timer < 1460) {
//        putsUart0("0 mL\n");
//    } else if(free_timer < 2150 && free_timer > 2050) {
//        putsUart0("Water level about 100 mL\n");
//    } else if(free_timer > 2150 && free_timer < 2250) {
//        putsUart0("Water level about 200 mL\n");
//    } else if(free_timer > 2250 && free_timer < 2350){
//        putsUart0("Water level about 300 mL\n");
//    } else if(free_timer > 2350 && free_timer < 2450){
//        putsUart0("Water level about 400 mL\n");
//    } else if(free_timer > 2450){
//        putsUart0("Water level above 400 mL\n");
//    }

    // Based on data collected
    // y ~ xm + b
    // m ~ 1
    // b ~ 1998

    // y = 50 floor((x-1998)/50)     Inverse Equation

    int32_t level = 50 * (int)(((float)free_timer-1980)/(50));      // Equation to get mL based on number of ticks

    if(level < 0) { level = 0;}                                     // Negative, make it 0

    snprintf(str, sizeof(str), "Water lever: ~%dmL\tTicks: %d\n", level , free_timer);
    putsUart0(str);

    //COMP_ACINTEN_R &= ~COMP_ACINTEN_IN0;        // Disable comparator
    COMP_ACMIS_R = COMP_ACMIS_IN0;              // Clear interrupt flag
}
//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

int main(void)
{
    // Initialize hardware
    initHw();
    initUart0();
    GREEN_LED = 0;

    // Setup UART0 baud rate
    setUart0BaudRate(19200, 40e6);


    // Endless loop
    while (true) {
        //char str1[100];
        //snprintf(str1, sizeof(str1), "Time: %d\n", time);
        //putsUart0(str1);
    }
}
