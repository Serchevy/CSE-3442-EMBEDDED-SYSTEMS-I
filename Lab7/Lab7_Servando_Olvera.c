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
#include "eeprom.h"

// Pin bit-bands
#define GREEN_LED   (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 3*4)))   // PF3

#define SENSOR      (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 2*4)))   // PF2
#define WATER       (*((volatile uint32_t *)(0x42000000 + (0x400063FC-0x40000000)*32 + 4*4)))   // PC4  M0PWM6
#define FOOD        (*((volatile uint32_t *)(0x42000000 + (0x400063FC-0x40000000)*32 + 5*4)))   // PC5  M0PWM7
#define DISH        (*((volatile uint32_t *)(0x42000000 + (0x400073FC-0x40000000)*32 + 1*4)))   // PD1
#define SPEAKER     (*((volatile uint32_t *)(0x42000000 + (0x400073FC-0x40000000)*32 + 0*4)))   // PD0


// PortF masks
#define GREEN_LED_MASK 8
#define C0_NEG_MASK 128     // 2^7
#define C0_POS_MASK 64      // 2^6

#define DISH_MASK 2         // 2^1
#define SENSOR_MASK 4       // 2^2
#define WATER_MASK 16       // 2^4
#define FOOD_MASK 32        // 2^5
#define SPEAKER_MASK 1      // 2^0

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

int free_timer = 0;
char str[100];

uint32_t EVENT_TO_RUN = 0;

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Initialize Hardware
void initHw()
{
    // Initialize system clock to 40 MHz
    initSystemClockTo40Mhz();

    // Enable clocks
    SYSCTL_RCGCPWM_R |= SYSCTL_RCGCPWM_R0;          // PWM

    SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_R1;      // Regulat timer 1 clock
    SYSCTL_RCGCWTIMER_R |= SYSCTL_RCGCWTIMER_R1;    // Wide timer 1 clock
    SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_R2;      // Regulat timer 2 clock

    SYSCTL_RCGCHIB_R |= SYSCTL_RCGCHIB_R0;          // Hibernation Clock

    SYSCTL_RCGCEEPROM_R |= SYSCTL_RCGCEEPROM_R0;    // Enable EEPROM clock

    SYSCTL_RCGCACMP_R |= SYSCTL_RCGCACMP_R0;        // Comparator 0

                            // Port C               Port D              Port F
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R2 | SYSCTL_RCGCGPIO_R3 | SYSCTL_RCGCGPIO_R5;

    _delay_cycles(3);

    // Real-time clock config
    while(~HIB_CTL_R & HIB_CTL_WRC);                   // Poll working bit
    HIB_CTL_R = HIB_CTL_CLK32EN;                       // Enable clock32

    while(~HIB_CTL_R & HIB_CTL_WRC);
    HIB_IM_R = HIB_IM_RTCALT0;                         // Alert Interrupt Mask

    while(~HIB_CTL_R & HIB_CTL_WRC);
    NVIC_EN1_R = 1 << (INT_HIBERNATE-16-32);            // Respective Interrup

    while(~HIB_CTL_R & HIB_CTL_WRC);
    HIB_CTL_R |= HIB_CTL_RTCEN;                         // Enable Real time clock

    // Configure LED pin
    GPIO_PORTF_DIR_R |= GREEN_LED_MASK;

    // Configure respective pins
    GPIO_PORTF_DIR_R &= ~SENSOR_MASK;

    //GPIO_PORTC_DIR_R |= WATER_MASK | FOOD_MASK;
    GPIO_PORTD_DIR_R |= SPEAKER_MASK | DISH_MASK;

    // Enable pins
    GPIO_PORTF_DEN_R |= SENSOR_MASK;

    GPIO_PORTD_DEN_R |= SPEAKER_MASK | DISH_MASK;

    GPIO_PORTF_DEN_R |= GREEN_LED_MASK;

    //PWM
    // PC4 M0PWM6  Gen3a
    // PC5 M0PWM7  Gen3b
    // ---------------------------------------------------
    GPIO_PORTC_DEN_R |= WATER_MASK | FOOD_MASK;                         // Dig Enable on motors
    GPIO_PORTC_AFSEL_R |= WATER_MASK | FOOD_MASK;                       // Alternative function
    GPIO_PORTC_PCTL_R &= ~(GPIO_PCTL_PC4_M | GPIO_PCTL_PC5_M);          //
    GPIO_PORTC_PCTL_R |= GPIO_PCTL_PC4_M0PWM6 | GPIO_PCTL_PC5_M0PWM7;   // PWM functionality

    SYSCTL_SRPWM_R = SYSCTL_SRPWM_R0;                // reset PWM0 module
    SYSCTL_SRPWM_R = 0;                              // leave reset state

    PWM0_3_CTL_R = 0;                                // turn-off PWM0 generator 3 (drives outs 6 and 7)

    PWM0_3_GENA_R |= PWM_0_GENA_ACTCMPAD_ONE | PWM_0_GENA_ACTLOAD_ZERO;  // output 6 on PWM0, gen 3a, cmpb
    PWM0_3_GENB_R |= PWM_0_GENB_ACTCMPBD_ONE | PWM_0_GENB_ACTLOAD_ZERO;  // output 7 on PWM0, gen 3b, cmpb

    PWM0_3_LOAD_R = 1024;

    PWM0_3_CMPA_R = 0;      // PC4 off (0=always low, 1023=always high)
    PWM0_3_CMPB_R = 0;      // PC5 off (0=always low, 1023=always high)

    PWM0_3_CTL_R = PWM_0_CTL_ENABLE;    // turn-on PWM0 generator 2

    PWM0_ENABLE_R |= PWM_ENABLE_PWM6EN | PWM_ENABLE_PWM7EN;

    // ---------------------------------------------------


    // Analog Comp
    GPIO_PORTC_DIR_R &= ~C0_NEG_MASK & ~C0_POS_MASK;    // Set as Inputs

    GPIO_PORTC_DEN_R &= ~C0_NEG_MASK & ~C0_POS_MASK;    // Dont't want digital input

    GPIO_PORTC_AFSEL_R |= C0_NEG_MASK | C0_POS_MASK;    // Enable alternative function
    GPIO_PORTC_AMSEL_R |= C0_NEG_MASK | C0_POS_MASK;    // Set as analog

    COMP_ACREFCTL_R |= COMP_ACREFCTL_EN | COMP_ACREFCTL_VREF_M | COMP_ACREFCTL_RNG;         // Enable Reference voltage and set its value (2.469V)
    COMP_ACCTL0_R   |= COMP_ACCTL0_ASRCP_REF | COMP_ACCTL0_CINV | COMP_ACCTL0_ISEN_RISE;    // Set COMP0 to use internal Vref, invert output, sense interrupt

    _delay_cycles(10);

    // Configure Timer 1
    TIMER1_CTL_R &= ~TIMER_CTL_TAEN;                                    // turn-off timer before reconfiguring
    TIMER1_CFG_R = TIMER_CFG_32_BIT_TIMER;                              // configure as 32-bit timer (A+B)
    TIMER1_TAMR_R |= TIMER_TAMR_TACDIR | TIMER_TAMR_TAMR_PERIOD;        // count up timer

    // Configure Wide Timer
    WTIMER1_CTL_R &= ~TIMER_CTL_TAEN;               // turn-off counter before reconfiguring
    WTIMER1_TAMR_R = TIMER_TAMR_TAMR_PERIOD;        // configure for periodic counter
    WTIMER1_TAILR_R = 400000000;                    // 10 seconds interrupt
    WTIMER1_IMR_R = TIMER_IMR_TATOIM;               // turn-on interrupts
    WTIMER1_CTL_R |= TIMER_CTL_TAEN;                // turn-on counter
    NVIC_EN3_R = 1 << (INT_WTIMER1A-16-96);         // turn-on interrupt 112 (WTIMER1A)
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
    free_timer = TIMER1_TAV_R;                                      // Get time from free running timer;

    TIMER1_CTL_R &= ~TIMER_CTL_TAEN;                                // Turn-off free running timer

    int32_t level = 50 * (int)(((float)free_timer-1980)/(50));      // Equation to get mL based on number of ticks

    if(level < 0) { level = 0;}                                     // Negative, make it 0

    snprintf(str, sizeof(str), "Water lever: ~%dmL\tTicks: %d\n", level , free_timer);
    putsUart0(str);

    COMP_ACMIS_R = COMP_ACMIS_IN0;                                  // Clear interrupt flag
}

int checkRTCC() {
    uint32_t time1 = HIB_RTCC_R;
    uint32_t sub_time = (HIB_RTCSS_R & HIB_RTCSS_RTCSSC_M);
    uint32_t time2 = HIB_RTCC_R;                                    // Obtain a valid read from RTCC

    if(time1 != time2) {
        putsUart0("ERROR: Invalid Read from RTC\n");
        return -1;
    }

    return time1;
}

void setAlarm() {

    uint32_t event_times[11];
    event_times[10] = 0xFFFFFFFF;                   // Junk Value
    uint32_t i;
    uint32_t num_events = 0;

    for(i = 0; i < 10; i++) {
        if(readEeprom( i * 16 + 0) != 0xFFFFFFFF) {
            uint32_t hrs = readEeprom( i * 16 + 3)*3600;
            uint32_t mins = readEeprom( i * 16 + 4)*60;
            event_times[i] = hrs + mins;
            num_events++;
        } else {
            event_times[i] = 0xFFFFFFFF;
        }
    }

    uint32_t real_time = checkRTCC();

    if((real_time % (24*3600)) > 0) { real_time %= (24*3600); }       // Current time in sense of 24 hr priod

    uint32_t next_event = 10;
    uint32_t event_today = 0;

    for(i = 0; i < 10; i++) {
        if((event_times[i] != 0xFFFFFFFF) && (event_times[i] > real_time) ) {
            event_today = 1;
        }
    }

    for(i = 0; i < 10; i++) {
        if(event_times[i] != 0xFFFFFFFF) {
            if (event_today) {
                if(event_times[i] < event_times[next_event] && event_times[i] > real_time ) {
                    next_event = i;
                }
            } else {
                if(event_times[i] < event_times[next_event]) {
                    next_event = i;
                }
            }
        }
    }

    real_time = checkRTCC();

    if(num_events == 0) {
        putsUart0("No events scheduled yet\n");
        HIB_RTCM0_R = 0xFFFFFFFF;                                                       // No events --> MATCH = -1;
    } else if(num_events > 0 && event_today == 0) {
        putsUart0("No events today\n");
        HIB_RTCM0_R = real_time + (86400 - real_time % 8600) + event_times[next_event]; // No event today. Calculate time till next day
    } else if(num_events > 0 && event_today == 1) {
        snprintf(str, sizeof(str), "Event %d scheduled later today\n", next_event);
        putsUart0(str);
        HIB_RTCM0_R = real_time - (real_time % 86400) + event_times[next_event];        // Event today. Calculate till match time
    }

    EVENT_TO_RUN = next_event;
}

void printSchedEvents() {
    int32_t data[10][5];
    uint32_t i;
    for(i = 0; i < 10; i++) {
        int j;
        for(j = 0; j < 5; j++) {
            data[i][j] = readEeprom( i * 16 + j);
        }
    }

    for(i = 0; i < 10; i++) {
        if(data[i][0] != 0xFFFFFFFF ) {
            snprintf(str, sizeof(str), "Event[%d] --> Dur:%d   PWM:%d   Hr:%d   Min:%d\n", data[i][0], data[i][1], data[i][2], data[i][3], data[i][4]);
            putsUart0(str);
        }
    }
}

void timer2Isr() {
    PWM0_3_CMPB_R = 0;

    setAlarm();

    TIMER2_ICR_R = TIMER_ICR_TATOCINT;              // Clear Flag
}

void hib0Isr() {
    GREEN_LED ^= 1;

    uint32_t pwm_raw = readEeprom( EVENT_TO_RUN * 16 + 2);

    uint32_t dur = readEeprom( EVENT_TO_RUN * 16 + 1) * 40000000;       // Turn secs into ticks. Valid Load
    uint32_t pwm = (((float)pwm_raw)/100) * 1023;                       // Duty cycle

    PWM0_3_CMPB_R = pwm;

    // One shot timer
    TIMER2_CTL_R &= ~TIMER_CTL_TAEN;                // turn-off timer before reconfiguring
    TIMER2_TAMR_R |= TIMER_TAMR_TAMR_1_SHOT;        // count down
    TIMER2_TAILR_R = dur;                           // Load value
    TIMER2_IMR_R = TIMER_IMR_TATOIM;                // turn-on interrupts
    TIMER2_CTL_R |= TIMER_CTL_TAEN;                 // turn-on timer
    NVIC_EN0_R = 1 << (INT_TIMER2A-16);             // turn-on interrupt 23

    HIB_IC_R = HIB_IC_RTCALT0;                      // Clear intr flag
}

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

int main(void)
{
    USER_DATA data;
    uint32_t event_data[5];

    // Initialize hardware
    initHw();
    initUart0();
    initEeprom();

    // Setup UART0 baud rate
    setUart0BaudRate(19200, 40e6);

    // Endless loop
    while(1) {

        bool valid = false;

        getsUart0(&data);
        parseFields(&data);

        // Set time HH:MM command
        if (isCommand(&data, "time", 2)) {
            uint32_t hrs = getFieldInteger(&data, 1);
            uint32_t mins = getFieldInteger(&data, 2);

            uint32_t hrs_in_secs  = hrs*3600;
            uint32_t mins_in_secs = mins*60;

            while (~HIB_CTL_R & HIB_CTL_WRC);   // Poll WRC bit

            HIB_RTCLD_R = hrs_in_secs + mins_in_secs;

            valid = true;
        }

        // Check time command
        if (isCommand(&data, "time", 0)) {

            uint32_t secs = checkRTCC();
            //snprintf(str, sizeof(str), "%d\n", secs);
            //putsUart0(str);

            uint32_t hrs = secs/3600;   // 3600 seconds = 1 hour
            if(hrs % 24 > 0) {
                hrs %= 24;
            }
            secs %= 3600;               // Remainder is minutes in seconds
            uint32_t mins = secs/60;    // 60 seconds = 1 minute

            snprintf(str, sizeof(str), "The time is %02d:%02d\n", hrs , mins);
            putsUart0(str);

            printSchedEvents();

            setAlarm();

            valid = true;
        }

        // feed FEEDING DURATION PWM HH:MM
        if (isCommand(&data, "feed", 5)) {
            event_data[0] = getFieldInteger(&data, 1);  // Feeding Event
            event_data[1] = getFieldInteger(&data, 2);  // Duration
            event_data[2] = getFieldInteger(&data, 3);  // PWM Speed
            event_data[3] = getFieldInteger(&data, 4);  // Hour
            event_data[4] = getFieldInteger(&data, 5);  // Minute

            if(event_data[0] > 9) {
                putsUart0("Error: Up to 10 events can be stored. [0-9.]\n");
                continue;
            }

            uint32_t i;
            for(i = 0; i < 5; i++) {
                writeEeprom((event_data[0] * 16 + i), event_data[i]);
            }

            valid = true;
        }

        // feeding FEED delete
        if(isCommand(&data, "feed", 2) && (strgcmp(getFieldString(&data, 2), "delete"))) {
            uint32_t event = getFieldInteger(&data, 1);

            uint32_t i;
            for(i = 0; i < 5; i++) {
                writeEeprom((event * 16 + i), 0xFFFFFFFF);
            }

            valid = true;
        }


        if (!valid) {
            putsUart0("Invalid command\n");
        }
    }
}
