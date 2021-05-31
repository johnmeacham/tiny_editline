build:
	pio run

native:
	pio run -e native

upload:
	pio run -e uno -t upload

connect: upload
	cu --nostop -f -l /dev/ttyUSB0 -s 38400
	reset
	tput cvvis

clean:
	pio run -t clean

dump:
	avr-objdump -S .pio/build/uno/firmware.elf > dump.s
	avr-nm --size-sort -S .pio/build/uno/firmware.elf
	avr-size .pio/build/uno/firmware.elf

.PHONY: build upload connect clean
