#!/bin/bash

## Debian/Ubuntu installation script

##
## Install g++, MinGW 32 and 64 bits
## and compile zlib, openssl, ssh2 and curl for MinGW
##

# Only root can run this script
if [ "$(id -u)" != "0" ]; then
   echo "This script must be run as root" 1>&2
   exit 1
fi

# Ubuntu packages
sudo apt install g++ libcurl4-gnutls-dev libssl-dev libssh2-1-dev zlib1g-dev libncurses5-dev mingw-w64

cd /tmp

# ZLIB:
wget http://zlib.net/zlib-1.2.11.tar.gz -O- | tar xfz -
cd zlib*
# zlib 'configure' script is currently broken, use win32/Makefile.gcc directly
# MinGW 32bits:
sed -e s/"PREFIX ="/"PREFIX = i686-w64-mingw32-"/ -i win32/Makefile.gcc # automatic replacement
make -f win32/Makefile.gcc
sudo BINARY_PATH=/usr/i686-w64-mingw32/bin \
    INCLUDE_PATH=/usr/i686-w64-mingw32/include \
    LIBRARY_PATH=/usr/i686-w64-mingw32/lib \
    make -f win32/Makefile.gcc install
# MinGW 64bits:
sed -e s/"PREFIX = i686-w64-mingw32-"/"PREFIX = x86_64-w64-mingw32-"/ -i win32/Makefile.gcc # automatic replacement
make distclean
make -f win32/Makefile.gcc
sudo BINARY_PATH=/usr/x86_64-w64-mingw32/bin \
    INCLUDE_PATH=/usr/x86_64-w64-mingw32/include \
    LIBRARY_PATH=/usr/x86_64-w64-mingw32/lib \
    make -f win32/Makefile.gcc install
cd ..

# OPENSSL:
wget https://www.openssl.org/source/openssl-1.1.1c.tar.gz -O- | tar xfz -
cd openssl*
# MinGW 32bits:
CROSS_COMPILE="i686-w64-mingw32-" ./Configure mingw no-asm shared --prefix=/usr/i686-w64-mingw32
make
sudo make install
# MinGW 64bits:
make distclean
CROSS_COMPILE="x86_64-w64-mingw32-" ./Configure mingw64 no-asm shared --prefix=/usr/x86_64-w64-mingw32
make
sudo make install
cd ..

# SSH2 (need openssl!):
wget https://www.libssh2.org/download/libssh2-1.8.2.tar.gz -O- | tar xfz -
cd libssh*
# MinGW 32bits:
./configure --host=i686-w64-mingw32 --prefix=/usr/i686-w64-mingw32
sudo make install
# MinGW 64bits:
make distclean
./configure --host=x86_64-w64-mingw32 --prefix=/usr/x86_64-w64-mingw32
sudo make install
cd ..

# CURL (with ssh2 support):
wget https://curl.haxx.se/download/curl-7.65.1.tar.gz -O- | tar xfz -
cd curl*
# MinGW 32bits:
./configure --host=i686-w64-mingw32 --build=i686-pc-linux-gnu --prefix=/usr/i686-w64-mingw32/ --enable-static --disable-shared --with-libssh2=/usr/i686-w64-mingw32
make
sudo make install
# MinGW 64bits:
make distclean
./configure --host=x86_64-w64-mingw32 --build=x86_64-pc-linux-gnu --prefix=/usr/x86_64-w64-mingw32/ --enable-static --disable-shared --with-libssh2=/usr/x86_64-w64-mingw32
make
sudo make install
cd ..
