#!/bin/bash

#-----------General settings -------------------
ESPTOOL_PATH=/home/christian/Dokumente/esp8266/esp-open-sdk/esptool/esptool.py
CROSSCOMPILER_PATH=/home/christian/Dokumente/esp8266/esp-open-sdk/xtensa-lx106-elf/bin


#-----------Project settings -------------------
#Keep PORT empty, to not flash the chip
PORT=/dev/ttyUSB0

SDK_PATH=/home/christian/Dokumente/esp8266/myRadio

PROJECT_PATH=$SDK_PATH/mp3

BIN_PATH=$SDK_PATH/bin

BINARY_FILE_1=$BIN_PATH/eagle.flash.bin
START_BYTE_1=0x00000

BINARY_FILE_2=$BIN_PATH/eagle.irom0text.bin
START_BYTE_2=0x20000

UART_SPEED=115200
START_UART_AFTERWARDS=1


######################################################CODE#####################################################


#-----------Functions -----------------

echo_start () {
        echo "${COLOR_GREEN}---------------------------"
	echo "${COLOR_RED}$1${COLOR_RESET}"
}

echo_end () {
	echo "${COLOR_GREEN}---------------------------${COLOR_RESET}"
	echo ""
}

echo_start_output () {
	echo "${COLOR_GREEN}->->->->->->->->->->->->->->->->->->->->->->->->${COLOR_RESET}"
}

echo_end_output () {
	echo "${COLOR_GREEN}<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-${COLOR_RESET}"
}


#-----------Start compiling -------------------

#Initialize
export PATH=$PATH:$CROSSCOMPILER_PATH
export BIN_PATH=$BIN_PATH
export SDK_PATH=$SDK_PATH
cd $PROJECT_PATH
COLOR_RED=`tput setaf 1`
COLOR_GREEN=`tput setaf 2`
COLOR_RESET=`tput sgr0`

#Compile
echo_start "Compile start..."
OUTPUT=`make COMPILE=gcc BOOT=none APP=0 SPI_SPEED=40 SPI_MODE=QIO SPI_SIZE=1024`	
OUTPUT_CORRECT="$OUTPUT" | tr \n \r\n
echo_start_output
echo $OUTPUT_CORRECT
echo_end_output
echo_end

#If error occures, do not flash ESP-Chip
if ( [[ $OUTPUT == *"Fehler"* ]]  ||  [[ $OUTPUT != *"successully"* ]] )
then
	echo_start "Error occures... break!"
	echo_end
else

	if ( [ $PORT != "" ] &&  [ -e "$PORT" ] )
	then
		#Flash first file...
		echo_start "Flash first Binary-file"
		echo_start_output
		sudo python $ESPTOOL_PATH --port $PORT write_flash $START_BYTE_1 $BINARY_FILE_1
		echo_end_output
		echo_end

		#Flash second file...
		if [[ $BINARY_FILE_2 != "" ]] 
		then
			echo_start "Flash secondary Binary-file"
			echo_start_output
			sudo python $ESPTOOL_PATH --port $PORT write_flash $START_BYTE_2 $BINARY_FILE_2
			echo_end_output
			echo_end	
		fi

		#Start UART...
		if [[ $START_UART_AFTERWARDS == 1 ]]
		then
			gtkterm --port /dev/ttyUSB0 --speed $UART_SPEED
		fi
	
	fi

fi
