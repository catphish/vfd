#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#include "uart.h"

uint32_t sine_position;
uint32_t throttle;
uint32_t speed;
uint32_t speed_copy;
char counter;

void wrap_sine_position() {
  if(sine_position >= 0x6000000) sine_position -= 0x6000000;
}

// PCINT0 handles the rotary encoder
// Each pulse from the encoder increments the sine wave position
// This allows us to make *relative* adjustments to slip
ISR(PCINT0_vect)
{
  // One complete sine wave = 65536*256*6 increments of the sine wave
  // The rotary encoder returns 1200 pulses per rotor rotation
  // (but only 600 per sine because it's a 4-pole motor)
  // 65536*256*6 / 600 = 13981 = synchronous speed
  sine_position += 167772;
  // The sum of the variable speed is used later to calculate V/Hz
  // based on how fast we're incrementing.
  speed += 167772;

  wrap_sine_position();
}

// Timer 1 runs at fixed intervals of 1kHz
// This allows us to make *absolute* adjustments to slip
ISR(TIMER1_COMPA_vect)
{
  // 1 thousandth of a sine rotation is 65536*256*6 / 1000 = 100663
  // Therefore for each 1Hz of slip, we add 100663 in this 1kHz timer

  // Add fixed slip (10Hz)
  sine_position += 1006633;
  speed += 1006633;

  // Cache the sum of speed into speed_copy every 20 iterations (50Hz)
  // This data will be used to calculate V/Hz later
  if(++counter > 19) {
    speed_copy = speed;
    speed = counter = 0;
  }
  wrap_sine_position();
}

// This method calculates the appropriate PWM outputs by calculating
// the necessary timings for space vector modulation in 6 segments.
void update_svm()
{
  uint16_t sine_angle;
  uint16_t voltage;
  uint8_t sine_segment;
  // Discard the least significant 14 bits and most significant
  // 8 bits from 32-bit sine position. We can only make use of 10 bits
  sine_angle = ((sine_position & 0xFFC000) >> 14);
  // The most significant 8 bits contain the SVM segment (0-5)
  sine_segment = sine_position >> 24;

  // To keep things simple, we'll just set the amplitude to the
  // throttle position. No V/Hz yet.
  voltage = throttle;

  // Set up space vector modulation according to segment, angle, and voltage
  switch(sine_segment) {
  case 0:
    // 100 -> 110
    OCR3A = voltage;
    OCR3B = ((sine_angle + 1) * (voltage + 1) - 1) >> 10;
    OCR3C = 0;
    break;

  case 1:
    // 110 -> 010
    OCR3A = ((1024 - sine_angle) * (voltage + 1) - 1) >> 10;
    OCR3B = voltage;
    OCR3C = 0;
    break;

  case 2:
    // 010 -> 011
    OCR3A = 0;
    OCR3B = voltage;
    OCR3C = ((sine_angle + 1) * (voltage + 1) - 1) >> 10;
    break;

  case 3:
    // 011 -> 001
    OCR3A = 0;
    OCR3B = ((1024 - sine_angle) * (voltage + 1) - 1) >> 10;
    OCR3C = voltage;
    break;

  case 4:
    // 001 -> 101
    OCR3A = ((sine_angle + 1) * (voltage + 1) - 1) >> 10;
    OCR3B = 0;
    OCR3C = voltage;
    break;

  case 5:
    // 101 -> 100
    OCR3A = voltage;
    OCR3B = 0;
    OCR3C = ((1024 - sine_angle) * (voltage + 1) - 1) >> 10;
    break;
  }
}

int main()
{
  // Initialize variables
  sine_position = 0;
  speed = 0;
  speed_copy = 0;
  counter = 0;

  // Set port F as input
  DDRF = 0;
  PORTF = 0;

  // set pins for output
  DDRE |= _BV(DDE3); // 5
  DDRE |= _BV(DDE4); // 2
  DDRE |= _BV(DDE5); // 3

  // Clear digital outputs
  PORTE = 0; // Port 2,3,5

  // Configure PWM channels and timer 3
  TCCR3A = _BV(COM3A1) | _BV(COM3B1) | _BV(COM3C1) | _BV(WGM31) | _BV(WGM30);
  TCCR3B = _BV(WGM32) | _BV(CS30);

  // Configure timer 1, this will increment sine wave
  TCCR1A = 0;
  TCCR1B = (1<<CS11) | (1<<WGM12); // Prescale (/8), CTC mode
  TCNT1 = 0;                       // Zero the timer
  TIMSK1 |= (1 << OCIE1A);         // Enable match interrupt
  OCR1A = 2000;                    // 1000 Hz

  // Initialize serial output
  uart_init();

  // Initialize ADC
  ADMUX = (1<<REFS0);
  ADCSRA = (1<<ADEN)|(1<<ADPS1);

  // Configure pin change interrupt on PCINT0
  DDRB = 0;      // Input
  PORTB = 0xFF;  // Pull High
  PCMSK0 = 1;    // Trigger on pin 0 only
  PCICR = 1;     // Enable PCIE0

  // Enable interrupts globally
  sei();

  // Main loop
  while(1) {
    // ADC0 (throttle)
    ADMUX = (1<<REFS0);
    ADCSRA |= (1<<ADSC);
    // Output current sine position / voltage to PWM while we wait for the ADC
    update_svm();
    while(ADCSRA & (1<<ADSC)) {
      update_svm();
    }
    // Throttle controls voltage
    throttle = ADC;

    //uart_write_uint32_t(throttle);
    //uart_write_byte(',');
    //uart_write_uint32_t(speed_copy);
    //uart_write_nl();
  }

}
