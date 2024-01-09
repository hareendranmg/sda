import serial
import time

port_name = "/dev/ttyXR0"
baud_rate = 2000000

# Corrected the variable name
serial_port = serial.Serial(port_name, baud_rate, parity="O", timeout=1)
if serial_port:
    try:
        while True:
            command = bytearray()

            command.append(0x0F)
            serial_port.write(command)
            # time.sleep(0.1)

            received_data = serial_port.read(22)
            hex_string = ' '.join([format(byte, '02x') for byte in received_data])

            print(f"Received: {hex_string}")
            # time.sleep(0.1)
    finally:
        # Close the serial port in a finally block to ensure it's closed even if an exception occurs
        serial_port.close()
