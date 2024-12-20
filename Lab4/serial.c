// Serial Example
// Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL Evaluation Board
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

// Hardware configuration:
// Red LED:
//   PF1 drives an NPN transistor that powers the red LED
// Green LED:
//   PF3 drives an NPN transistor that powers the green LED
// UART Interface:
//   U0TX (PA1) and U0RX (PA0) are connected to the 2nd controller
//   The USB on the 2nd controller enumerates to an ICDI interface and a virtual COM port
//   Configured to 115,200 baud, 8N1

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "clock.h"
#include "uart0.h"
#include "tm4c123gh6pm.h"

#define DEBUG

#define MAX_CHARS 80
#define MAX_FIELDS 5

typedef struct _USER_DATA {
    char buffer[MAX_CHARS+1];
    uint8_t fieldCount;
    uint8_t fieldPosition[MAX_FIELDS];
    char fieldType[MAX_FIELDS];
} USER_DATA;


// Bitband aliases
#define RED_LED      (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 1*4)))
#define GREEN_LED    (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 3*4)))

// PortF masks
#define GREEN_LED_MASK 8
#define RED_LED_MASK 2

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Initialize Hardware
void initHw()
{
    // Initialize system clock to 40 MHz
    initSystemClockTo40Mhz();

    // Enable clocks
    SYSCTL_RCGCGPIO_R = SYSCTL_RCGCGPIO_R5;
    _delay_cycles(3);

    // Configure LED pins
    GPIO_PORTF_DIR_R |= GREEN_LED_MASK | RED_LED_MASK;  // bits 1 and 3 are outputs
    GPIO_PORTF_DR2R_R |= GREEN_LED_MASK | RED_LED_MASK; // set drive strength to 2mA (not needed since default configuration -- for clarity)
    GPIO_PORTF_DEN_R |= GREEN_LED_MASK | RED_LED_MASK;  // enable LEDs
}


void getsUart0(USER_DATA *data) {
    int count = 0;

    while(1) {

        char c = getcUart0();

        if( (c == 8 || c == 127 ) && (count > 0) ) {    // if backspace decrement count
            count--;
        } else if ( c == 13 ) {                         // if new line (Enter key) is entered, return
            data->buffer[count] = 0;
            return;
        } else if( c >= 32) {                           // Any printable characters, keep in buffer
            data->buffer[count] = c;
            count++;

            if(count>=MAX_CHARS) {
                data->buffer[count] = 0;
                return;
            }
        }
    }
}


void parseFields(USER_DATA *data) {
    // Numeric 48-57
    // Alpha 65-90 and 97-122
    // Everything else is a delimiter

    char prev = 0;                              // assume previous char is delimiter
    int i;
    data->fieldCount = 0;

    for(i = 0; i<MAX_CHARS; i++) {

        char curr = data->buffer[i];

        if( !((prev>=65 && prev<=90) || (prev>=97 && prev<=122)) ) {            // Check if previous char IS a delimiter
            if( (curr>=65 && curr<=90) || (curr>=97 && curr<=122) ) {           // Check if current char is NOT delimiter
                data->fieldType[data->fieldCount] = 'a';                        // if so, there is a transition to a word
                data->fieldPosition[data->fieldCount] = i;
                data->fieldCount++;
            }
        }

        if(!(prev>=48 && prev<=57)) {                                       // Check if previous char is delimiter
            if(curr>=48 && curr<=57) {                                          // Check if current char is NOT delimiter
                data->fieldType[data->fieldCount] = 'n';                        // if so, there is a transition to a letter
                data->fieldPosition[data->fieldCount] = i;
                data->fieldCount++;
            }
        }

        prev = curr;

        if(data->fieldCount >= MAX_FIELDS ) {
            return;
        }
    }

    for(i = 0; i<MAX_CHARS; i++) {
        if( !((data->buffer[i]>=65 && data->buffer[i]<=90) || (data->buffer[i]>=97 && data->buffer[i]<=122)) ) {    // if its not a letter
            if( !(data->buffer[i]>=48 && data->buffer[i]<=57) ) {                                                   // and not a number
                data->buffer[i] = 0;                                                                                // set it to NULL
            }
        }
    }

    return;
}

char* getFieldString(USER_DATA* data, uint8_t fieldNumber) {

    if(fieldNumber < data->fieldCount) {
        return &data->buffer[data->fieldPosition[fieldNumber]];
    } else {
        return NULL;
    }
}

int32_t getFieldInteger(USER_DATA* data, uint8_t fieldNumber) {

    int32_t num = 0;

    if(fieldNumber < data->fieldCount) {
        if((data->fieldType[fieldNumber]) == 'n') {
            int temp = 0;
            int i = 0 ;
            char *c = &data->buffer[data->fieldPosition[fieldNumber]];

            while (c[i] != 0) {
                temp = c[i] - 48;

                if(i) {
                    num = num*10;
                }

                num += temp;
                i++;
            }
            return num;
        }
    }

    return num;
}

bool isCommand(USER_DATA* data, const char strCommand[],uint8_t minArguments) {

    if(data->buffer[0] == 0) { return 0; }

    int i = 0;

    while (data->buffer[i] != 0 && strCommand[i] != 0) {
        if( !(strCommand[i] == data->buffer[i]) ) {                 // While not at the end of both strings compare chars
            return 0;                                               // Mismatch, return 0;
        }
        i++;
    }

    if (data->fieldCount-1 >= minArguments) {
        return 1;
    }

    return 0;
}

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

int main(void)
{
    USER_DATA data;

	// Initialize hardware
    initHw();
    initUart0();

    while(1) {

        bool valid = false;

        // Get the string from the user
        getsUart0(&data);

        // Echo back to the user of the TTY interface for testing
        #ifdef DEBUG
        putsUart0(data.buffer);
        putcUart0('\n');
        #endif

        // Parse fields
        parseFields(&data);

        // Echo back the parsed field data (type and fields)
        #ifdef DEBUG
        uint8_t i;
        for (i = 0; i < data.fieldCount; i++) {
            putcUart0(data.fieldType[i]);
            putcUart0('\n');
            putsUart0(&data.buffer[data.fieldPosition[i]]);
            putcUart0('\n');
        }
        #endif

        // Command evaluation
        // set add, data -> add and data are integers
        if (isCommand(&data, "set", 2)) {
            int32_t add = getFieldInteger(&data, 1);
            int32_t data_1 = getFieldInteger(&data, 2);
            valid = true;
            // do something with this information
        }

        // alert ON|OFF -> alert ON or alert OFF are the expected commands
        if (isCommand(&data, "alert", 1)) {
            char* str = getFieldString(&data, 1);
            putsUart0(str);
            valid = true;
            // process the string with your custom strcmp
            //instruction, then do something
        }

        // Process other commands here
        // Look for error
        if (!valid) {
            putsUart0("Invalid command\n");
        }
    }

//    while(1) {
//        getsUart0(&data);
//        putsUart0(data.buffer);
//        parseFields(&data);
//    }


}
