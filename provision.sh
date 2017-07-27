#!/bin/bash

# Update to make sure we have the latest packages
sudo apt-get update
sudo apt-get -y autoremove

# Tools nessesary for both composite and the cFE
sudo apt-get -y install make

# Tools nessesary for the cFE
sudo apt-get -y install cmake

# Tools nessesary for the cFE tools
sudo apt-get -y install python-qt4
sudo apt-get -y install pyqt4-dev-tools

# Tools nessesary for composite
sudo apt-get -y install bc
sudo apt-get -y install gcc-multilib
sudo apt-get -y install binutils-dev
sudo apt-get -y install qemu-kvm

# Useful debugging tools
sudo apt-get -y install systemtap

# Useful general purpose tools
sudo apt-get -y install git
sudo apt-get -y install ntp

# Link .bash_aliases to the cFE2cos script
ln -s /home/vagrant/cFE2cos/cFE2cos.sh /home/vagrant/.bash_aliases
