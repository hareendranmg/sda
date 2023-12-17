#!/bin/bash

# Check if the correct number of arguments is provided
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <source_file.c>"
    exit 1
fi

# Extract the filename and remove the .c extension
filename=$(basename "$1")
filename_no_extension="${filename%.*}"

# rm data_dump
# Compile the C program to ./data_dump
gcc -o "./$filename_no_extension" "$1"

# Check if the compilation was successful
if [ $? -eq 0 ]; then
    echo "Compilation successful. Executable: ./$filename_no_extension"
else
    echo "Compilation failed."
fi


#  ./data_dump /dev/ttyXR0 1.txt 2000000 N 8 1