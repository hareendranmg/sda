import serial
import time

port_name = "/dev/ttyXR0"
# port_name = "/dev/ttyXR7"
baud_rate = 2000000

# Corrected the variable name
serial_port = serial.Serial(port_name, baud_rate, parity="O", timeout=0.1)
# open a file named data.csv
file = open("data.csv", "w")

if serial_port :
    print(f"Serial port {port_name} opened successfully")
    try:
        command = bytearray()
        command.append(0x0f)
        while True:
            writed = serial_port.write(command)
            time.sleep(0.0001)
            received_data = serial_port.read(22)
            hex_string = ' '.join([format(byte, '02x') for byte in received_data])
            # write the data to the file
            file.write(hex_string + "\n")
            time.sleep(0.001)
    finally:
        # Close the serial port in a finally block to ensure it's closed even if an exception occurs
        serial_port.close()