import serial
import time

port_name = "/dev/ttyXR0"
# port_name = "/dev/ttyXR7"
baud_rate = 1000000

# Corrected the variable name
serial_port = serial.Serial(port_name, baud_rate, parity="M", timeout=0.1)
# open a file named data.csv
file = open("data.csv", "w")

if serial_port :
    print(f"Serial port {port_name} opened successfully")
    try:
        command1 = bytearray()
        command1.append(0x4B)
        command2 = bytearray()
        command2.append(0x41)
        command3 = bytearray()
        command3.append(0x42)
        while True:
            writed = serial_port.write(command1)
            time.sleep(0.0001)
            received_data = serial_port.read(24)
            hex_string = ''.join([format(byte, '02x') for byte in received_data])
            file.write('4B, ' + hex_string + ", ")

            writed = serial_port.write(command2)
            time.sleep(0.0001)
            received_data = serial_port.read(24)
            hex_string = ''.join([format(byte, '02x') for byte in received_data])
            file.write('41, ' + hex_string + ", ")

            writed = serial_port.write(command3)
            time.sleep(0.0001)
            received_data = serial_port.read(20)
            hex_string = ''.join([format(byte, '02x') for byte in received_data])
            file.write('42, ' + hex_string + "\n")
            time.sleep(0.001)
    finally:
        # Close the serial port in a finally block to ensure it's closed even if an exception occurs
        serial_port.close()