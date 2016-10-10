#include "esp_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "lwip/api.h"

#include "playerconfig.h"
#include "channel.h"
#include "wifi.h"
#include "flash.h"


//Define used Flash-address
#define FLASH_ADDRESS 0x075000
#define FLASH_SEC_COUNT 6

//Define header, which indecates valid available datas at flash
#define HEADER 18181818

//Copy-settings:
#define MAX_CHAR_LEN 512

//Erase used flash-sectors
int flash_erase_sector(){
	
	int i = 0;
	int ret = 0;
	for(i = 0; i < FLASH_SEC_COUNT; i++){
		ret += spi_flash_erase_sector(FLASH_ADDRESS / SPI_FLASH_SEC_SIZE + i);
	}

	return ret;


}


//Write config to flash
int flash_writeConfig(){

	int ret = 0;

	//Empty flash
	if(flash_erase_sector() != 0){
		printf("flash - Err erase flash\n");
		ret = -1;
	}

	int flash_count = 0;	

	//Header
	if(flash_write_header(&flash_count) !=0){
		printf("flash - Err write header\n");
		ret = -1;
	}

	//WifiSSID
	char* ssid = wifi_getSSID();
	if(flash_write_string(ssid, &flash_count) != 0){
		printf("flash - Err write ssid\n");
		ret = -1;
	}

	//WifiPass
	char* pass = wifi_getPass();
	if(flash_write_string(pass, &flash_count) != 0){
		printf("flash - Err write pass\n");
		ret = -1;
	}
	
	//Stream Count
	int streamCount = channel_getStreamCount();
	if(flash_write_int(&streamCount, &flash_count) != 0){
		printf("flash - Err wirte streamCount\n");
		ret = -1;
	}

	int i = 0;
	for(i = 0; i < streamCount; i++){
		char* host = channel_getStreamHost(i);
		char* path = channel_getStreamPath(i);
		int port = channel_getStreamPort(i);

		//Stream Host
		if(flash_write_string(host, &flash_count) != 0){
			printf("flash - Err wirte host %d\n", i);
			ret = -1;
		}

		//Stream Path
		if(flash_write_string(path, &flash_count) != 0){
			printf("flash - Err wirte path %d\n", i);
			ret = -1;
		}
	
		//Sream Port
		if(flash_write_int(&port, &flash_count) != 0){
			printf("flash - Err wirte port %d\n", i);
			ret = -1;
		}
	}
	
	//Check, if data can be read
	if(ret == 0)
		return flash_readConfig();

	return ret;

}


//Read config from flash
int flash_readConfig(){

	int ret = 0;
	int flash_count = 0;
	char buffer[MAX_CHAR_LEN];
	int len = 0;
	int i = 0;
	uint32 int_buf;

	//Read Header	
	if(flash_read_header(&flash_count) !=0){
		printf("flash - Err read header\n");
		return -1;
	}

	//Read SSID
	if(flash_read_string(buffer, &flash_count, &len) != 0){
		printf("flash - Err read SSID\n");
		return -1;
	}
	wifi_setSSID(buffer, len);

	//Read Password
	if(flash_read_string(buffer, &flash_count, &len) != 0){
		printf("flash - Err read Pass\n");
		return -1;
	}
	wifi_setPass(buffer, len);


	//StreamCount
	if(flash_read_int(&int_buf, &flash_count) != 0){
		printf("flash - Err read StreamCount\n");
		return -1;	
	}
	channel_setStreamCount(int_buf);

	for(i = 0; i < channel_getStreamCount(); i++){

		//Read Host
		if(flash_read_string(buffer, &flash_count, &len) != 0){
			printf("flash - Err read Host %d\n", i);
			return -1;
		}
		channel_setStreamHost(i, buffer, len);

		//Read Path
		if(flash_read_string(buffer, &flash_count, &len) != 0){
			printf("Err read Path %d\n", i);
			return -1;
		}
		channel_setStreamPath(i, buffer, len);
		
		//Read Port
		if(flash_read_int(&int_buf, &flash_count) != 0){
			printf("Err read StreamCount\n");
			return -1;	
		}
		channel_setStreamPort(i, int_buf);

	}

	return 0;

}


/********************************************************************************************/
//Low-level functions

//Write header
int flash_write_header(int* flash_offset){

	int ret;
	
	if(*flash_offset + 4  > SPI_FLASH_SEC_SIZE * FLASH_SEC_COUNT)
		return -1;

	uint32 header = HEADER;

	ret = spi_flash_write(FLASH_ADDRESS + *flash_offset, &header, 4);
	*flash_offset += 4;

	return ret;
}


//Write integer
int flash_write_int(int* ptr, int* flash_offset){

	int ret;
	
	if(*flash_offset + 4  > SPI_FLASH_SEC_SIZE * FLASH_SEC_COUNT)
		return -1;
	
	ret = spi_flash_write(FLASH_ADDRESS + *flash_offset, (uint32*)ptr, 4);
	*flash_offset += 4;

	return ret;
}


//Write string
int flash_write_string(char* ptr, int* flash_offset){

	char buf[4];
	
	int i = 0;	
	int buf_cnt = 0;
	int ret;

	bool ready = false;

	while(ready == false){

		buf[buf_cnt++]=ptr[i++];

		//minimum wirte-size == 4
		if(buf_cnt >= 4){

			if(*flash_offset + 4  > SPI_FLASH_SEC_SIZE * FLASH_SEC_COUNT)
				return -1;

			if(spi_flash_write(FLASH_ADDRESS + *flash_offset, (uint32*)buf, 4) != 0)
				return -1;
			*flash_offset += 4;
			buf_cnt = 0;
		}
		
		//if end of string is detected ('\0'), stop writing to flash, '\0' has to be writen to flash
		if(ptr[i-1] == '\0')
			ready = true;
		
	}

	//if there are still data in write-buffer availabe, write by filling up with '\0'
	if(buf_cnt > 0){
		while(buf_cnt <= 4){
			buf[buf_cnt++] = '\0';
		}
		
		//minimum wirte-size == 4
		if(*flash_offset + 4  > SPI_FLASH_SEC_SIZE * FLASH_SEC_COUNT)
			return -1;

		if(spi_flash_write(FLASH_ADDRESS + *flash_offset, (uint32*)buf, 4) != 0)
			return -1;
		*flash_offset += 4;
		buf_cnt = 0;
	}

	return 0;

}


//Read header and check, if header is correct
int flash_read_header(int* flash_offset){

	int ret;
	
	if(*flash_offset + 4  > SPI_FLASH_SEC_SIZE * FLASH_SEC_COUNT){
		return -1;
	}

	uint32 header;

	ret = spi_flash_read(FLASH_ADDRESS + *flash_offset, &header, 4);
	*flash_offset += 4;

	if(header != HEADER){
		printf("flash - Header-data does not match %d\n", header);
		return -1;
	}

	return 0;

}

//Read integer
int flash_read_int(int* ptr, int* flash_offset){

	int ret;
	
	if(*flash_offset + 4  > SPI_FLASH_SEC_SIZE * FLASH_SEC_COUNT)
		return -1;
	
	ret = spi_flash_read(FLASH_ADDRESS + *flash_offset, (uint32*)ptr, 4);
	*flash_offset += 4;

	return ret;
}


//Read string
int flash_read_string(char* ptr, int* flash_offset, int* len){

	char buf[4];
	int i = 0;	
	int buf_cnt = 0;
	int ret = 0;

	bool ready = false;
	
	while(ready == false){

		if(*flash_offset + 4  > SPI_FLASH_SEC_SIZE * FLASH_SEC_COUNT){
			return -1;
		}

		//minimum read-size == 4
		if(spi_flash_read(FLASH_ADDRESS + *flash_offset, (uint32*)buf, 4) != 0)
			return -1;

		*flash_offset += 4;

		//copy read data to read-buffer
		for(i = 0; i < 4; i++){

			if(buf_cnt >= MAX_CHAR_LEN)
			{
				return -1;
			}
			ptr[buf_cnt++] = buf[i];
			
			//Stop reading if '\0' is detected
			if(buf[i] == '\0'){
				ready = true;
				break;
			}	
		}


	}

	*len = buf_cnt;

	return ret;


}






