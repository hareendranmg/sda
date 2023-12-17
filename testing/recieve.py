import serial
import time

port_name = "/dev/ttyXR0"
baud_rate = 2000000

serial_port = serial.Serial(port_name, baud_rate)

if serial_port:
    try:
        while True:
            received_data = serial_port.read(200)
            print(f"Received: {received_data}")
    except KeyboardInterrupt:
        pass
    finally:
        serial_port.close()
