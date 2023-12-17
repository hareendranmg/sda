#!/bin/bash

# Specify the directory name
exar_dir="/home/root/xr17-lnx2.6.32-and-newer-pak"

# Check if the directory exists
if [ ! -d "$exar_dir" ]; then
  echo "Error: Directory $exar_dir does not exist."
  exit 1
fi

# Change to the directory
cd "$exar_dir" || exit 1

# Check if ttyXR* ports already exist
if ls /dev/ttyXR* 2>/dev/null; then
  echo "Info: TTY ports already exist. Exiting without further actions."
  exit 0
fi

# List exar_serial drivers in /sys/bus/pci/drivers
ls /sys/bus/pci/drivers/exar_serial/

# Find the PCI address dynamically
exar_serial_pci_address=$(ls /sys/bus/pci/drivers/exar_serial/ | grep -E '^[0-9a-fA-F]{4}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}\.[0-9a-fA-F]$')

if [ -z "$exar_serial_pci_address" ]; then
  echo "Error: Unable to find Exar serial device PCI address."
  exit 1
fi

echo -n "$exar_serial_pci_address" > /sys/bus/pci/drivers/exar_serial/unbind

# Insert the maxlinear custom kernel module
insmod xr17v35x.ko

# Check if the module is successfully inserted
if lsmod | grep -q "xr17v35x"; then
  echo "Maxlinear custom kernel module (xr17v35x) successfully inserted."

  # Show newly created serial ports
  echo "Newly created serial ports:"
  ls /dev/ttyXR*

else
  echo "Error: Failed to insert the Maxlinear custom kernel module."
fi

# You can customize the script as needed for your specific use case.
