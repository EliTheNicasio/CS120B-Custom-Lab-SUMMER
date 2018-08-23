/* Name & E-mail: Elijah Nicasio (enica001@ucr.edu)
 * Lab Section: B21
 * Assignment: Custom Project
 * Exercise Description: Custom Project
 *
 * I acknowledge all content contained herein, excluding template or example
 * code, is my own original work.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include "WS2812.h"
#include "ADC_H.h"

// Concurrent SM Stuff
typedef struct
{
	int state;
	unsigned long period;
	unsigned long elapsedTime;
	int (*TickFct)(int);
} Task;

const unsigned long tasksPeriod = 50;

#define tasksSize 1
Task tasks[tasksSize];

enum IN_States {IN_Start, IN_Cycle} IN_State;
int TickFct_Input(int state);

enum CN_States {CN_Start, CN_Idle, CN_Move, CN_MoveWait} CN_State;
int TickFct_Controller(int state);

enum LED_States {LED_Start, LED_Init, LED_Cycle} LED_State;
int TickFct_LED(int state);


// Synch SM Vars and Functions
#pragma region

volatile unsigned char TimerFlag = 0; // TimerISR() sets this to 1. C programmer should clear to 0.

// Internal variables for mapping AVR's ISR to our cleaner TimerISR model.
unsigned long _avr_timer_M = 1; // Start count from here, down to 0. Default 1 ms.
unsigned long _avr_timer_cntcurr = 0; // Current internal count of 1ms ticks

void TimerOn() {
	// AVR timer/counter controller register TCCR1
	TCCR1B = 0x0B;// bit3 = 0: CTC mode (clear timer on compare)
	// bit2bit1bit0=011: pre-scaler /64
	// 00001011: 0x0B
	// SO, 8 MHz clock or 8,000,000 /64 = 125,000 ticks/s
	// Thus, TCNT1 register will count at 125,000 ticks/s

	// AVR output compare register OCR1A.
	OCR1A = 125;	// Timer interrupt will be generated when TCNT1==OCR1A
	// We want a 1 ms tick. 0.001 s * 125,000 ticks/s = 125
	// So when TCNT1 register equals 125,
	// 1 ms has passed. Thus, we compare to 125.
	// AVR timer interrupt mask register
	TIMSK1 = 0x02; // bit1: OCIE1A -- enables compare match interrupt

	//Initialize avr counter
	TCNT1=0;

	_avr_timer_cntcurr = _avr_timer_M;
	// TimerISR will be called every _avr_timer_cntcurr milliseconds

	//Enable global interrupts
	SREG |= 0x80; // 0x80: 1000000
}

void TimerOff() {
	TCCR1B = 0x00; // bit3bit1bit0=000: timer off
}

void TimerISR() {
	unsigned char i;
	for(i = 0; i < tasksSize; ++i)
	{
		if(tasks[i].elapsedTime >= tasks[i].period)
		{
			tasks[i].state = tasks[i].TickFct(tasks[i].state);
			tasks[i].elapsedTime = 0;
		}
		tasks[i].elapsedTime += tasksPeriod;
	}
}

// In our approach, the C programmer does not touch this ISR, but rather TimerISR()
ISR(TIMER1_COMPA_vect) {
	// CPU automatically calls when TCNT1 == OCR1 (every 1 ms per TimerOn settings)
	_avr_timer_cntcurr--; // Count down to 0 rather than up to TOP
	if (_avr_timer_cntcurr == 0) { // results in a more efficient compare
		TimerISR(); // Call the ISR that the user uses
		_avr_timer_cntcurr = _avr_timer_M;
	}
}

// Set TimerISR() to tick every M ms
void TimerSet(unsigned long M) {
	_avr_timer_M = M;
	_avr_timer_cntcurr = _avr_timer_M;
}
#pragma endregion

typedef struct 
{
	unsigned char green;
	unsigned char red;
	unsigned char blue;
} LED;

LED LEDs[64]; 

int main(void)
{
	DDRA = 0x00; PORTA = 0xFF;
	DDRB = 0xFF; PORTB = 0x00;
	
	unsigned char i = 0;
	/*tasks[i].state = IN_Start;
	tasks[i].period = 50;
	tasks[i].elapsedTime = 0;
	tasks[i].TickFct = &TickFct_Input;
	i++;
	tasks[i].state = CN_Start;
	tasks[i].period = 50;
	tasks[i].elapsedTime = 0;
	tasks[i].TickFct = &TickFct_Controller;
	i++;*/
	tasks[i].state = LED_Start;
	tasks[i].period = 100;
	tasks[i].elapsedTime = 0;
	tasks[i].TickFct = &TickFct_LED;
	
	TimerSet(1);
	TimerOn();
	
	initMatrix();
	
	srand(time(0));
	
    while (1) 
    {
    }
}

// Input Reader

unsigned short ADC_X;
unsigned short ADC_Y;

#define analog_X 0
#define analog_Y 1

int TickFct_Input(int state)
{
	switch(state)
	{
		case IN_Start:
			state = IN_Cycle;
			break;
			
		case IN_Cycle:
			state = IN_Cycle;
			break;
			
		default:
			state = IN_Start;
			break;
	}
	switch(state)
	{
		case IN_Start:
			break;
			
		case IN_Cycle:
			ADC_X = ADC_Read(analog_X);
			ADC_Y = ADC_Read(analog_Y);
			break;
	}
	return state;
}

// Controller (Analog Stick)

enum Direction {North, NorthEast, East, SouthEast, South, SouthWest, West, NorthWest, INVALID} moveDir;

unsigned char stickMoved(unsigned short x, unsigned short y);
int getDirection(unsigned short x, unsigned short y);

int TickFct_Controller(int state)
{
	switch(state)
	{
		case CN_Start:
			state = CN_Idle;
			break;
			
		case CN_Idle:
			if(stickMoved(ADC_X, ADC_Y))
			{
				state = CN_Move;
			}
			else
			{
				state = CN_Idle;
			}
			break;
			
		case CN_Move:
			state = CN_MoveWait;
			break;
		
		case CN_MoveWait:
			if(stickMoved(ADC_X, ADC_Y))
			{
				state = CN_MoveWait;
			}
		
	}
	return state;
}

unsigned char stickMoved(unsigned short x, unsigned short y)
{
	if(x > 0x0300 || x < 0x0100)
	{		
		return 1;
	}
	else if(y > 0x0300 || y < 0x0100)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

int getDirection(unsigned short x, unsigned short y)
{
	if(x > 0x0300 && y < 0x0100)
	{
		return NorthWest;
	}
	else if(x > 0x0300 && y > 0x0100 && y < 0x0300)
	{
		return North;
	}
	//else if( x > 0x0300)
}

// LEDs

void initLEDs();

int TickFct_LED(int state)
{
	switch(state)
	{
		case LED_Start:
			state = LED_Init;
			break;
		
		case LED_Init:
			state = LED_Cycle;
			break;
			
		case LED_Cycle:
			state = LED_Cycle;
			break;
			
		default:
			break;
	}
	switch(state)
	{
		case LED_Start:
			break;
			
		case LED_Init:
			initLEDs();
			break;
			
		case LED_Cycle:
			for(unsigned char i = 0; i < 64; i++)
			{
				setLED(i, LEDs[1].green, LEDs[i].red, LEDs[i].blue);
			}
			cycle();
			break;
	}
	return state;
}

void initLEDs()
{
	for(unsigned char i = 0; i < 64; i++)
	{
		LEDs[i].green = 1;
		LEDs[i].red = 1;
		LEDs[i].blue = 1;
	}
	for(unsigned char i = 0; i < 4; i++)
	{
		LEDs[i + 18].green = 2;
		LEDs[i + 18].red = 0;
		LEDs[i + 18].blue = 0;
	}
	for(unsigned char i = 0; i < 4; i++)
	{
		LEDs[i + 42].green = 2;
		LEDs[i + 42].red = 0;
		LEDs[i + 42].blue = 0;
	}
	LEDs[26].green = 2;
	LEDs[26].red = 0;
	LEDs[26].blue = 0;
	
	LEDs[34].green = 2;
	LEDs[34].red = 0;
	LEDs[34].blue = 0;
	
	LEDs[29].green = 2;
	LEDs[29].red = 0;
	LEDs[29].blue = 0;
	
	LEDs[37].green = 2;
	LEDs[37].red = 0;
	LEDs[37].blue = 0;
}