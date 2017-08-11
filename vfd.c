#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#include "table.h"

uint32_t sine_position; // Stores the current angle of the sine wave output
                        // 0 - 360 degrees = (0 - 0x5ffffff)
uint32_t throttle;      // Stores the current throttle input (10 bit)
uint32_t speed;         // Incremented and later used to estimate roational speed
uint32_t speed_copy;    // A cached copy of speed, used as an estimate of speed
char counter;           // Just a counter that we use to count some cycles

void wrap_sine_position() {
  // This ensures that any time sine_position is inremented, it stays in range
  if(sine_position >= 0x6000000) sine_position -= 0x6000000;
}

// PCINT0 handles the rotary encoder
// Each pulse from the encoder increments the sine wave position
// This also allows us to make relative adjustments to slip if necessary
ISR(PCINT0_vect)
{
  // One complete sine wave = 65536*256*6 increments
  // The rotary encoder returns 600 pulses per rotor rotation
  // With our 4 pole motor this means 300 pulses per sine wave
  // 300 pulses = 600 interrupts (high/low)
  // 65536*256*6 / 600 = 167772
  // Increment by 167772 for synchronous speed
  sine_position += 167772;

  // We also increment `speed` which is inspected at fixed intervals
  // to determine the rotational speed, and later, V/Hz.
  speed += 167772;

  // Ensure we stay in range
  wrap_sine_position();
}

// Timer 1 runs at fixed intervals of 1kHz
// This allows us to make absolute adjustments to slip
ISR(TIMER1_COMPA_vect)
{
  // 1 thousandth of a sine rotation is 65536*256*6 / 1000 = 100663
  // Therefore for each 1Hz of slip, we add 100663 in this 1kHz timer

  // Add fixed slip according to throttle. 5Hz.
  sine_position += (98 * throttle * 5);

  // Increment speed as above
  speed += (98 * throttle * 5);

  // Ensure we stay in range
  wrap_sine_position();

  // Cache the sum of speed into speed_copy every 20 iterations (50Hz)
  // This data will be used to calculate V/Hz later
  if(++counter > 19) {
    speed_copy = speed;
    speed = counter = 0;
  }
}

// This method calculates the appropriate PWM outputs by calculating
// the necessary timings for space vector modulation in 6 segments.
void update_svm()
{
  uint32_t sine_angle;  // The angle within the current segment (10 bits)
  uint32_t voltage;     // The voltage to apply (10 bits)
  uint8_t sine_segment; // The current segment of rotation (0-5)

  // Discard the least significant 14 bits and most significant
  // 8 bits from 32-bit sine position. We can only make use of 10 bits
  sine_angle = ((sine_position & 0xFFC000) >> 14);

  // The most significant 8 bits contain the SVM segment (0-5)
  sine_segment = sine_position >> 24;

  // Set voltage according to throttle
  // Currently the throttle applies full voltage.
  // With higher supply voltages we will want to cap this
  // with a V/Hz curve to prevent excessive current.
  voltage = throttle * 2; // 0-2046

  // Limit voltage to line voltage, obviously
  // Currently redundant but left in for safety
  if(voltage > 2047) voltage = 2047;

  // Set up space vector modulation according to segment, angle, and voltage
  switch(sine_segment) {
  case 0:
    // 100 -> 110
    OCR3A = ((voltage + 1) * table[sine_angle] - 1) >> 11;
    OCR3B = ((sine_angle + 1) * (voltage + 1) * table[sine_angle] - 1) >> 21;
    OCR3C = 0;
    break;

  case 1:
    // 110 -> 010
    OCR3A = ((1024 - sine_angle) * (voltage + 1) * table[sine_angle] - 1) >> 21;
    OCR3B = ((voltage + 1) * table[sine_angle] - 1) >> 11;
    OCR3C = 0;
    break;

  case 2:
    // 010 -> 011
    OCR3A = 0;
    OCR3B = ((voltage + 1) * table[sine_angle] - 1) >> 11;
    OCR3C = ((sine_angle + 1) * (voltage + 1) * table[sine_angle] - 1) >> 21;
    break;

  case 3:
    // 011 -> 001
    OCR3A = 0;
    OCR3B = ((1024 - sine_angle) * (voltage + 1) * table[sine_angle] - 1) >> 21;
    OCR3C = ((voltage + 1) * table[sine_angle] - 1) >> 11;
    break;

  case 4:
    // 001 -> 101
    OCR3A = ((sine_angle + 1) * (voltage + 1) * table[sine_angle] - 1) >> 21;
    OCR3B = 0;
    OCR3C = ((voltage + 1) * table[sine_angle] - 1) >> 11;
    break;

  case 5:
    // 101 -> 100
    OCR3A = ((voltage + 1) * table[sine_angle] - 1) >> 11;
    OCR3B = 0;
    OCR3C = ((1024 - sine_angle) * (voltage + 1) * table[sine_angle] - 1) >> 21;
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

  // set PORT E pins for output
  DDRE |= _BV(DDE3); // A5
  DDRE |= _BV(DDE4); // A2
  DDRE |= _BV(DDE5); // A3

  // Clear PORT E digital outputs
  PORTE = 0; // Port 2,3,5

  // Configure PWM channels and timer 3
  // WGM30 | WGM31 | WGM32 configures Fast PWM, 10-bit
  // CS30 configures 16MHz (ie non-divided) clock speed
  // 16,000,000 (clock) / 1024 (10 bit resolution) = 15,625 Hz PWM frequency
  TCCR3A = _BV(COM3A1) | _BV(COM3B1) | _BV(COM3C1) | _BV(WGM31) | _BV(WGM30);
  TCCR3B = _BV(WGM32) | _BV(CS30);

  // Configure timer 1, this will trigger interrupt at exact 1kHz intervals
  TCCR1A = 0;
  TCCR1B = (1<<CS11) | (1<<WGM12); // Prescale (/8), CTC mode
  TCNT1 = 0;                       // Zero the timer
  TIMSK1 |= (1 << OCIE1A);         // Enable match interrupt
  OCR1A = 2000;                    // 1000 Hz

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
    update_svm();
    // Output current sine position to PWM as often as possible
    while(ADCSRA & (1<<ADSC)) {
      update_svm();
    }
    // Read throttle from ADC whenever a value is available
    throttle = ADC;

  }

}
