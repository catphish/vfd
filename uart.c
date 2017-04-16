#include <avr/interrupt.h>

#define BAUD 57600                       // define baud
#define BAUDRATE ((F_CPU)/(BAUD*16UL)-1) // set baud rate value for UBRR

void uart_init ()
{
    UBRR0H = (BAUDRATE>>8);              // shift the register right by 8 bits
    UBRR0L = BAUDRATE;                   // set baud rate
    UCSR0B|= (1<<TXEN0);                 // enable receiver and transmitter
    UCSR0C|= (1<<UCSZ01)|(1<<UCSZ00);    // 8bit data format
}

void uart_write_byte(unsigned char byte)
{
  while (!( UCSR0A & (1<<UDRE0)));
  UDR0 = byte;
}

void uart_sendstring(char* str)
{
  while (*str)
  {
    uart_write_byte(*str++);
  }
}

void uart_write_int(unsigned int i) {
  char buffer[20];
  itoa(i, buffer, 10);
  uart_sendstring(buffer);
}

void uart_write_uint(unsigned int i) {
  char buffer[20];
  utoa(i, buffer, 10);
  uart_sendstring(buffer);
}

void uart_write_nl() {
  uart_write_byte(10);
}
