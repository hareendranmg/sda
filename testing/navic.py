import serial
import time

# port_name1 = "/dev/ttyXR0"
port_name1 = "/dev/ttyXR7"
baud_rate = 2000000

# Corrected the variable name
serial_port1 = serial.Serial(port_name1, baud_rate, parity="O", timeout=1)
# serial_port2 = serial.Serial(port_name2, baud_rate, parity="O", timeout=0.01)

if serial_port1 :
    try:
        while True:
            command = bytearray()

            command.append(0x0F)
            serial_port1.write(command)
            # serial_port2.write(command)
            time.sleep(0.1)

            received_data1 = serial_port1.read(22)
            # received_data2 = serial_port2.read(22)

            hex_string1 = ' '.join([format(byte, '02x') for byte in received_data1])
            # hex_string2 = ' '.join([format(byte, '02x') for byte in received_data2])

            print(f"Received1: {hex_string1}")
            # print(f"Received2: {hex_string2}")
            time.sleep(0.1)
    finally:
        # Close the serial port in a finally block to ensure it's closed even if an exception occurs
        serial_port1.close()
        # serial_port2.close()
