#!/bin/bash

# Check if both date and time arguments are provided
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <date> <time>"
    exit 1
fi

# Extract date and time from arguments
input_date="$1"
input_time="$2"

# Combine date and time
datetime="${input_date} ${input_time}"

# Set system date and time
sudo date -s "$datetime"

echo "Date and time has been set to: $datetime"
