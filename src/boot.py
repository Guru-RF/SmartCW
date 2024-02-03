import usb_cdc
import board
import storage
import supervisor
from digitalio import DigitalInOut, Direction, Pull

supervisor.set_usb_identification("RF.Guru", "SmartCW")

# setup paddle inputs
dit_key = DigitalInOut(board.GP13)
dit_key.direction = Direction.INPUT
dit_key.pull = Pull.UP
dah_key = DigitalInOut(board.GP12)
dah_key.direction = Direction.INPUT
dah_key.pull = Pull.UP

# Disable devices only if dah/dit is not pressed.
if dit_key.value is True and dah_key.value is True:
    print(f"boot: button not pressed, disabling drive")
    storage.disable_usb_drive()
    storage.remount("/", readonly=False)

    usb_cdc.enable(console=False, data=True)
else:
    print(f"boot: button pressed, enable console, enabling drive")

    usb_cdc.enable(console=True, data=True)

    new_name = "SmartCW"
    storage.remount("/", readonly=False)
    m = storage.getmount("/")
    m.label = new_name
    storage.remount("/", readonly=True)
