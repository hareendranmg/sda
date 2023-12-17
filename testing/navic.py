import serial
import time

port_name = "/dev/ttyXR6"
baud_rate = 1000000

# Corrected the variable name
serial_port = serial.Serial(port_name, baud_rate)

if serial_port:
    try:
        Msg1 = bytearray()
        Msg2 = bytearray()

        # Msg1.append(0xA5)
        # serial_port.write(Msg1)
        # time.sleep(0.5)        

        Msg2.append(0x0F)
        serial_port.write(Msg2)
        # time.sleep(0.1)

        received_data = serial_port.read(40)
        hex_string = ' '.join([format(byte, '02x') for byte in received_data])

        print(f"Received: {hex_string}")
    finally:
        # Close the serial port in a finally block to ensure it's closed even if an exception occurs
        serial_port.close()
