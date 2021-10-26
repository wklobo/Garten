#!/usr/bin/env python
# clightoff.py - 2014-07-25

import RPi.GPIO as GPIO

# Use GPIO numbering
GPIO.setmode(GPIO.BCM)

# Set GPIO for camera LED
CAMLED = 5

# Set GPIO to output
GPIO.setup(CAMLED, GPIO.OUT, initial=False)

GPIO.output(CAMLED,False) # Off

