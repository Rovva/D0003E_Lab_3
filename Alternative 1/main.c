#include <avr/io.h>
#include <stdbool.h>
#include <stdint.h>
#include <avr/interrupt.h>

#include "tinythreads.h"

void init_lcd() {
	// LCD Enable (LCDEN) & Low Power Waveform (LCDAB)
	LCDCRA = (1<<LCDEN) | (1<<LCDAB) | (0<<LCDIF) | (0<<LCDIE) | (0<<LCDBL);

	// external asynchronous clock source (LCDCS), 1/3 bias (LCD2B), 1/4 duty (LCDMUX1:0), 25 segments enabled (LCDPM2:0)
	LCDCRB = (1<<LCDCS) | (0<<LCD2B) | (1<<LCDMUX1) | (1<<LCDMUX0) | (1<<LCDPM2) | (1<<LCDPM1) | (1<<LCDPM0);

	// prescaler setting N=16 (LCDPS2:0), clock divider setting D=8 (LCDCD2:0)
	LCDFRR = (0<<LCDPS2) | (0<<LCDPS1) | (0<<LCDPS0) | (1<<LCDCD2) | (1<<LCDCD1) | (1<<LCDCD0);

	// drive time 300 microseconds (LCDDC2:0), contrast control voltage 3.35 V (LCDCC3:0)
	LCDCCR = (0<<LCDDC2) | (0<<LCDDC1) | (0<<LCDDC0) | (1<<LCDCC3) | (1<<LCDCC2) | (1<<LCDCC1) | (1<<LCDCC0);

}

uint16_t digitLookUp(uint8_t c) {

	if(c < 48 || c > 57) {
		return 0;
	}

	uint16_t binary[10];

	binary[0] = 0b0001010101010001; // 0
	binary[1] = 0b0000000100010000; // 1
	binary[2] = 0b0001000111100001; // 2
	binary[3] = 0b0001000110110001; // 3
	binary[4] = 0b0000010110110000; // 4
	binary[5] = 0b0001010010110001; // 5
	binary[6] = 0b0001010011110001; // 6
	binary[7] = 0b0001000100010000; // 7
	binary[8] = 0b0001010111110001; // 8
	binary[9] = 0b0001010110110000; // 9
	binary[10] = '\0';

	return binary[(c - 48)];
}

void writeChar(char ch, int pos) {
	// If pos is less than zero or greater than 5, do nothing
	if(pos < 0 || pos > 5) {
		return;
	}

	uint16_t digitBinary = 0;
	uint8_t nibble_0 = 0, nibble_1 = 0, nibble_2 = 0, nibble_3 = 0, oldValue = 0, mask = 0, increment = 0;

	// Fetch the value needed to display number "ch" in LCDDRx
	digitBinary = digitLookUp(ch);

	// Bitshift 1 bit to get valid values for incrementing the pointer
	increment = pos >> 1;

	// Depending on if the value of pos is even or odd we adjust the nibbles
	// and mask correctly
	if((pos % 2) == 0) {
		nibble_0 = 0b00001111 & (digitBinary >> 12);
		nibble_1 = 0b00001111 & (digitBinary >> 8);
		nibble_2 = 0b00001111 & (digitBinary >> 4);
		nibble_3 = 0b00001111 & digitBinary;
		// This mask is needed to preserve what is on the right side of
		// LCDDRx
		mask = 0b11110000;
		} else {
		nibble_0 = 0b11110000 & (digitBinary >> 8);
		nibble_1 = 0b11110000 & (digitBinary >> 4);
		nibble_2 = 0b11110000 & digitBinary;
		nibble_3 = 0b11110000 & (digitBinary << 4);
		// Mask needed to preserve what is on the left side of LCDDRx
		mask = 0b00001111;
	}

	// Create a pointer and assign the memory address of LCDDR0
	// (is volatile really needed?)
	volatile uint8_t *LCDDRAddress = &LCDDR0;
	// Increment the pointers memory address with the value calculated earlier
	// This is needed to be able to use LCDDR0+x, LCDDR1+x, LCDDR2+x etc.
	LCDDRAddress = (LCDDRAddress + increment);
	// Preserve the old value by using a mask
	oldValue = mask & *LCDDRAddress;
	// Add the nibble using OR
	*LCDDRAddress = oldValue | nibble_0;
	// Increase the memory address of the pointer with 5 to be able to
	// use LCDDRx+5
	LCDDRAddress = LCDDRAddress + 5;

	oldValue = mask & *LCDDRAddress;
	*LCDDRAddress = oldValue | nibble_1;
	// Increase with 5 to be able to use LCDDRx+10
	LCDDRAddress = LCDDRAddress + 5;

	oldValue = mask & *LCDDRAddress;
	*LCDDRAddress = oldValue | nibble_2;
	// Increase with 5 to be able to use LCDDRx+15
	LCDDRAddress = LCDDRAddress + 5;

	oldValue = mask & *LCDDRAddress;
	*LCDDRAddress = oldValue | nibble_3;

}

bool is_prime(long i) {
	uint32_t c;
	// Loop to check if a number is dividable with anything less than half the value of "i"
	for(c = 2; c <= i/2; c++) {
		if(i%c == 0) {
			// Return false as the value of i is not a prime number
			return false;
		}
	}

	if(c == i/2 + 1) {
		// Return true as "i" is a prime number
		return true;
	}

	return false;
}

void printAt(long num, int pos) {
	// Use the global variable pp to test mutex
	int pp = pos;
	writeChar( (num % 100) / 10 + '0', pp);
	for(volatile int i = 0; i < 1000; i++) {}
	pp++;
	writeChar( num % 10 + '0', pp);
}

void computePrimes(int pos) {
	long n;

	for(n = 1; ; n++) {
		if (is_prime(n)) {
			// Lock the mutex
			//lock(&mutexlock);
			printAt(n, pos);
			//yield();
			//unlock(&mutexlock);
		}
	}
}

void blink() {
	/*
		1 second = 1000 ms
		Interrupt every 50 ms
		1000 / 50 = 20;
	*/

	for(;;) {
		if(readMilliseconds() >= 20) {
			LCDDR3 = LCDDR3 ^ 0b00000001;
			resetMilliseconds();
		}
	}
}

void init_button() {
	PORTB = (1<<PB7);
}

void button() {
	bool latch = false;
	uint8_t buttonNow = 0, buttonPrev = 0;

	for(;;) {
		// Read value of PINB7
		buttonNow = (PINB >> 7);
		// If the button state is 0 and the previous state was 1 then change latch state to true
		if(buttonNow == 0 && buttonPrev == 1) {
			if(latch == true) {
				latch = false;
				} else {
				latch = true;
			}
		}

		// Store the new value of buttonNow in buttonPrev
		buttonPrev = buttonNow;
		if(latch == true) {
			LCDDR1 = LCDDR1 ^ 0b00000010;			

			LCDDR0 = 0b11111011 & LCDDR0;
			//LCDDR1 = LCDDR1 | 0b000000010;
			//LCDDR0 = LCDDR0 ^ 0b000000100;
		} else {
			LCDDR1 = 0b11111101 & LCDDR1;

			LCDDR0 = LCDDR0 ^ 0b00000100;

			//LCDDR1 = LCDDR1 ^ 0b000000010;
			//LCDDR0 = LCDDR0 | 0b000000100;
		}
	}
}

// Yield when timer interrupts
ISR(TIMER1_COMPA_vect) {
	yield();
}

int main()
{
	init_lcd();
	init_button();
	
	spawn(computePrimes, 0);
	spawn(blink, 0);
	button();
	
}

