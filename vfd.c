#include <avr/interrupt.h>
#include "uart.h"
#include "sine.h"

#define DIALINC 3                        // Set the increment for the control dial

unsigned char sine_position;
unsigned short delay;
unsigned char old_portb;
unsigned char direction;

// This method writes data from sine tables to the PWMs
void update_sine()
{
  double voltage;
  // 45v at 20Hz, this seems the most efficient for my no-load testing
  voltage = (double)3125 / (double)delay;
  // We could alter the curve
  // voltage = 0.2 + 0.8 * voltage;

  // Limit to bus voltage
  if(voltage > 1) voltage = 1;

  // We have a table for each motor direction
  if(direction) {
    OCR3B = sine_ocr3b[sine_position] * voltage;
    OCR3C = sine_ocr3c[sine_position] * voltage;

    OCR3A = sine_ocr3a[sine_position] * voltage;
    OCR4A = sine_ocr4a[sine_position] * voltage;

    OCR4B = sine_ocr4b[sine_position] * voltage;
    OCR4C = sine_ocr4c[sine_position] * voltage;

    // These final tables switch on either the positive or negative PWM pins.
    TCCR3A = sine_tccr3a[sine_position] | _BV(WGM31);
    TCCR4A = sine_tccr4a[sine_position] | _BV(WGM41);
  } else {
    OCR3B = sine_ocr3b_r[sine_position] * voltage;
    OCR3C = sine_ocr3c_r[sine_position] * voltage;

    OCR3A = sine_ocr3a_r[sine_position] * voltage;
    OCR4A = sine_ocr4a_r[sine_position] * voltage;

    OCR4B = sine_ocr4b_r[sine_position] * voltage;
    OCR4C = sine_ocr4c_r[sine_position] * voltage;

    // These final tables switch on either the positive or negative PWM pins.
    TCCR3A = sine_tccr3a_r[sine_position] | _BV(WGM31);
    TCCR4A = sine_tccr4a_r[sine_position] | _BV(WGM41);
  }
}

// Timer 1 advances the sine wave
ISR(TIMER1_COMPA_vect)
{
  sine_position++;
  update_sine();
  OCR1A = delay;

}

// PCINT0 handles the rotary encoder
// Turninng the rotary encoder changes the delay between each sine wave increment
ISR(PCINT0_vect)
{
  unsigned char new_portb;
  new_portb = PINB;

  if ((new_portb & 1) && !(old_portb & 1)) {
    // B0 went high
    if(new_portb & 2) {
      // B1 is high, CCW
      delay-=DIALINC;
    } else {
      // B1 is low, CW
      delay+=DIALINC;
    }
  }
  if (!(new_portb & 1) && (old_portb & 1)) {
    // B0 went low
    if(new_portb & 2) {
      // B1 is high, CW
      delay+=DIALINC;
    } else {
      // B1 is low, CCW
      delay-=DIALINC;
    }
  }
  if ((new_portb & 2) && !(old_portb & 2)) {
    // B1 went high
    if(new_portb & 1) {
      // B0 is high, CW
      delay+=DIALINC;
    } else {
      // B0 is low, CCW
      delay-=DIALINC;
    }
  }
  if (!(new_portb & 2) && (old_portb & 2)) {
    // B1 went low
    if(new_portb & 1) {
      // B0 is high, CCW
      delay-=DIALINC;
    } else {
      // B0 is low, CW
      delay+=DIALINC;
    }
  }
  old_portb = new_portb;
  // Don't go faster than 20Hz
  if(delay < 3125) delay = 3125;
  // Don't go slower than 1Hz
  if(delay > 62496) delay = 62496;
}

int main()
{
  // Initialize variables
  sine_position = 0;
  direction = 1;
  old_portb = PINB;

  // set pins for output
  DDRE |= _BV(DDE4); // 2
  DDRE |= _BV(DDE5); // 3
  DDRE |= _BV(DDE3); // 5
  DDRH |= _BV(DDE3); // 6
  DDRH |= _BV(DDE4); // 7
  DDRH |= _BV(DDE5); // 8

  // Clear digital outputs
  PORTE = 0; // Port 2,3,5
  PORTH = 0; // Port 6,7,8

  // Configure PWM channels and timer 3/4
  TCCR3A = _BV(WGM31); // Port 2,3,5
  TCCR3B = _BV(WGM32) | _BV(WGM33) | _BV(CS30);  // Port 2,3,5
  ICR3 = 0x3FF;
  TCCR4A = _BV(WGM41); // Port 6,7,8
  TCCR4B = _BV(WGM42) | _BV(WGM43) | _BV(CS40);  // Port 6,7,8
  ICR4 = 0x3FF;

  // Configure timer 1, this will increment sine wave
  TCCR1A = 0;
  TCCR1B = (1<<CS10) | (1<<WGM12); // Prescale (/1), CTC mode
  TCNT1 = 0;                       // Zero the timer
  TIMSK1 |= (1 << OCIE1A);         // Enable match interrupt
  OCR1A = 62496;                   // 62496 = 1Hz, this line mostly for reference
  delay = 3125;                    // Start at at 20Hz

  // Configure pin change interrupt on PCINT0
  DDRB = 0;      // Input
  PORTB = 0xFF;  // Pull High
  PCMSK0 = 3;    // Trigger on pins 0 and 1
  PCICR = 1;     // Enable PCIE0

  // Initialize serial output
  uart_init();

  // Enable interrupts globally
  sei();

  while(1) {
  }

}
