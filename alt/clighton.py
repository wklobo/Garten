#!/usr/bin/env python
# clighton.py - 2014-07-25

import RPi.GPIO as GPIO

# Use GPIO numbering
GPIO.setmode(GPIO.BCM)

# Set GPIO for camera LED
CAMLED = 17

# Set GPIO to output
GPIO.setup(CAMLED, GPIO.OUT, initial=False)

GPIO.output(CAMLED,True) # On