import serial
import time

port_name = "/dev/ttyXR6"
baud_rate = 1000000

# Corrected the variable name
serial_port = serial.Serial(port_name, baud_rate)

if serial_port:
    try:
        while True:
            # Record the start time before sending command (Msg2)
            start_time = time.time()

            # Send command (Msg2)
            Msg2 = bytearray()
            Msg2.append(0x0F)
            serial_port.write(Msg2)

            # Receive data
            received_data = serial_port.read(40)
            hex_string = ' '.join([format(byte, '02x') for byte in received_data])

            # Record the end time after receiving data
            end_time = time.time()

            # Calculate elapsed time in milliseconds
            elapsed_time_ms = (end_time - start_time) * 1000

            print(f"Received: {hex_string}")
            print(f"Elapsed Time: {elapsed_time_ms:.2f} ms")

            # Wait for a longer time before sending the next command (adjust as needed)
            # time.sleep(1)
    except KeyboardInterrupt:
        # Allow the program to be terminated by KeyboardInterrupt (e.g., Ctrl+C)
        pass
    finally:
        # Close the serial port in a finally block to ensure it's closed even if an exception occurs
        serial_port.close()
