import serial
import time

port = "/dev/ttyXR0"
baudrate = 2000000
bytesize = serial.EIGHTBITS
parity = serial.PARITY_
stopbits = serial.STOPBITS_ONE

serial_port = serial.Serial(port, baudrate, bytesize=bytesize, parity=parity, stopbits=stopbits, timeout=0.1)

if serial_port :
    try:
        command = bytearray()
        # command.append(0x2d)
        command.append(0x1a)
        command.append(0x2a)
        while True:
            serial_port.write(command)
            time.sleep(0.01)
            received_data = serial_port.read(22)
            hex_string = ' '.join([format(byte, '02x') for byte in received_data])
            print(f"0F: {hex_string}")
            time.sleep(0.1)
    finally:
        serial_port.close()
