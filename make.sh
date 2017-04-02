avr-gcc -Os -DF_CPU=16000000UL -mmcu=atmega2560 -c -o vfd.o vfd.c &&
avr-gcc -Os -DF_CPU=16000000UL -mmcu=atmega2560 -c -o uart.o uart.c &&
avr-gcc -mmcu=atmega2560 vfd.o uart.o -o vfd &&
avr-objcopy -O ihex -R .eeprom vfd vfd.hex &&
avrdude -C/home/charlie/arduino-1.8.1/hardware/tools/avr/etc/avrdude.conf -patmega2560 -cwiring -P/dev/ttyACM0 -b115200 -D -Uflash:w:vfd.hex:i
