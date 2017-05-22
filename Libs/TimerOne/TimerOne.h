/*
 *  Interrupt and PWM utilities for 16 bit Timer1 on ATmega168/328
 *  Original code by Jesse Tane for http://labs.ideo.com August 2008
 *  Modified March 2009 by Jérôme Despatis and Jesse Tane for ATmega328 support
 *  Modified June 2009 by Michael Polli and Jesse Tane to fix a bug in setPeriod() which caused the timer to stop
 *  Modified April 2012 by Paul Stoffregen - portable to other AVR chips, use inline functions
 *
 *  This is free software. You can redistribute it and/or modify it under
 *  the terms of Creative Commons Attribution 3.0 United States License. 
 *  To view a copy of this license, visit http://creativecommons.org/licenses/by/3.0/us/ 
 *  or send a letter to Creative Commons, 171 Second Street, Suite 300, San Francisco, California, 94105, USA.
 *
 */

#ifndef TimerOne_h_
#define TimerOne_h_

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif


#define TIMER2_RESOLUTION 256UL  // Timer1 is 16 bit


// Placing nearly all the code in this .h file allows the functions to be
// inlined by the compiler.  In the very common case with constant values
// the compiler will perform all calculations and simply write constants
// to the hardware registers (for example, setPeriod).


class TimerOne
{
  public:
    //****************************
    //  Configuration
    //****************************
    void initialize(unsigned long microseconds=1000000) __attribute__((always_inline)) {
	TCCR1B = _BV(WGM13);        // set mode as phase and frequency correct pwm, stop the timer
	TCCR1A = 0;                 // clear control register A 
	setPeriod(microseconds);
    }
    void setPeriod(unsigned long microseconds) __attribute__((always_inline)) {
	const unsigned long cycles = (F_CPU / 2000000) * microseconds;
	if (cycles < TIMER2_RESOLUTION) {
		clockSelectBits = _BV(CS20);
		pwmPeriod = cycles;
	} else
	if (cycles < TIMER2_RESOLUTION * 8) {
		clockSelectBits = _BV(CS21);
		pwmPeriod = cycles / 8;
	} else
	if (cycles < TIMER2_RESOLUTION * 64) {
		clockSelectBits = _BV(CS21) | _BV(CS20);
		pwmPeriod = cycles / 64;
	} else
	if (cycles < TIMER2_RESOLUTION * 256) {
		clockSelectBits = _BV(CS22);
		pwmPeriod = cycles / 256;
	} else
	if (cycles < TIMER2_RESOLUTION * 1024) {
		clockSelectBits = _BV(CS22) | _BV(CS20);
		pwmPeriod = cycles / 1024;
	} else {
		clockSelectBits = _BV(CS22) | _BV(CS20);
		pwmPeriod = TIMER2_RESOLUTION - 1;
	}
	ICR1 = pwmPeriod;
	TCCR1B = _BV(WGM13) | clockSelectBits;
    }

    //****************************
    //  Run Control
    //****************************
    void start() __attribute__((always_inline)) {
	TCCR2B = 0;
	TCNT2 = 0;		// TODO: does this cause an undesired interrupt?
	TCCR2B = _BV(WGM13) | clockSelectBits;
    }
    void stop() __attribute__((always_inline)) {
	TCCR2B = _BV(WGM13);
    }
    void restart() __attribute__((always_inline)) {
	TCCR2B = 0;
	TCNT2 = 0;
	TCCR2B = _BV(WGM13) | clockSelectBits;
    }
    void resume() __attribute__((always_inline)) {
	TCCR2B = _BV(WGM13) | clockSelectBits;
    }

    //****************************
    //  Interrupt Function
    //****************************
    void attachInterrupt(void (*isr)()) __attribute__((always_inline)) {
	isrCallback = isr;
	TIMSK2 = _BV(TOIE2);
    }
    void attachInterrupt(void (*isr)(), unsigned long microseconds) __attribute__((always_inline)) {
	if(microseconds > 0) setPeriod(microseconds);
	attachInterrupt(isr);
    }
    void detachInterrupt() __attribute__((always_inline)) {
	TIMSK2 = 0;
    }
    void (*isrCallback)();

  protected:
    // properties
    static unsigned int pwmPeriod;
    static unsigned char clockSelectBits;
};

extern TimerOne Timer2;

#endif

