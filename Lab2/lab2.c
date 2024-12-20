#include <clock.h>
#include <stdint.h>
#include <stdbool.h>
#include <wait.h>
#include "clock.h"
#include "tm4c123gh6pm.h"

// Bitband aliases
#define RED_LED     (*((volatile uint32_t *)(0x42000000 + (0x400063FC-0x40000000)*32 + 4*4)))     //PC4
#define GREEN_LED   (*((volatile uint32_t *)(0x42000000 + (0x400063FC-0x40000000)*32 + 5*4)))     //PC5
#define YELLOW_LED  (*((volatile uint32_t *)(0x42000000 + (0x400063FC-0x40000000)*32 + 6*4)))     //PC6
#define BLUE_LED    (*((volatile uint32_t *)(0x42000000 + (0x400063FC-0x40000000)*32 + 7*4)))     //PC7
 
#define PB1         (*((volatile uint32_t *)(0x42000000 + (0x400043FC-0x40000000)*32 + 5*4)))     //PA5
#define PB2         (*((volatile uint32_t *)(0x42000000 + (0x400043FC-0x40000000)*32 + 6*4)))     //PA6 

// PortC&A masks
#define RED_LED_MASK 16     //2^4
#define GREEN_LED_MASK 32   //2^5
#define YELLOW_LED_MASK 64  //2^6
#define BLUE_LED_MASK 128   //2^7

#define PB1_MASK 32         //2^5
#define PB2_MASK 64         //2^6

// Pull Down
void waitPb1Press(void) {
	while(!PB1);
}
// Pull UP
void waitPb2Press(void) {
    while(PB2);
}

// Initialize Hardware
void initHw(void) {
    // Initialize system clock to 40 MHz
    initSystemClockTo40Mhz();

    // Enable clocks
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R2 | SYSCTL_RCGCGPIO_R0;                           // Register R2 = C
    _delay_cycles(3);                                                                       // Register R0 = A

    // Configure LED and pushbutton pins
    GPIO_PORTC_DIR_R |= RED_LED_MASK | GREEN_LED_MASK | YELLOW_LED_MASK | BLUE_LED_MASK;    // bits 4, 5, 6, & 7 are outputs on PC

    GPIO_PORTA_DIR_R &= ~PB1_MASK & ~PB2_MASK;                                              // bit 5 & 6 are inputs on PA

    GPIO_PORTC_DR2R_R |= RED_LED_MASK | GREEN_LED_MASK | YELLOW_LED_MASK | BLUE_LED_MASK;   // set output pins to the 2mA maximum current

    GPIO_PORTC_DEN_R |= RED_LED_MASK | GREEN_LED_MASK | YELLOW_LED_MASK | BLUE_LED_MASK;    // enable LEDs from PC
    GPIO_PORTA_DEN_R |= PB1_MASK | PB2_MASK;                                                // enable pushbuttons from PA

    GPIO_PORTA_PDR_R |= PB1_MASK;                                                           // enable internal pull-down for PB1
    GPIO_PORTA_PUR_R |= PB2_MASK;                                                           // enable internal pull-up for PB2

}

int main(void) {
    // Initialize hardware
	initHw();

    // Turn off all LEDs
    RED_LED = 0;        // 1 = on
    GREEN_LED = 1;      // 0 = on
    YELLOW_LED = 1;     // 0 = on
    BLUE_LED = 0;       // 1 = on

    // Enable red LED
    RED_LED = 1;

    // Wait for SW2 to be pressed
    waitPb2Press();

    // Disable red LED and enable green LED
    RED_LED = 0;
    GREEN_LED = 0;

    // Wait 1 sec
    waitMicrosecond(1000000);

    // Enable blue LED
    BLUE_LED = 1;

    // Wait for SW1 to be pressed
    waitPb1Press();

    // Wait 500ms & Toggle yellow LED
    while(true) {
        waitMicrosecond(500000);
        YELLOW_LED ^= 1;
	}

    //while(true);
}
