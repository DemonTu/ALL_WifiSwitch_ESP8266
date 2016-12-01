#!/bin/bash

echo "gen_misc.sh version 20161116"
echo ""

boot=new
app=1
spi_speed=80
spi_mode=DOUT
spi_size_map=4

echo ""
touch user/user_main.c
echo ""
echo "start..."

echo ""
make COMPILE=gcc BOOT=$boot APP=$app SPI_SPEED=$spi_speed SPI_MODE=$spi_mode SPI_SIZE_MAP=$spi_size_map
