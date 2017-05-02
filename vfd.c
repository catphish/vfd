#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#include "uart.h"
#include "sine.h"

uint32_t sine_position;
uint32_t sine_position_msb;
uint32_t voltage;

// PCINT0 handles the rotary encoder
// Each pulse from the encoder increments the sine wave position
ISR(PCINT0_vect)
{
  // The angle by which we increment the output sine for each pulse determines slip
  // 13981 = synchronous speed
  sine_position += 20971; // 150%
  
  // Note: We don't actually know the frequency, we just advance the sine wave
  // relative to the rotor position.
}

// This method writes data from sine tables to the PWMs
void update_sine()
{
  sine_position_msb = ((sine_position & 0xFF0000) >> 16);

  // Limit to bus voltage
  if(voltage > 65536) voltage = 65536;
  
  // TODO: We should calculate the current frequency and scale voltage to V/Hz
  
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

  // Initialize serial output
  uart_init();

  // Initialize ADC
  ADMUX = (1<<REFS0);
  ADCSRA = (1<<ADEN)|(1<<ADPS1);

  // Configure pin change interrupt on PCINT0
  DDRB = 0;      // Input
  PORTB = 0xFF;  // Pull High
  PCMSK0 = 3;    // Trigger on pins 0 and 1
  PCICR = 1;     // Enable PCIE0

  // Enable interrupts globally
  sei();

  uint16_t n;
  
  // Main loop
  while(1) {
    // ADC0 (throttle)
    ADMUX = (1<<REFS0);
    ADCSRA |= (1<<ADSC);
    while(ADCSRA & (1<<ADSC));
    // Throttle controls voltage
    voltage = (uint32_t)65 * ADC;

    // Go ahead and output current sine position and voltage to PWM
    update_sine();
    
    // Not really sure we need to sleep here
    _delay_us(100);

    //uart_write_uint32_t(x);
    //uart_write_byte(',');
    //uart_write_uint32_t(y);
    //uart_write_nl();
  }

}
