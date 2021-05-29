upload:
	pio run -v -t upload

connect: upload
	cu --nostop -f -l /dev/ttyUSB0 -s 38400

.PHONY: upload connect
