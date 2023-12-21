#!/bin/bash

# Check if the correct number of arguments is provided
if [ "$#" -ne 5 ]; then
    echo "Usage: $0 <ip_address> <netmask> <gateway> <dns> <alternate_dns>"
    exit 1
fi

ethernet_service=$(connmanctl services | awk '/\*AR Wired/ {print $3}')
ip_address="$1"
netmask="$2"
gateway="$3"
dns="$4"
alternate_dns="$5"

# Build the connmanctl command
connmanctl_command="connmanctl config $ethernet_service --ipv4 manual $ip_address $netmask $gateway --nameservers $dns $alternate_dns"

# Execute the connmanctl command
eval "$connmanctl_command"
