# Uncomment lines below if you have problems with $PATH
#SHELL := /bin/bash
#PATH := /usr/local/bin:$(PATH)

all:
				pio run

upload:
				pio run --target upload

clean:
				pio run --target clean

program:
				pio run --target program

uploadfs:
				pio run --target uploadfs

update:
				pio update
db:
				pio run --target compiledb