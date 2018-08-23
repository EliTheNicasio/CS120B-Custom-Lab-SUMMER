/*
 * WS2812.h
 *
 *  Author: User bianchi77 on forum avrfreaks.net, modified by Elijah Nicasio
 *  URL: https://www.avrfreaks.net/forum/tiny85-pwm-125-micro-second
 */ 


#ifndef WS2812_H_
#define WS2812_H_


#include <util/delay.h>
#include <avr/io.h>

#define F_CPU   16000000

#define TOn (PORTB = PORTB | 0x01)
#define TOff (PORTB = PORTB & 0xFE)


unsigned char g[64], r[64], b[64];

void T1();
void T0();
void RES();

void initMatrix();
void setLED(unsigned char index, unsigned char green, unsigned char red, unsigned char blue);
void setAllLEDs(unsigned char green, unsigned char red, unsigned char blue);
void cycle();

void T0();
void T1();
void RES();
unsigned char getBit(unsigned char bit, unsigned char x);

#endif /* WS2812_H_ */