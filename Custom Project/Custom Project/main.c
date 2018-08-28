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
#include "nokia5110.h"

// Concurrent SM Stuff
typedef struct
{
	int state;
	unsigned long period;
	unsigned long elapsedTime;
	int (*TickFct)(int);
} Task;

const unsigned long tasksPeriod = 50;

#define tasksSize 5
Task tasks[tasksSize];

enum IN_States {IN_Start, IN_Cycle} IN_State;
int TickFct_Input(int state);

enum CN_States {CN_Start, CN_Init, CN_Idle, CN_Move, CN_MoveWait} CN_State;
int TickFct_Controller(int state);

enum AN_States {AN_Start, AN_Init, AN_FallingIntro, AN_BlinkCursor} AN_State;
int TickFct_Animate(int state);

enum LED_States {LED_Start, LED_Init, LED_Cycle} LED_State;
int TickFct_LED(int state);

enum NK_States {NK_Start, NK_Init, NK_Countdown, NK_Finish} NK_State;
int TickFct_Nokia(int state);

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

enum GemColor {GC_Red, GC_Blue, GC_Green, GC_Yellow};

typedef struct 
{
	unsigned char green;
	unsigned char red;
	unsigned char blue;
} LED;

LED LEDs[64]; 

LED Gems[64];
	
void setGemColor(unsigned char i, int color)
{
	switch(color)
	{
		case GC_Red:
			Gems[i].green = 0;
			Gems[i].red = 2;
			Gems[i].blue = 0;
			break;
			
		case GC_Blue:
			Gems[i].green = 0;
			Gems[i].red = 0;
			Gems[i].blue = 2;
			break;
			
		case GC_Green:
			Gems[i].green = 2;
			Gems[i].red = 0;
			Gems[i].blue = 0;
			break;
			
		case GC_Yellow:
			Gems[i].green = 2;
			Gems[i].red = 2;
			Gems[i].blue = 0;
			break;
			
		default:
			break;
	}
}

typedef struct 
{
	unsigned char xIndex;
	unsigned char yIndex;
} Cursor;

Cursor cursor;

unsigned char checkMatch();

int main(void)
{
	DDRA = 0x00; PORTA = 0xFF;
	DDRB = 0xFF; PORTB = 0x00;
	DDRC = 0xFF; PORTC = 0x00;
	
	unsigned char i = 0;
	tasks[i].state = IN_Start;
	tasks[i].period = 50;
	tasks[i].elapsedTime = 0;
	tasks[i].TickFct = &TickFct_Input;
	i++;
	tasks[i].state = CN_Start;
	tasks[i].period = 200;
	tasks[i].elapsedTime = 0;
	tasks[i].TickFct = &TickFct_Controller;
	i++;
	tasks[i].state = NK_Start;
	tasks[i].period = 15000;
	tasks[i].elapsedTime = 0;
	tasks[i].TickFct = &TickFct_Nokia;
	i++;
	tasks[i].state = AN_Start;
	tasks[i].period = 2500;
	tasks[i].elapsedTime = 0;
	tasks[i].TickFct = &TickFct_Animate;
	i++;
	tasks[i].state = LED_Start;
	tasks[i].period = 200;
	tasks[i].elapsedTime = 0;
	tasks[i].TickFct = &TickFct_LED;
	

	ADC_Init();
	
	TimerSet(1);
	TimerOn();
	
	nokia_lcd_init();
	
	initMatrix();
	setAllLEDs(0, 2, 0);
	
	srand(time(0));
	
    while (1) 
    {
    }
}

// Input Reader

unsigned short ADC_X;
unsigned short ADC_Y;

#define analog_X 0x00
#define analog_Y 0x01

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
void moveCursor(int dir);

int TickFct_Controller(int state)
{
	switch(state)
	{
		case CN_Start:
			state = CN_Init;
			break;
			
		case CN_Init:
			state = CN_Idle;
			break;
			
		case CN_Idle:
			if(stickMoved(ADC_X, ADC_Y))
			{
				state = CN_Move;
				moveDir = getDirection(ADC_X, ADC_Y);
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
		/*	if(moveDir != getDirection(ADC_X, ADC_Y))
			{
				state = CN_Move;
			}
			else */if(!stickMoved(ADC_X, ADC_Y))
			{
				state = CN_Idle;
			}
			else
			{
				state = CN_MoveWait;
			}
			break;
			
		default:
			state = CN_Start;
			break;
	}
	switch(state)
	{
		case CN_Start:
			break;
			
		case CN_Init:
			cursor.xIndex = 0;
			cursor.yIndex = 0;
			break;
			
		case CN_Idle:
			/*if(stickMoved(ADC_X, ADC_Y))
			{
				LEDs[63].green = 0;
				LEDs[63].red = 0;
				LEDs[63].blue = 2;
			}
			else
			{
				LEDs[63].green = 0;
				LEDs[63].red = 2;
				LEDs[63].blue = 0;
			}*/
			break;
			
		case CN_Move:
			moveCursor(moveDir);
			break;
			
		case CN_MoveWait:
			break;
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

#define up y > 0x0300
#define down y < 0x0100
#define neutralY y < 0x0300 && y > 0x0100
#define right x < 0x0100
#define left x > 0x0300
#define neutralX x < 0x0300 && x > 0x0100

int getDirection(unsigned short x, unsigned short y)
{
	if(up && left)
	{
		return NorthWest;
	}
	else if(up && neutralX)
	{
		return North;
	}
	else if(up && right)
	{
		return NorthEast;
	}
	else if (left && neutralY)
	{
		return West;
	}
	else if(right && neutralY)
	{
		return East;
	}
	else if(left && down)
	{
		return SouthWest;
	}
	else if(down && neutralX)
	{
		return South;
	}
	else if(down && right)
	{
		return SouthEast;
	}
	else
	{
		return INVALID;
	}
}

unsigned char blinkCursor;

void moveCursor(int dir)
{
//	blinkCursor = 1;
	
	switch(dir)
	{
		case North:
			if(cursor.yIndex > 0)
			{
				cursor.yIndex--;
			}
			break;
			
		case NorthEast:
			if(cursor.yIndex > 0 && cursor.xIndex < 7)
			{
				cursor.xIndex++;
				cursor.yIndex--;
			}
			break;
			
		case East:
			if(cursor.xIndex < 7)
			{
				cursor.xIndex++;
			}
			break;
			
		case SouthEast:
			if(cursor.xIndex < 7 && cursor.yIndex < 7)
			{
				cursor.xIndex++;
				cursor.yIndex++;
			}
			break;
			
		case South:
			if(cursor.yIndex < 7)
			{
				cursor.yIndex++;
			}
			break;
			
		case SouthWest:
			if(cursor.yIndex < 7 && cursor.xIndex > 0)
			{
				cursor.yIndex++;
				cursor.xIndex--;
			}
			break;
			
		case West:
			if(cursor.xIndex > 0)
			{
				cursor.xIndex--;
			}
			break;
			
		case NorthWest:
			if(cursor.xIndex > 0 && cursor.yIndex > 0)
			{
				cursor.xIndex--;
				cursor.yIndex--;
			}
			break;
			
		default:
			break;
	}
}

// Blink Cursor

unsigned char getCursorIndex();

int TickFct_Animate(int state)
{
	static unsigned int i;
	switch(state)
	{
		case AN_Start:
			state = AN_Init;
			break;
			
		case AN_Init:
			state = AN_FallingIntro;
			i = 0;
			break;
			
		case AN_FallingIntro:
			if(i < 8)
			{
				state = AN_FallingIntro;
			}
			else
			{
				state = AN_BlinkCursor;
			}

			break;
			
		case AN_BlinkCursor:
			state = AN_BlinkCursor;
			break;
			
		default:
			state = AN_Start;
			break;
	}
	switch(state)
	{
		case AN_Start:
			break;
			
		case AN_Init:
			blinkCursor = 0;
			break;
			
		case AN_FallingIntro:
			for(unsigned char j = 0; j < 64; j++)
			{
				if(j >= (8 * (7 - i)))
				{
					LEDs[j].green = Gems[((i + 1) * 8) - 1 - (63 - j)].green;
					LEDs[j].red = Gems[((i + 1) * 8) - 1 - (63 - j)].red;
					LEDs[j].blue = Gems[((i + 1) * 8) - 1 - (63 - j)].blue;
				}
				else
				{
					LEDs[j].green = 0;
					LEDs[j].red = 0;
					LEDs[j].blue = 0;
				}
			}
			i++;
			break;
			
		case AN_BlinkCursor:
			//blinkCursor = blinkCursor ? 0 : 1;
			for(unsigned char j = 0; j < 64; j++)
			{
				LEDs[j].green = Gems[j].green;
				LEDs[j].red = Gems[j].red;
				LEDs[j].blue = Gems[j].blue;
			}
			blinkCursor = 1;
			break;
	}
	return state;
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
				if(i != getCursorIndex())
				{
					setLED(i, LEDs[i].green, LEDs[i].blue, LEDs[i].red);	
				}
			}
			if(blinkCursor)
			{
				setLED(getCursorIndex(), 5, 5, 5);
			}
			else
			{
				setLED(getCursorIndex(), LEDs[getCursorIndex()].green, LEDs[getCursorIndex()].blue, LEDs[getCursorIndex()].red);
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
		setGemColor(i, rand() % 4);
	}
}

unsigned char checkMatch()
{
	unsigned char g1 = 0, g2 = 0, g3 = 0;
	for(unsigned char i = 0; i < 62; i++)
	{
		
	}
	
	return g1;
}

// Specifically for LED function, converts 2 indices to 1
unsigned char getCursorIndex()
{
	return cursor.xIndex + (cursor.yIndex * 8);
}

// Nokia

unsigned char countdown;
unsigned char strOut[2];

int TickFct_Nokia(int state)
{
	switch(state)
	{
		case NK_Start:
			state = NK_Init;
			break;
			
		case NK_Init:
			state = NK_Countdown;
			countdown = 60;
			break;
			
		case NK_Countdown:
			if(countdown > 0)
			{
				state = NK_Countdown;
				countdown--;
			}
			else
			{
				state = NK_Finish;
			}
			break;
			
		case NK_Finish:
			state = NK_Finish;
			break;
			
		default:
			break;
	}
	switch(state)
	{
		case NK_Start:
			nokia_lcd_clear();
			nokia_lcd_set_cursor(1,1);
			nokia_lcd_write_string("Start", 2);
			break;
			
		case NK_Init:
			strOut[0] = 'Z';
			strOut[1] = ' ';
			nokia_lcd_clear();
			nokia_lcd_set_cursor(1,1);
			nokia_lcd_write_string("Init", 2);
			break;
			
		case NK_Countdown:
			strOut[0] = 48 + (countdown / 10);
			strOut[1] = 48 + (countdown % 10);
			
			nokia_lcd_clear();
			nokia_lcd_set_cursor(1,1);
			nokia_lcd_write_string(strOut, 2);
			break;
			
		case NK_Finish:
			nokia_lcd_clear();
			nokia_lcd_set_cursor(1,1);
			nokia_lcd_write_string("Finish!", 2);		
			break;
	}
	
	nokia_lcd_render();
	
	return state;
}