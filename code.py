# SmartCW Copyright 2023 Joeri Van Dooren (ON3URE)

# based on xiaoKey

# xiaoKey - a computer connected iambic keyer
# Copyright 2022 Mark Woodworth (AC9YW)
# https://github.com/MarkWoodworth/xiaokey/blob/master/code/code.py

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

import time
import board
import digitalio
import pwmio
from digitalio import DigitalInOut, Direction, Pull
import usb_cdc
import usb_hid
from adafruit_hid.keyboard import Keyboard
from adafruit_hid.keyboard_layout_us import KeyboardLayoutUS
from adafruit_hid.keycode import Keycode
import usb_midi
import adafruit_midi
from adafruit_midi.note_off import NoteOff
from adafruit_midi.note_on import NoteOn
import config

# User config
WPM = config.WPM
SIDETONE = config.SIDETONE
SIDEFREQ = config.SIDEFREQ

# Vars
KEYBOARD = False

# setup buzzer (set duty cycle to ON to sound)
buzzer = pwmio.PWMOut(board.GP24,variable_frequency=True)
buzzer.frequency = SIDEFREQ
OFF = 0
ON = 2**15

# setup midi
midi = adafruit_midi.MIDI(
        midi_in=usb_midi.ports[0], in_channel=0, midi_out=usb_midi.ports[1], out_channel=0 )

# leds
pwrLED = digitalio.DigitalInOut(board.GP3)
pwrLED.direction = digitalio.Direction.OUTPUT
pwrLED.value = False

dahLED = digitalio.DigitalInOut(board.GP8)
dahLED.direction = digitalio.Direction.OUTPUT
dahLED.value = True

ditLED = digitalio.DigitalInOut(board.GP11)
ditLED.direction = digitalio.Direction.OUTPUT
ditLED.value = True

serLED = digitalio.DigitalInOut(board.GP10)
serLED.direction = digitalio.Direction.OUTPUT
serLED.value = True

serLED2 = digitalio.DigitalInOut(board.GP7)
serLED2.direction = digitalio.Direction.OUTPUT
serLED2.value = True

def led(what):
    if what=='dit':  
        ditLED.value = False
    if what=='ditOFF':
        ditLED.value = True
    if what=='dah':  
        dahLED.value = False
    if what=='dahOFF':
        dahLED.value = True
    if what=='serial':
        ditLED.value = False
        serLED.value = False
        serLED2.value = False
    if what=='serialOFF':
        ditLED.value = True
        serLED.value = True
        serLED2.value = True
    if what=='pwr':  
        pwrLED.value = False
    if what=='pwrOFF':
        pwrLED.value = True

# setup keyer output
#key = DigitalInOut(board.GP25) 
#key.direction = Direction.OUTPUT

# setup paddle inputs
dit_key = DigitalInOut(board.GP13)
dit_key.direction = Direction.INPUT
dit_key.pull = Pull.UP
dah_key = DigitalInOut(board.GP12)
dah_key.direction = Direction.INPUT
dah_key.pull = Pull.UP

# setup push buttons
pttBTN = DigitalInOut(board.GP16)
pttBTN.direction = Direction.INPUT
pttBTN.pull = Pull.UP

pttINPUT = DigitalInOut(board.GP15)
pttINPUT.direction = Direction.INPUT
pttINPUT.pull = Pull.UP

# keyboard mode
if pttBTN.value is False:
    KEYBOARD=True

# setup usb serial
serial = usb_cdc.data

# setup keyboard output
time.sleep(1)  
keyboard = Keyboard(usb_hid.devices)
keyboard_layout = KeyboardLayoutUS(keyboard)

# setup encode and decode
encodings = {}
def encode(char):
    global encodings
    if char in encodings:
        return encodings[char]
    elif char.lower() in encodings:
        return encodings[char.lower()]
    else:
        return ''

decodings = {}
def decode(char):
    global decodings
    if char in decodings:
        return decodings[char]
    else:
        #return '('+char+'?)'
        return '??'

def MAP(pattern,letter):
    decodings[pattern] = letter
    encodings[letter ] = pattern
    
MAP('.-'   ,'a') ; MAP('-...' ,'b') ; MAP('-.-.' ,'c') ; MAP('-..'  ,'d') ; MAP('.'    ,'e')
MAP('..-.' ,'f') ; MAP('--.'  ,'g') ; MAP('....' ,'h') ; MAP('..'   ,'i') ; MAP('.---' ,'j')
MAP('-.-'  ,'k') ; MAP('.-..' ,'l') ; MAP('--'   ,'m') ; MAP('-.'   ,'n') ; MAP('---'  ,'o')
MAP('.--.' ,'p') ; MAP('--.-' ,'q') ; MAP('.-.'  ,'r') ; MAP('...'  ,'s') ; MAP('-'    ,'t')
MAP('..-'  ,'u') ; MAP('...-' ,'v') ; MAP('.--'  ,'w') ; MAP('-..-' ,'x') ; MAP('-.--' ,'y')
MAP('--..' ,'z')
              
MAP('.----','1') ; MAP('..---','2') ; MAP('...--','3') ; MAP('....-','4') ; MAP('.....','5')
MAP('-....','6') ; MAP('--...','7') ; MAP('---..','8') ; MAP('----.','9') ; MAP('-----','0')

MAP('.-.-.-','.') # period
MAP('--..--',',') # comma
MAP('..--..','?') # question mark
MAP('-...-', '=') # equals, also /BT separator
MAP('-....-','-') # hyphen
MAP('-..-.', '/') # forward slash
MAP('.--.-.','@') # at sign

MAP('-.--.', '(') # /KN over to named station
MAP('.-.-.', '+') # /AR stop (end of message)
MAP('.-...', '&') # /AS wait
MAP('...-.-','|') # /SK end of contact
MAP('...-.', '*') # /SN understood
MAP('.......','#') # error

# key down and up
def cw(on):
    if on:
        # key.value = True
        midi.send(NoteOn(65,0))
        if SIDETONE:
           buzzer.duty_cycle = ON
    else:
        # key.value = False
        midi.send(NoteOff(65,0))
        buzzer.duty_cycle = OFF

# ptt on/off    
def ptt(on):
    if on:
        led('pwrOFF')
        led('dit')
        midi.send(NoteOn(66,0))
        time.sleep(.15)  
        led('dah')
        midi.send(NoteOff(66,0))
        time.sleep(.15)  
        led('pwr')
        time.sleep(.15)  
        led('pwrOFF')
        time.sleep(.15)  
        led('dahOFF')
        time.sleep(.15)  
        led('ditOFF')
        time.sleep(.15)  
        led('pwr')

# timing
def dit_time():
    global WPM
    PARIS = 50 
    return 60.0 / WPM / PARIS

# send to computer
def send(c):
#   print(c,end='')
    if serial.connected:
       serial.write(str.encode(c))
    if KEYBOARD:
        keyboard_layout.write(c)
        
# transmit pattern
def play(pattern):
    for sound in pattern:
        if sound == '.':
            cw(True)
            time.sleep(dit_time())
            cw(False)
            time.sleep(dit_time())
        elif sound == '-':
            cw(True)
            time.sleep(3*dit_time())
            cw(False)
            time.sleep(dit_time())
        elif sound == ' ':
            time.sleep(4*dit_time())
    time.sleep(2*dit_time())

## send and play message
#def xmit(message):
#    led('xmit')
#    for letter in message:
#        send(letter)
#        play(encode(letter))
#    play(' ')
#    led('xmitOFF')
    
# send and play memories on button presses
def buttons():
    global pttBTN, pttINPUT
    if not pttBTN.value:
        ptt(True)
    if not pttINPUT.value:
        ptt(True)
        
# receive, send, and play keystrokes from computer
def serials():
    if serial.connected:
        if serial.in_waiting > 0:
            led('serial') 
            letter = serial.read().decode('utf-8')
            send(letter)
            play(encode(letter))
            led('serialOFF')

# decode iambic b paddles
class Iambic:
    def __init__(self,dit_key,dah_key):
        self.dit_key = dit_key
        self.dah_key = dah_key
        self.dit = False ; self.dah = False
        self.SPACE=0 ; self.DIT=1 ; self.DIT_WAIT=2 ; self.DAH=3 ; self.DAH_WAIT=4
        self.state = self.SPACE
        self.in_char = False ; self.in_word = False
        self.start = 0
        self.char = ''
    def hack(self):
        self.start = time.monotonic()
    def elapsed(self):
        return time.monotonic() - self.start
    def set_state(self, new_state):
        self.hack()
        self.state = new_state
    def latch_paddles(self):
        if not self.dit_key.value:
            self.dit = True
        if not self.dah_key.value:
            self.dah = True
    def start_dit(self):
        self.dit = False    ; self.dah = False
        self.in_char = True ; self.in_word = True
        self.char += "."
        cw(True)
        self.set_state(self.DIT)
        led('dit')
    def start_dah(self):
        self.dit = False    ; self.dah = False
        self.in_char = True ; self.in_word = True
        self.char += "-"
        cw(True)
        self.set_state(self.DAH)
        led('dah')
    def cycle(self):
        self.latch_paddles()
        if self.state == self.SPACE:
            if self.dit:
                self.start_dit()
            elif self.dah:
                self.start_dah()
            elif self.in_char and self.elapsed()>2*dit_time():
                self.in_char = False
                send(decode(self.char))
                self.char = ""
                led('ditOFF')
                led('dahOFF')
            elif self.in_word and self.elapsed()>6*dit_time():
                self.in_word = False
                send(" ")
                led('ditOFF')
                led('dahOFF')
        elif self.state == self.DIT:
            if self.elapsed() > dit_time():
                cw(False)
                self.dit = False
                self.set_state(self.DIT_WAIT)
        elif self.state == self.DIT_WAIT:
            if self.elapsed() > dit_time():
                if self.dah:
                    self.start_dah()
                elif self.dit:
                    self.start_dit()
                else:
                    self.set_state(self.SPACE)
        elif self.state == self.DAH:
            if self.elapsed() > 3*dit_time():
                cw(False)
                self.dah = False
                self.set_state(self.DAH_WAIT)
        elif self.state == self.DAH_WAIT:
            if self.elapsed() > dit_time():
                if self.dit:
                    self.start_dit()
                elif self.dah:
                    self.start_dah()
                else:
                    self.set_state(self.SPACE)              

# paddle instance
iambic = Iambic(dit_key,dah_key)

# run
while True:
    buttons()
    serials()
    iambic.cycle()          
