/*
 * ADC_H.h
 *
 * Created: 7/28/2016 4:56:54 PM
 *  Author: Unlisted, code found on the following site:
 *	http://www.electronicwings.com/avr-atmega/analog-joystick-interface-with-atmega-16-32,
 *	and then modified by Elijah Nicasio.
 *	Description: Drives ADC. Inputs are the vertical and horizontal directions of the analog stick.
 *	To receive ADC value, use ADC_Read function and pass channel of direction to read.
 */



#ifndef ADC_H_H_
#define ADC_H_H_

#define F_CPU 8000000UL					/* Define CPU Frequency e.g. here its 8 Mhz */
#include <avr/io.h>						/* Include AVR std. library file */
#include <util/delay.h>
#define ipin PINA
#define iport PORTA

void ADC_Init();
unsigned short ADC_Read(unsigned char channel);

#endif /* ADC_H_H_ */