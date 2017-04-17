#include <avr/interrupt.h>
#include <util/delay.h>
#include "uart.h"
#include "sine.h"

#define DIALINC 100 // Set the increment for the control dial

uint32_t sine_position;
int32_t rate;
unsigned char old_portb;
unsigned char direction;

// This method writes data from sine tables to the PWMs
void update_sine()
{
  // 45v at 7Hz - (62496 / 7) * 256
  uint32_t voltage;
  uint32_t sine_position_msb;
  sine_position_msb = (sine_position >> 24);
  //voltage = 2285568 / delay;

  // Limit to bus voltage
  //if(voltage > 255) voltage = 255;
  voltage = 256;

  OCR3B = (voltage * sine_ocr3b[sine_position_msb]) >> 8;
  OCR3C = (voltage * sine_ocr3c[sine_position_msb]) >> 8;

  OCR3A = (voltage * sine_ocr3a[sine_position_msb]) >> 8;
  OCR4A = (voltage * sine_ocr4a[sine_position_msb]) >> 8;

  OCR4B = (voltage * sine_ocr4b[sine_position_msb]) >> 8;
  OCR4C = (voltage * sine_ocr4c[sine_position_msb]) >> 8;

  // These final tables switch on either the positive or negative PWM pins.
  TCCR3A = sine_tccr3a[sine_position_msb] | _BV(WGM31);
  TCCR4A = sine_tccr4a[sine_position_msb] | _BV(WGM41);
}

// Timer 1 advances the sine wave
ISR(TIMER1_COMPA_vect)
{
  sine_position += rate;
  update_sine();
}

// PCINT0 handles the rotary encoder
// Turninng the rotary encoder changes the rate of sine wave increment
ISR(PCINT0_vect)
{
  unsigned char new_portb;
  new_portb = PINB;

  if ((new_portb & 1) && !(old_portb & 1)) {
    // B0 went high
    if(new_portb & 2) {
      // B1 is high, CCW
      rate+=DIALINC;
    } else {
      // B1 is low, CW
      rate-=DIALINC;
    }
  }
  if (!(new_portb & 1) && (old_portb & 1)) {
    // B0 went low
    if(new_portb & 2) {
      // B1 is high, CW
      rate-=DIALINC;
    } else {
      // B1 is low, CCW
      rate+=DIALINC;
    }
  }
  if ((new_portb & 2) && !(old_portb & 2)) {
    // B1 went high
    if(new_portb & 1) {
      // B0 is high, CW
      rate-=DIALINC;
    } else {
      // B0 is low, CCW
      rate+=DIALINC;
    }
  }
  if (!(new_portb & 2) && (old_portb & 2)) {
    // B1 went low
    if(new_portb & 1) {
      // B0 is high, CCW
      rate+=DIALINC;
    } else {
      // B0 is low, CW
      rate-=DIALINC;
    }
  }
  old_portb = new_portb;

  // Don't go slower than 1Hz
  if(rate < 268435)   rate = 268435;
  // Don't go faster than 100Hz
  if(rate > 26843546) rate = 26843546;
}

int main()
{
  // Initialize variables
  sine_position = 0;
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
  OCR1A = 1000;                    // 16,000 Hz
  rate = 2684355;                  // 10Hz

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
