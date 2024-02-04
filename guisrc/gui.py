import config
import PySimpleGUI as sg
import serial

ser = serial.Serial()


def sendCW(data):
    global ser
    if not ser.isOpen():
        ser = serial.Serial(port=config.SerialPort, baudrate=4000000)

    ser.write(bytes(data, "ascii"))


def readCW():
    global ser
    if not ser.isOpen():
        ser = serial.Serial(port=config.SerialPort, baudrate=4000000)

    data = bytes()
    while ser.inWaiting() > 0:
        data = data + ser.read(1)

    return data.decode("utf-8")


sg.theme(config.Theme)  # Add a touch of color

layout = []
l1 = sg.Text(
    "Type CW",
    key="-IN-",
    font=("Arial Bold", 20),
    expand_x=True,
    justification="left",
)
t1 = sg.Input(
    "",
    enable_events=True,
    key="-INPUT-",
    font=("Arial", 20),
    expand_x=True,
    justification="left",
)
l2 = sg.Text(
    "Read CW",
    key="-OUT-",
    font=("Arial Bold", 20),
    expand_x=True,
    justification="left",
)
t2 = sg.Input(
    "",
    enable_events=True,
    key="-OUTPUT-",
    font=("Arial", 20),
    expand_x=True,
    justification="left",
    readonly=True,
)

layout = [[l1], [t1], [l2], [t2]]

# Create the Window
windowTitle = "RF.Guru SmartCW"
window = sg.Window(
    windowTitle, layout, keep_on_top=True, grab_anywhere=True, finalize=True
)
window["-INPUT-"].bind("<BackSpace>", "_Bs")
window["-INPUT-"].bind("<Return>", "_Enter")
# Event Loop to process "events" and get the "values" of the inputs
lastchar = ""
lastlen = 0
while True:
    event, values = window.read(timeout=10)
    if len(values["-OUTPUT-"]) > 55:
        window["-OUTPUT-"].update(values["-OUTPUT-"][10:] + readCW())
    else:
        window["-OUTPUT-"].update(values["-OUTPUT-"] + readCW())
    if event == sg.WIN_CLOSED:
        break
    elif event == "-INPUT-" + "_Enter":
        window["-INPUT-"].update("")
    elif event == "-INPUT-" + "_Bs":
        if len(values["-INPUT-"]) != 0:
            window["-INPUT-"].update(values["-INPUT-"] + lastchar)
    elif event == "-INPUT-":
        if len(values["-INPUT-"]) != 0:
            if lastlen != len(values["-INPUT-"]):
                lastchar = str(values["-INPUT-"][-1])
                lastlen = len(values["-INPUT-"])
                sendCW(values["-INPUT-"][-1])
        else:
            lastlen = len(values["-INPUT-"])
            sendCW(values["-INPUT-"])

window.close()
