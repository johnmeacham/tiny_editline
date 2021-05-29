build:
	pio run

upload:
	pio run -t upload

connect: upload
	cu --nostop -f -l /dev/ttyUSB0 -s 38400
	reset
	tput cvvis

dump:
	avr-objdump -S .pio/build/uno/firmware.elf > dump.s
	avr-nm --size-sort -S .pio/build/uno/firmware.elf

.PHONY: build upload connect
