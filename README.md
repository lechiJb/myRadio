# myRadio – ESP8266 WLAN radio

This project is based at following templates from Espressif:

ESP8266_MP3_DECODER
https://github.com/espressif/ESP8266_MP3_DECODER.git

ESP8266_RTOS_SDK
https://github.com/espressif/ESP8266_RTOS_SDK.git

The project provides a WLAN radio, based on an ESP8266-chip.
Audio-playback is streamed by use of the I2S-interface and an external I2S-DAC-chip.
All settings (stream-URL, stream-Port, WLAN-settings) are adjustable by an integrated HTML-server.

In chapter/folder “Hardware” you can find information about used hardware for this project.

Inside the project there are also CAD-file for a housing (see chapter “CAD” below) 
and simulation-files for calculating crossover network-components (see chapter “Boxsim” below). 

##Hardware
As esp-board I used following NodeMCU-Board:
https://github.com/nodemcu/nodemcu-devkit-v1.0

It has a esp12e-board integrated, but this code runs on a esp12f-board as well (other boards will also run, 
but I only tested esp12e and esp12f-boards).
Some users tested a esp12-board without success!!!!
For MP3-decoding a chip called PCM5102 is used. There are many cheep breakout-boards available. 
To select a stream-channel, a potentiometer is used. I used a linear potentiometer but a standard one is also fine.
All other used components are optional and can be replaced or removed depending of your project!

Following there is a list of all used components:
	Controller:
- NodeMCU-board https://github.com/nodemcu/nodemcu-devkit-v1.0
- PCM5102 breakout-board http://www.ebay.com/itm/Assembled-Board-PCM5102-DAC-Decoder-I2S-Player-32bit-384K-Beyond-ES9023-PCM1794-/311402292006

All following components are optional. They are used in my radio, 
but if you like to implement this code into your system, you might need other / less components 

 Amplifier:
- DROK® TA2024B amplifier-board https://www.amazon.com/DROK-Computer-Amplifier-Electric-Capacity/dp/B00C03YYCK/ref=sr_1_1?ie=UTF8&qid=1476053365&sr=8-1&keywords=DROK%C2%AE+TA2024B

Stream-Change:
- Linear potentiometer 10K mono http://www.ebay.com/itm/ALPS-Schieberegler-RSA0N12-Schiebeweg-100mm-Potentiometer-Poti-Fader-/260967189845?var=&hash=item3cc2db4d55:m:mypFES_1KwCYSBZHzkEIXgg

Power-supply:
- Salcar Power-Supply 12V 3A 36W https://www.amazon.de/gp/product/B00LFB4GC6/ref=oh_aui_detailpage_o03_s00?ie=UTF8&psc=1
- Mini DC-DC step down converter http://www.ebay.com/itm/10pcs-Mini-360-DC-DC-4-75V-23V-to-1V-17V-Buck-Converter-Step-Down-Power-supply-/272362239425?hash=item3f6a0df5c1:g:FZcAAOSw9IpXyY1d
- Slide-Switch 2-way (for disconnecting power-supply from NodeMCU in case of USB-connection) 
- Connector for power supply C6 inclusive power-switch http://www.conrad-electronic.co.uk/ce/en/product/718564/IEC-connector-C6-Plug-horizontal-mount-Total-number-of-pins-2-PE-25-A-Black-1-pcs?ref=searchDetail 

Bluetooth-connection:
- KRC-86B http://www.ebay.com/itm/KRC-86B-CSR8630-Bluetooth-4-0-Module-Stereo-Audio-Receiver-Amplifier-Board-ST-/172339536663?hash=item28203ca317:g:sIUAAOSwOdpX1pjO

Speaker
- Monacor SP40 https://www.monacor.com/de-de/monacorinternational/marken/monacor/beschallungstechnik/lautsprechertechnik/hi-fi-breitbandlautsprecher/sp-40/
- Dayton ND20FA
http://www.daytonaudio.com/index.php/nd20fa-6-3-4-neodymium-dome-tweeter.html

Crossover Network
- Mundorf air coil L100, 2,2 mH/0,83 Ohm
- Mundorf air coil  L71, 0,47 mH/0,57 Ohm
- Capacitor MKT 10,0 uF / 250Vdc
- Capacitor MKT 2,2 uF / 250Vdc
- Resistor MOX5 12 Ohm 2%
- Resistor MOX5 1,0 Ohm 2%


## Compile code
For a successful code-compiling following components are required:

Linux operation system (used for Project: Debian 9.0 Stretch, others should work as well).
Also a virtual-machine should be fine.

esp-open-sdk:
	https://github.com/pfalcon/esp-open-sdk.git

gtkterm (optional, is used for reviewing serial output for debugging):
	Run following code in a terminal-window:
		sudo apt-get install gtkterm 
	


1. Clone and compile esp-open-sdk like described in the repository.

2. Clone this repository to your local computer

3. Edit following file “<path_to_repository>/mp3/compile-script.sh” by using a standard-editor like gedit or notepad++. Following entries must be adjusted:
		
		
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


	Description:

		 ESPTOOL_PATH 		Path to esptool.py-file (used for flashing program at esp-chip)

		 CROSSCOMPILER_PATH 	Path to cross-compiler (included in esp-open-sdk)

		 PORT 			Link to Serial-Port, which is used for flashing esp-chip		 

		 SDK_PATH 		Path to esp-open-sdk

		 START_UART_AFTERWARDS	Start gtkterm after successful code-compiling. If gtkterm is not available, set this parameter to 0


	All other values are project-related and should not be changed!
		

4. Connect esp-chip to computer
5. Start code-compiling and flashing by running scirpt “compile-script.sh” with root privilegs:
	sudo ./<path_to_repository>/mp3/compile-script.sh


##How to use
Adjust settings by using html-server:

	1. switch poti to total maximum value (end-position of poti)
	2. After some seconds the esp-chip opens a wlan-hot-spot with SSID "myRadio" and no password.
	3. Connect to this hotspot
	4. Open your browser (google-chrome and firefox tested) and go to webside "http://192.168.4.1"
		
		Following adjustments are possible:

		WLAN-SSID		SSID for your home-network
		WLAN-Password		Password to your home-network

		Channel-count		Number of stream-channels, which should be used+

		For each channel:
		Link to channel		Link to stream-channel (without "http://")
		Port to channel		Port to stream-channel (often port 80)

		Save to flash		Save all settings to flash of esp-chip

	5. To exit config-server-mode, select another channel by changing poti-position
	6. Reboot device by refreshing homepage in browser or by disconnecting and connecting power-supply again
	7. Ready!!

Selecting channel
	
	Just use poti to select different channels...

Volume control
	
	Volum has to be controlled by the amplifier, which is connected to PCM5102-device.


##CAD
I designed a housing for my radio by using a web-CAD-service called OnShape:

	Link to CAD-Software:	https://www.onshape.com/
	Link to project:	https://cad.onshape.com/documents/0631abc9282a401a9c4bd0fe/w/ab4baf43f92f08b8f3445f90/e/15931d366a16f0a984bb5bd2

In folder "CAD" you can find all CAD-files and Drawings, which I created for this project.
After building all parts by use of plywook, I used some fleece inside the speaker box to avoid oscilation of echo effects.

After assembling all parts together you will need an additional front-cover for the radio. 


##Boxsim
To get really good sound, I used two speakers in my project:
	
	- Monacor SP40 
	- Dayton ND20FA
 
I also used a crossover network to optimize sound-quality. 
For simulation of the frequency-response of both speaker, I used an application called "Boxsim":

	Link to homepage: http://www.boxsim.de/	
	Link to download: http://www.boxsim.de/download/Boxsim120en.zip

In folder called "Boxsim" you can find my Boxsim-project.
To get a perfect sound, the frequency-response should be as straight as possible. 
In my project there is still a peak avalable at 5000Hz.
The sound, which I get, sounds really great, but I can heard some small "scratches" if someone speaks.
I don't know the reason for it. Maybe the esp-output is not perfect, some materials starts to oscilate, 
or the frequency-response is not perfect.
But I am really happy with the sound, so I will not optimize it any more.




 
