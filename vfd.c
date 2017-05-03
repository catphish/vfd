#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#include "uart.h"
#include "sine.h"

uint32_t sine_position;
uint32_t counter;
uint32_t last_counter;
uint32_t throttle;

// PCINT0 handles the rotary encoder
// Each pulse from the encoder increments the sine wave position
ISR(PCINT0_vect)
{
  // Increment the sine wave by the angle of rotation
  sine_position += 13981; // 13981 = synchronous speed

  // Advance a regular counter that can be used to calculate the angular speed
  counter++;
}

// Timer 1 will also increment the sine wave, this time by a fixed slip
ISR(TIMER1_COMPA_vect)
{
  // This runs at 1000Hz, slip = 1Hz @ +16777
  sine_position += 83885; // 5Hz
}

// This method writes data from sine tables to the PWMs
void update_sine()
{
  uint32_t sine_position_msb;
  uint32_t voltage;
  sine_position_msb = ((sine_position & 0xFF0000) >> 16);

  // Scale V/Hz with throttle?
  voltage = throttle * 65;

  // Limit to bus voltage
  if(voltage > 65536) voltage = 65536;


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
  counter = 0;
  last_counter = 0;

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
    // Output current sine position / voltage to PWM while we wait for the ADC
    update_sine();
    while(ADCSRA & (1<<ADSC)) {
      update_sine();
    }
    // Throttle controls voltage
    throttle = ADC;

    //uart_write_uint32_t(x);
    //uart_write_byte(',');
    //uart_write_uint32_t(y);
    //uart_write_nl();
  }

}
