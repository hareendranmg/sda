# connmanctl services
# connmanctl config ethernet_XXXX_YYYY --ipv4 manual 192.168.10.100 255.255.255.0 192.168.10.1

#!/bin/bash

# Install micro
curl https://getmic.ro | bash && mv micro /usr/bin

# Disable HDMI and enable LVDS display
echo "Disabling HDMI and enabling LVDS display..."
echo "fdt_overlays=apalis-imx8_panel-cap-touch-10inch-lvds_overlay.dtbo apalis-imx8_spi1_spidev_overlay.dtbo apalis-imx8_spi2_spidev_overlay.dtbo" | tee /boot/overlays.txt

# Execute sync
sync

# Disable Desktop bar/Top status bar
echo "Disabling Desktop bar/Top status bar..."
echo -e "[shell]\npanel-position=none" | tee -a /etc/xdg/weston/weston.ini

# Restart weston service
systemctl restart weston.service

# Set Date and Time
echo "Setting Date and Time..."
timedatectl set-timezone Asia/Kolkata

# Install Pyserial
echo "Installing Pyserial..."
pip3 install pyserial

# Clone sda support library at /home/root location
# echo "Cloning sda support library..."
# git clone https://github.com/hareendranmg/sda.git /home/root/sda

# Reboot if all tasks are done correctly
read -p "All tasks completed successfully. Do you want to reboot now? (y/n): " choice
if [ "$choice" == "y" ]; then
  echo "Rebooting..."
  reboot
else
  echo "Please reboot your system manually when ready."
fi
