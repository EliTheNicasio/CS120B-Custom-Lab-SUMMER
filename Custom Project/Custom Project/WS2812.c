/*
 * WS2812.c
 *
 *  Author: User bianchi77 on forum avrfreaks.net, modified by Elijah Nicasio
 *  URL: https://www.avrfreaks.net/forum/tiny85-pwm-125-micro-second
 *
 * Functions to get correct timings and display lights on 8x8 NeoPixel 
 */ 
#include <util/delay.h>
#include <avr/io.h>

#define F_CPU   16000000

#define TOn() (PORTB = PORTB | 0x01)
#define TOff() (PORTB = PORTB & 0xFE) 

#define T0H() asm("nop");
#define T0L() asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");

#define T1H() asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");
#define T1L() asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");asm("nop");

#define send0() TOn(); T0H(); TOff(); T0L();
#define send1() TOn(); T1H(); TOff(); T1L();

unsigned char g[64], r[64], b[64];

void RES();

void initMatrix()
{
	for(unsigned char i = 0;i < 64;++i)
	{
		g[i] = 0;
		r[i] = 0;
		b[i] = 0;
	}
}

void setLED(unsigned char index, unsigned char green, unsigned char red, unsigned char blue)
{
	g[index] = green;
	r[index] = red;
	b[index] = blue;
}

void setAllLEDs(unsigned char green, unsigned char red, unsigned char blue)
{
	for(unsigned char i = 0; i < 64; ++i)
		setLED(i, green, red, blue); 
}

void send(unsigned char b){

	unsigned char n;

	for(n=0x80; n>0; n>>=1)
	{
		if(b & n){
			send1();
			}else{
			send0();
		}
	}
}

void cycle()
{
	//calls send 64 times
	unsigned char *pr;
	unsigned char *pg;
	unsigned char *pb;
	unsigned char n;

	pr=r; //init pointers
	pg=g;
	pb=b;
	n=64;
	while(n--){    //update 30 leds
		send(*pg++);
		send(*pb++);
		send(*pr++);
	}
	RES();
}

void delnus(int n){
	//delay n us
	int x;

	while(n--){
		x=3;       //empirically determined fudge factor  16 mhz
		while(x--);
	}
}

void RES()
{
	TOff(); delnus(55);
	//_delay_us(55);
}

unsigned char getBit(unsigned char bit, unsigned char x)
{
	return (x >> bit) & 0x01;
}