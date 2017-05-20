#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#include "uart.h"
#include "sine.h"

uint32_t sine_position;
uint32_t throttle;
uint32_t speed;
uint32_t speed_copy;
char counter;

// PCINT0 handles the rotary encoder
// Each pulse from the encoder increments the sine wave position
// This allows us to make *relative* adjustments to slip
ISR(PCINT0_vect)
{
  // One complete sine wave = 65536*256 increments of the sine wave
  // The rotary encoder returns 2400 pulses per rotor rotation
  // (but only 1200 per sine because it's a 4-pole motor)
  // 65536*256 / 1200 = 13981 = synchronous speed
  sine_position += 13981*2;
  // The sum of the variable speed is used later to calculate V/Hz
  // based on how fast we're incrementing.
  speed += 13981*2;
}

// Timer 1 runs at fixed intervals of 1kHz
// This allows us to make *absolute* adjustments to slip
ISR(TIMER1_COMPA_vect)
{
  // 1 thousandth of a sine rotation is 65536*256 / 1000 = 16777
  // Therefore for each 1Hz of slip, we add 16777 in this 1kHz timer

  // Add fixed slip (10Hz)
  sine_position += 167770;
  speed += 167770;

  // Cache the sum of speed into speed_copy every 20 iterations (50Hz)
  // This data will be used to calculate V/Hz later
  if(++counter > 19) {
    speed_copy = speed;
    speed = counter = 0;
  }
}

// This method calculates the appropriate output voltage based
// on various factors, looks up the output position in the sine
// wave tables, and outputs the product to the PWMs.
void update_sine()
{
  uint32_t sine_position_msb;
  uint32_t voltage;
  // Discard the least significant 16 bits and most significant
  // 8 bits from 32-bit sine position. We only have an 8-bit table
  sine_position_msb = ((sine_position & 0xFF0000) >> 16);

  // Calculate the portion of voltage due to current output frequency.
  // In the input (speed_copy), 16777 * 20 = 1Hz
  // In the output, AC voltage = rail voltage / sqrt(2) / 65535
  // Currently: 77.7*4 / 1.4142 / 65535, each output unit equals 0.003353489v
  // There if voltage = speed_copy, 1Hz = 1125.2 VAC
  // We shift this 8 bits to get 4.39V/Hz, a reasonable value for my motor.
  // We'll want a more accurate way to tune this.
  voltage = speed_copy >> 8;

  // We boost the voltage a bit to give more torque at low frequency.
  // This is quite arbitrary. Current value is 10% of line voltage.
  // At 10% slip, this results in a total locked rotor voltage of
  // 19659 (about 66 VAC)
  voltage = voltage + 6552;

  // Cap at line voltage
  if(voltage > 65535) voltage = 65535;

  // Scale with throttle
  voltage = (voltage * throttle) >> 10;

  // Cap at line voltage again, probably unnecessary.
  if(voltage > 65535) voltage = 65535;

  // Lookup positions in sine table, multiply by voltage, and sent to PWM
  OCR3B = (voltage * sine_ocr3b[sine_position_msb]) >> 16;
  OCR3C = (voltage * sine_ocr3c[sine_position_msb]) >> 16;

  OCR3A = (voltage * sine_ocr3a[sine_position_msb]) >> 16;
  OCR4A = (voltage * sine_ocr4a[sine_position_msb]) >> 16;

  OCR4B = (voltage * sine_ocr4b[sine_position_msb]) >> 16;
  OCR4C = (voltage * sine_ocr4c[sine_position_msb]) >> 16;

  // These final tables switch on either the positive or negative PWM pins.
  TCCR3A = sine_tccr3a[sine_position_msb] | _BV(WGM31);
  TCCR4A = sine_tccr4a[sine_position_msb] | _BV(WGM41);
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
  PCMSK0 = 1;    // Trigger on pins 0 and 1
  PCICR = 1;     // Enable PCIE0

  // Enable interrupts globally
  sei();

  // Main loop
  while(1) {
    // ADC0 (throttle)
    ADMUX = (1<<REFS0);
    ADCSRA |= (1<<ADSC);
    // Output current sine position / voltage to PWM while we wait for the ADC
    update_sine();
    while(ADCSRA & (1<<ADSC)) {
      update_sine();
    }
    // Throttle controls voltage
    throttle = ADC;

    //uart_write_uint32_t(throttle);
    //uart_write_byte(',');
    //uart_write_uint32_t(speed_copy);
    //uart_write_nl();
  }

}
