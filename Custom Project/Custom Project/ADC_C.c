/*
 * ADC_C.c
 *
 * Created: 7/28/2016 4:56:54 PM
 *  Author: Unlisted, code found on the following site:
 *	http://www.electronicwings.com/avr-atmega/analog-joystick-interface-with-atmega-16-32,
 *	and then modified by Elijah Nicasio.
 *	Description: Drives ADC. Inputs are the vertical and horizontal directions of the analog stick.
 *	To receive ADC value, use ADC_Read function and pass channel of direction to read.
 */ 

#include "ADC_H.h"

// Initialize ADC, pass proper flags
void ADC_Init()
{
	//DDRA = 0x00;		/* Make ADC port as input */
	//ADCSRA = 0x87;		/* Enable ADC, fr/128  */
	//ADMUX = 0x40;		/* Vref: Avcc, ADC channel: 0 */
	
	ADCSRA |= (1 << ADEN) | (1 << ADSC);
}

// Read value on ADC. Passing 0 returns horizontal value, 1 returns vertical value
unsigned short ADC_Read(unsigned char channel)
{
	//unsigned short ADC_value;
	
	//ADMUX = (0x40) | (channel & 0x07);/* set input channel to read */
	//ADCSRA |= (1<<ADSC);	/* start conversion */
	
	// Following causes issues with task struct, so it is commented out
	
	//while((ADCSRA &(1<<ADIF))== 0);	/* monitor end of conversion interrupt flag */
	
	//ADCSRA |= (1<<ADIF);	/* clear interrupt flag */
	//ADC_value = (int)ADCL;	/* read lower byte */
	//ADC_value = ADC_value + (int)ADCH*256;/* read higher 2 bits, Multiply with weightage */

	//return ADC;		/* return digital value */
	
	if(channel == 0x00)
	{
		ADMUX &= 0xFE;
	}
	else if(channel == 0x01)
	{
		ADMUX |= 0x01;
	}
	
	ADCSRA |= (1 << ADSC);
	//while (!(ADCSRA & (1<<ADSC)));
	
	return ADC;
}