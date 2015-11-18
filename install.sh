#!/bin/sh
set -e

# Setup machine
sudo apt-get update
sudo apt-get upgrade -y
sudo apt-get install git build-essential -y

# Install CMake
sudo apt-get install software-properties-common -y
sudo add-apt-repository ppa:george-edison55/cmake-3.x -y
sudo apt-get update
sudo apt-get install cmake -y

git clone https://github.com/llvm-mirror/llvm.git
cd llvm
mkdir build
cd build
cmake ..
cmake --build .
sudo cmake --build . --target install
cd ~

git clone https://github.com/llvm-mirror/clang.git
cd clang
mkdir build
cd build
cmake ..
make
sudo make install
cd ~
