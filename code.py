# SmartCW Copyright 2023 Joeri Van Dooren (ON3URE)

# loosly based on xiaoKey
# especially the CW code to text mapping

# completely ASYNC
# CW output to PIEZO and line-out
# CW output to USB Keyboard emulator, Digital output (for connecting to a transceiver), Serial output (to use on a computer)
# CW input via Serial, Paddle
# Indicative LEDS
# PTT button
# config file

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
import config
import asyncio

# User config
WPM = config.WPM
SIDETONE = config.SIDETONE
SIDEFREQ = config.SIDEFREQ

# Vars
KEYBOARD = False

# setup buzzer (set duty cycle to ON to sound)
buzzer = pwmio.PWMOut(board.GP24,variable_frequency=True)
buzzer.frequency = SIDEFREQ

# setup buzzer (set duty cycle to ON to sound)
lineout = pwmio.PWMOut(board.GP15,variable_frequency=True)
lineout.frequency = SIDEFREQ

# setup CW out
cwOUT = digitalio.DigitalInOut(board.GP14)
cwOUT.direction = digitalio.Direction.OUTPUT
cwOUT.value = True

OFF = 0
ON = 2**15


# setup midi
hasMidi = False
midi = None
if config.enableMidi is True:
	import usb_midi
	import adafruit_midi
	from adafruit_midi.note_off import NoteOff
	from adafruit_midi.note_on import NoteOn

	if len(usb_midi.ports) is not 0:
		midi = adafruit_midi.MIDI(
			midi_in=usb_midi.ports[0], in_channel=0, midi_out=usb_midi.ports[1], out_channel=0 )
		hasMidi = True

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

async def led(what):
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

# keyboard mode
if pttBTN.value is False:
	KEYBOARD=True
	import usb_hid
	from adafruit_hid.keyboard import Keyboard
	from adafruit_hid.keyboard_layout_us import KeyboardLayoutUS
	from adafruit_hid.keycode import Keycode
	# setup keyboard output
	time.sleep(1)  
	keyboard = Keyboard(usb_hid.devices)
	keyboard_layout = KeyboardLayoutUS(keyboard)

# setup usb serial
serial = usb_cdc.data


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
        return '¿'

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
async def cw(on):
    if on:
        # key.value = True
        if hasMidi:
            midi.send(NoteOn(65,0))
        if SIDETONE:
           buzzer.duty_cycle = ON
           lineout.duty_cycle = ON
           cwOUT.value = True
    else:
        # key.value = False
        if hasMidi:
            midi.send(NoteOff(65,0))
        buzzer.duty_cycle = OFF
        lineout.duty_cycle = OFF
        cwOUT.value = False

# ptt on/off    
async def ptt(on):
    if on:
        await led('pwrOFF')
        await led('dit')
        if hasMidi:
            midi.send(NoteOn(66,0))
        await asyncio.sleep(.15)  
        await led('dah')
        if hasMidi:
            midi.send(NoteOff(66,0))
        await asyncio.sleep(.15)  
        await led('pwr')
        await asyncio.sleep(.15)  
        await led('pwrOFF')
        await asyncio.sleep(.15)  
        await led('dahOFF')
        await asyncio.sleep(.15)  
        await led('ditOFF')
        await asyncio.sleep(.15)  
        await led('pwr')

# timing
def dit_time():
    global WPM
    PARIS = 50 
    return 60.0 / WPM / PARIS

# send to computer
async def send(c):
#   print(c,end='')
    if serial is not None:
        if serial.connected:
            serial.write(str.encode(c))
    if KEYBOARD:
        keyboard_layout.write(c)
        
# transmit pattern
async def play(pattern):
    for sound in pattern:
        if sound == '.':
            await cw(True)
            await asyncio.sleep(dit_time())
            await cw(False)
            await asyncio.sleep(dit_time())
        elif sound == '-':
            await cw(True)
            await asyncio.sleep(3*dit_time())
            await cw(False)
            await asyncio.sleep(dit_time())
        elif sound == ' ':
            await asyncio.sleep(4*dit_time())
    await asyncio.sleep(2*dit_time())
    
# send and play memories on button presses
async def buttons():
    global pttBTN
    if not pttBTN.value:
        await ptt(True)
        
# receive, send, and play keystrokes from computer
async def serials():
    if serial is not None:
        if serial.connected:
            if serial.in_waiting > 0:
                await led('serial') 
                letter = serial.read().decode('utf-8')
                await send(letter)
                await play(encode(letter))
                await led('serialOFF')

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
        return None
    def hack(self):
        self.start = time.monotonic()
    def elapsed(self):
        return time.monotonic() - self.start
    def set_state(self, new_state):
        self.hack()
        self.state = new_state
    async def latch_paddles(self):
        if not self.dit_key.value:
            self.dit = True
        if not self.dah_key.value:
            self.dah = True
    async def start_dit(self):
        self.dit = False    ; self.dah = False
        self.in_char = True ; self.in_word = True
        self.char += "."
        await cw(True)
        self.set_state(self.DIT)
        await led('dit')
    async def start_dah(self):
        self.dit = False    ; self.dah = False
        self.in_char = True ; self.in_word = True
        self.char += "-"
        await cw(True)
        self.set_state(self.DAH)
        await led('dah')
    async def cycle(self):
        await self.latch_paddles()
        if self.state == self.SPACE:
            if self.dit:
                await self.start_dit()
            elif self.dah:
                await self.start_dah()
            elif self.in_char and self.elapsed()>2*dit_time():
                self.in_char = False
                await send(decode(self.char))
                self.char = ""
                await led('ditOFF')
                await led('dahOFF')
            elif self.in_word and self.elapsed()>6*dit_time():
                self.in_word = False
                await send(" ")
                await led('ditOFF')
                await led('dahOFF')
        elif self.state == self.DIT:
            if self.elapsed() > dit_time():
                await cw(False)
                self.dit = False
                self.set_state(self.DIT_WAIT)
        elif self.state == self.DIT_WAIT:
            if self.elapsed() > dit_time():
                if self.dah:
                    await self.start_dah()
                elif self.dit:
                    await self.start_dit()
                else:
                    self.set_state(self.SPACE)
        elif self.state == self.DAH:
            if self.elapsed() > 3*dit_time():
                await cw(False)
                self.dah = False
                self.set_state(self.DAH_WAIT)
        elif self.state == self.DAH_WAIT:
            if self.elapsed() > dit_time():
                if self.dit:
                    await self.start_dit()
                elif self.dah:
                    await self.start_dah()
                else:
                    self.set_state(self.SPACE)              


async def iambic_runner(iambic):
    print("Iambic task")
    while True:
        await iambic.cycle()
        await asyncio.sleep(0)


async def buttons_runner():
    print("Buttons task")
    while True:
       await buttons()
       await asyncio.sleep(0)

async def serials_runner():
    print("Serials task")
    while True:
       await serials()
       await asyncio.sleep(0)

async def main():  # Don't forget the async!
   iambic = Iambic(dit_key,dah_key)
   iambic_task = asyncio.create_task(iambic_runner(iambic))
   serials_task = asyncio.create_task(serials_runner())
   buttons_task = asyncio.create_task(buttons_runner())
   await asyncio.gather(iambic_task, serials_task, buttons_task)


asyncio.run(main())
        
