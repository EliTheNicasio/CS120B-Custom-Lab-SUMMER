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

#define tasksSize 7
Task tasks[tasksSize];

enum IN_States {IN_Start, IN_Init, IN_Cycle} IN_State;
int TickFct_Input(int state);

enum CN_States {CN_Start, CN_Init, CN_Idle, CN_Press, CN_Release, CN_Move, CN_MoveWait} CN_State;
int TickFct_Controller(int state);

enum AN_States {AN_Start, AN_Init, AN_FallingIntro, AN_Idle, AN_Refill} AN_State;
int TickFct_Animate(int state);

enum LED_States {LED_Start, LED_Init, LED_Cycle} LED_State;
int TickFct_LED(int state);

enum NK_States {NK_Begin, NK_StartWait, NK_StartPress, NK_Countdown, NK_Finish} NK_State;
int TickFct_Nokia(int state);

enum RS_States{RS_Start, RS_Init, RS_Idle, RS_Press, RS_Release} RS_State;
int TickFct_Reset(int state);

enum CD_States{CD_Start, CD_Init, CD_Idle} CD_State;
int TickFct_Countdown(int state);

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

enum GemColor {GC_Red, GC_Blue, GC_Green, GC_Yellow, GC_Empty, GC_INVALID};

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
			
		case GC_Empty:
			Gems[i].green = 0;
			Gems[i].red = 0;
			Gems[i].blue = 0;
			break;
			
		default:
			break;
	}
}

int getGemColor(unsigned char i)
{
	unsigned char red = Gems[i].red, green = Gems[i].green, blue = Gems[i].blue;
	
	if(green == 2 && red == 0 && blue == 0)
	{
		return GC_Green;
	}
	else if(green == 0 && red == 2 && blue == 0)
	{
		return GC_Red;
	}
	else if(green == 0 && red == 0 && blue == 2)
	{
		return GC_Blue;
	}
	else if(green == 2 && red == 2 && blue == 0)
	{
		return GC_Yellow;
	}
	else if(green == 0 && red == 0 && blue == 0)
	{
		return GC_Empty;
	}
	else
	{
		return GC_INVALID;
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
	tasks[i].period = 100;
	tasks[i].elapsedTime = 0;
	tasks[i].TickFct = &TickFct_Input;
	i++;
	tasks[i].state = CN_Start;
	tasks[i].period = 200;
	tasks[i].elapsedTime = 0;
	tasks[i].TickFct = &TickFct_Controller;
	i++;
	tasks[i].state = NK_Begin;
	tasks[i].period = 500;
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
	i++;
	tasks[i].state = RS_Start;
	tasks[i].period = 200;
	tasks[i].elapsedTime = 0;
	tasks[i].TickFct = &TickFct_Reset;
	i++;
	tasks[i].state = CD_Start;
	tasks[i].period = 1000;
	tasks[i].elapsedTime = 0;
	tasks[i].TickFct = &TickFct_Countdown;
	

	ADC_Init();
	
	TimerSet(1);
	TimerOn();
	
	nokia_lcd_init();
	
	initMatrix();
	setAllLEDs(0, 2, 0);
	
    while (1) 
    {
    }
}

unsigned char prevCursor;

unsigned char countdown;

unsigned char fallFinished;
unsigned char gameFinished;

unsigned long score;

unsigned int seed; 

unsigned char startGame;

unsigned char getIndex(unsigned char x, unsigned char y);

unsigned char bnPress;
unsigned char rsPress;

#define a3 ~PINA & 0x08

// Input Reader
unsigned short ADC_X;
unsigned short ADC_Y;

#define analog_X 0x00
#define analog_Y 0x01

#define a2 ~PINA & 0x04

int TickFct_Input(int state)
{
	switch(state)
	{
		case IN_Start:
			state = IN_Init;
			break;
			
		case IN_Init:
			state = IN_Cycle;
			seed = 0;
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
			bnPress = a2;
			rsPress = a3;
			seed++;
			break;
	}
	return state;
}

// Resets game

int TickFct_Reset(int state)
{
	switch(state)
	{
		case RS_Start:
			state = RS_Init;
			break;
			
		case RS_Init:
			state = RS_Idle;
			break;
			
		case RS_Idle:
			if(rsPress)
			{
				state = RS_Press;
			}
			else
			{
				state = RS_Idle;
			}
			break;
			
		case RS_Press:
			state = RS_Release;
			break;
			
		case RS_Release:
			if(rsPress)
			{
				state = RS_Release;
			}
			else
			{
				state = RS_Idle;
			}
			break;
			
		default:
			state = RS_Start;
			break;
	}
	switch(state)
	{
		case RS_Start:
			break;
			
		case RS_Init:
			break;
			
		case RS_Idle:
			break;
			
		case RS_Press:
			for(unsigned char i = 0; i < tasksSize; i++)
			{
				tasks[i].state = 0;
			}
			for(unsigned char i = 0; i < 64; i++)
			{
				setGemColor(i, GC_Empty);
				LEDs[i].green = Gems[i].green;
				LEDs[i].red = Gems[i].red;
				LEDs[i].blue = Gems[i].blue;
			}	
			break;
			
		case RS_Release:
			break;
	}
	return state;
}

// Controller (Analog Stick)

enum Direction {North, NorthEast, East, SouthEast, South, SouthWest, West, NorthWest, INVALID} moveDir;

unsigned char stickMoved(unsigned short x, unsigned short y);
int getDirection(unsigned short x, unsigned short y);
void moveCursor(int dir);
unsigned char getCursorIndex();
void destroy();

unsigned char isDestroyed;

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
			if(bnPress)
			{
				state = CN_Press;
			}
			else if(stickMoved(ADC_X, ADC_Y))
			{
				state = CN_Move;
				moveDir = getDirection(ADC_X, ADC_Y);
			}
			else
			{
				state = CN_Idle;
			}
			break;
			
		case CN_Press:
			state = CN_Release;
			break;
			
		case CN_Release:
			if(bnPress)
			{
				state = CN_Release;
			}
			else
			{
				state= CN_Idle;
			}
			break;
			
		case CN_Move:
			state = CN_MoveWait;
			break;
		
		case CN_MoveWait:
			if(!stickMoved(ADC_X, ADC_Y))
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
			isDestroyed = 0;
			score = 0;
			prevCursor = 0;
			break;
			
		case CN_Idle:
			isDestroyed = 0;
			break;
			
		case CN_Press:
			if(startGame)
			{
				destroy();
				isDestroyed = 1;
				prevCursor = getCursorIndex();
			}
			break;
			
		case CN_Release:
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

void refillGems();

unsigned char maxHeight;
unsigned char minHeight;

int TickFct_Animate(int state)
{
	static unsigned int i;
	switch(state)
	{
		case AN_Start:
			state = AN_Init;
			break;
			
		case AN_Init:
			i = 0;
			if(startGame)
			{
				initLEDs();
				state = AN_FallingIntro;
			}
			else
			{
				state = AN_Init;
			}
			break;
			
		case AN_FallingIntro:
			if(i < 8)
			{
				state = AN_FallingIntro;
			}
			else
			{
				fallFinished = 1;
				state = AN_Idle;
			}

			break;
			
		case AN_Idle:
			if(isDestroyed)
			{
				state = AN_Refill;
				
			/*	i = 0;
				
				// 9 is an invalid height, ranges from 0 to 7
				maxHeight = 9;
				minHeight = 9;
				
				for(unsigned char j = 0; j < 63; j++)
				{
					if(Matching[j] == 1)
					{
						if(maxHeight == 9)
						{
							maxHeight = j / 8;
						}
						else if(maxHeight < (j / 8))
						{
							maxHeight = j / 8;
						}
						
						if(minHeight == 9)
						{
							minHeight = j / 8;
						}
						else if(minHeight > (j / 8))
						{
							minHeight = j / 8;
						}
					}
				}*/
			}
			else
			{
				state = AN_Idle;
			}
			break;
			
		case AN_Refill:
		/*	if(i < maxHeight - minHeight)
			{
				state = AN_Refill;
				i++;
			}
			else
			{
				state = AN_Idle;
			}*/
			state = AN_Idle;
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
			fallFinished = 0;
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
			
		case AN_Idle:
			//blinkCursor = blinkCursor ? 0 : 1;
			for(unsigned char j = 0; j < 64; j++)
			{
				LEDs[j].green = Gems[j].green;
				LEDs[j].red = Gems[j].red;
				LEDs[j].blue = Gems[j].blue;
			}
			blinkCursor = 1;
			break;
			
		case AN_Refill:
			refillGems();
			
		/*	for(unsigned char j = 0; j < 8; j++)
			{
				for(unsigned char k = 1; k < 8; k++)
				{
					if(Matching[getIndex(j, k - 1)] == 1)
					{
						setGemColor()
					}
				}
			}*/
			
			for(unsigned char j = 0; j < 64; j++)
			{
				LEDs[j].green = Gems[j].green;
				LEDs[j].red = Gems[j].red;
				LEDs[j].blue = Gems[j].blue;
			}
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
	srand(seed);
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

// Same as getCursorIndex() but with input

unsigned char getIndex(unsigned char x, unsigned char y)
{
	return x + (y * 8);
}

unsigned char getXIndex(unsigned char i)
{
	return i % 8;
}

unsigned char getYIndex(unsigned char i)
{
	return i / 8;
}

// Nokia

unsigned char strScore[12];
unsigned char strOut[2];
unsigned char finalScore[5];

int TickFct_Nokia(int state)
{
	static unsigned char i;
	
	switch(state)
	{
		case NK_Begin:
			state = NK_StartWait;
			break;
			
		case NK_StartWait:
			if(bnPress)
			{
				state = NK_StartPress;
			}
			else
			{
				state = NK_StartWait;
			}
			break;
			
		case NK_StartPress:
			state = NK_Countdown;
			i = 0;
			break;
			
		case NK_Countdown:
			if(!gameFinished)
			{
				state = NK_Countdown;
			}
			else
			{
				finalScore[0] = 48 + (score / 10000);
				finalScore[1] = 48 + ((score % 10000) / 1000);
				finalScore[2] = 48 + ((score % 1000) / 100);
				finalScore[3] = 48 + ((score % 100) / 10);
				finalScore[4] = 48 + (score % 10);
				
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
		case NK_Begin:
			nokia_lcd_clear();
			nokia_lcd_set_cursor(1,1);
			nokia_lcd_write_string("Start", 2);
			break;
			
		case NK_StartWait:
			nokia_lcd_clear();
			nokia_lcd_set_cursor(1,1);
			nokia_lcd_write_string("  Welcome to", 1);
			nokia_lcd_set_cursor(12,15);
			nokia_lcd_write_string("Collapse!!!", 1);
			nokia_lcd_set_cursor(1,30);
			nokia_lcd_write_string("  Hold down   click to begin", 1);
			
			startGame = 0;
			i = 0;
			
			strncpy(strOut, "  ", sizeof(strOut));
			strncpy(strScore, "score:      ", sizeof(strScore));
			
			break;
			
		case NK_StartPress:
			nokia_lcd_clear();
			startGame = 1;
			break;
			
		case NK_Countdown:
			
			strOut[0] = 48 + (countdown / 10);
			strOut[1] = 48 + (countdown % 10);
			
			strScore[7] = 48 + (score / 10000);
			strScore[8] = 48 + ((score % 10000) / 1000);
			strScore[9] = 48 + ((score % 1000) / 100);
			strScore[10] = 48 + ((score % 100) / 10);
			strScore[11] = 48 + (score % 10);
			
			nokia_lcd_clear();
			nokia_lcd_set_cursor(32,1);
			nokia_lcd_write_string_with_size(strOut, 2, 2);
			
			nokia_lcd_set_cursor(8, 30);
			nokia_lcd_write_string_with_size(strScore, 1, 12);
			
			break;
			
		case NK_Finish:
			nokia_lcd_clear();
			nokia_lcd_set_cursor(16,1);
			nokia_lcd_write_string("Game Over", 1);
			nokia_lcd_set_cursor(10, 20);
			nokia_lcd_write_string("Final Score:", 1);
			nokia_lcd_set_cursor(28, 30);
			nokia_lcd_write_string_with_size(finalScore, 1, 5);
			break;
	}
	
	nokia_lcd_render();
	
	return state;
}

unsigned char Matching[64];
int matchColor;

void searchMatching(unsigned char x, unsigned char y)
{	
	for(unsigned char i = 0; i < 7 - x; i++)
	{
		if(getGemColor(getIndex(x + i + 1, y)) != matchColor)
		{
			break;
		}
		else
		{
			Matching[getIndex(x + i + 1, y)] = 1;
		}
	}
	
	for(unsigned char i = 0; i < x; i++)
	{
		if(getGemColor(getIndex(x - i - 1, y)) != matchColor)
		{
			break;
		}
		else
		{
			Matching[getIndex(x - i +- 1, y)] = 1;
		}
	}
	
	for(unsigned char i = 0; i < 7 - y; i++)
	{
		if(getGemColor(getIndex(x, y + i + 1)) != matchColor)
		{
			break;
		}
		else
		{
			Matching[getIndex(x, y + i + 1)] = 1;
		}
	}
	
	for(unsigned char i = 0; i < y; i++)
	{
		if(getGemColor(getIndex(x, y - i - 1)) != matchColor)
		{
			break;
		}
		else
		{
			Matching[getIndex(x, y - i - 1)] = 1;
		}
	}
}

void refillGemsNoRepeat(int color);

// Destroy, the ultimate function. Destroys gem at cursor location and other gems
// of the same color that are connected to it.
void destroy()
{
	for(unsigned char i = 0; i < 64; i++)
	{
		Matching[i] = 0;
	}
	
	matchColor = getGemColor(getIndex(cursor.xIndex, cursor.yIndex));
	Matching[getCursorIndex()] = 1;
	
	searchMatching(cursor.xIndex, cursor.yIndex);
	
	for(unsigned char j = 0; j < 6; j++)
	{
		for(unsigned char i = 0; i < 64; i++)
		{
			if(Matching[i])
			{
				searchMatching(getXIndex(i), getYIndex(i));
			}
		}
	}
	
	for(unsigned char i = 0; i < 64; i++)
	{
		if(Matching[i])
		{
			setGemColor(i,GC_Empty);
			if(prevCursor == getCursorIndex())
			{
				score+=1;
			}
			else
			{
				score += 10;
			}
		}
	}
	refillGemsNoRepeat(matchColor);
	//refillGems();
}

void refillGems()
{
	for(unsigned char i = 0; i < 64; i++)
	{
		if(getGemColor(i) == GC_Empty)
		{
			setGemColor(i, rand() % 4);
		}
	}	
}

void refillGemsNoRepeat(int color)
{
	for(unsigned char i = 0; i < 64; i++)
	{
		if(getGemColor(i) == GC_Empty)
		{
			setGemColor(i, rand() % 4);
			while(getGemColor(i) == color)
			{
				setGemColor(i, rand() % 4);
			}
		}
	}
}


// Countdown

int TickFct_Countdown(int state)
{
	static unsigned char i;
	switch(state)
	{
		case CD_Start:
			state = CD_Init;
			break;
			
		case CD_Init:
			state = CD_Idle;
			break;
			
		case CD_Idle:
			state = CD_Idle;
			break;
			
		default:
			state = CD_Start;
			break;
	}
	switch(state)
	{
		case CD_Start:
			break;
			
		case CD_Init:
			countdown = 60;
			gameFinished = 0;
			i = 0;
			break;
			
		case CD_Idle:
			if(countdown == 0)
			{
				gameFinished = 1;
			}
			else if(fallFinished && !(i < 8) && countdown > 0)
			{
				countdown--;
				i = 0;
			}
			else if(fallFinished && i < 8)
			{
				i++;
			}
			break;
	}
	return state;
}