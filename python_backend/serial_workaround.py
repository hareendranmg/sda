import serial
import sys
import time

def open_serial_port(serial_port_name, baud_rate, parity, data_bits, stop_bits):
    try:

        # print all parameters
        print(f"Serial port name: {serial_port_name}")
        print(f"Baud rate: {baud_rate}")
        print(f"Parity: {parity}")
        print(f"Data bits: {data_bits}")
        print(f"Stop bits: {stop_bits}")

        ser = serial.Serial(
            port=serial_port_name,
            baudrate=baud_rate,
            parity=parity,
            bytesize=data_bits,
            stopbits=stop_bits,
            timeout=1  # Set timeout for read operations (in seconds)
        )

        print(f"Serial port {serial_port_name} opened successfully.")
        time.sleep(0.1)  # Hold the port open for 0.1 seconds

    except serial.SerialException as e:
        print(f"Error opening serial port: {e}")
        sys.exit(1)

    finally:
        ser.close()
        print(f"Serial port {serial_port_name} closed.")

if __name__ == "__main__":
    if len(sys.argv) != 6:
        print("Usage: python3 serial_workaround.py serialPortName baudRate parity dataBits stopBits")
        sys.exit(1)

    serial_port_name = sys.argv[1]
    baud_rate = int(sys.argv[2])
    parity = sys.argv[3]
    data_bits = int(sys.argv[4])
    stop_bits = int(sys.argv[5])

    open_serial_port(serial_port_name, baud_rate, parity, data_bits, stop_bits)
