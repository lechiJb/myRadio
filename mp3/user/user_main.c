/******************************************************************************
 * Copyright 2013-2015 Espressif Systems
 *
 * FileName: user_main.c
 *
 * Description: Main routines for MP3 decoder.
 *
 * Modification history:
 *     2015/06/01, v1.0 File created.
*******************************************************************************/
/******************************************************************************
 * Modifications applied by Christian Lechner 2016/10/06
 *
*******************************************************************************/

#include "esp_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "lwip/api.h"

#include "spiram_fifo.h"
#include "playerconfig.h"
#include "webserver.h"
#include "channel.h"
#include "mad_helper.h"




//Priorities of the reader and the decoder thread. Higher = higher prio.
#define PRIO_READER 11
#define PRIO_MAD 1


//Default values for channel and Wlan
#define DEFAULT_SSID "Test_SSID"
#define DEFAULT_PASS "Test_Pass"

#define DEFAULT_CHANNEL_HOST "mp3ad.egofm.c.nmdn.net"
#define DEFAULT_CHANNEL_PATH "/ps-egofm_192/livestream.mp3"
#define DEFAULT_CHANNEL_PORT 80


//Get IP-Address for Host-Name
int ICACHE_FLASH_ATTR getIpForHost(const char *host, struct sockaddr_in *ip) {

	err_t err;
  	ip_addr_t addr;
	err = netconn_gethostbyname(host, &addr);
  	if (err != ERR_OK) {
		printf("Connect - ERR getIP: %d  %s\n", err, host);
		return 0;
	}
	ip->sin_family=AF_INET;
	memcpy(&ip->sin_addr, &addr, sizeof(ip->sin_addr));
	return 1;
}


//Connect to channel by using netconn-connection
//netconn-connections seems to be more stable and produces less sound-lags
struct netconn * ICACHE_FLASH_ATTR openConn_netconn(int streamID){

	int ret, n;
	struct ip_addr remote_ip; 
	err_t err;
  	ip_addr_t addr;
	struct netconn *xNetConn = NULL;


	//get IP
	err = netconn_gethostbyname(channel_getStreamHost(streamID), &remote_ip);
  	if (err != ERR_OK) {
		printf("Connect - ERR getIP: %d  %s\n", err, channel_getStreamHost(streamID));
		netconn_delete ( xNetConn );
		return NULL;
	}


	//create connection
	xNetConn = netconn_new ( NETCONN_TCP ); 
	if ( xNetConn == NULL ) { 
 		printf("Connect - ERR new con\n");
		netconn_delete ( xNetConn );
		return NULL;
	 	
	}


	//connect to server
	err = netconn_connect (xNetConn, &remote_ip, (u16_t)channel_getStreamPort(streamID) ); 
	if ( err != ERR_OK)
	{
		printf("Connect - ERR connect, ret=%d\n", ret);
		netconn_close(xNetConn); 
		netconn_delete ( xNetConn );
		return NULL;

	}


	//write request
	err = netconn_write(xNetConn, "GET ", 4, NETCONN_NOCOPY);
	if ( err != ERR_OK)
	{
		printf("Connect - ERR write, ret=%d\n", ret);
		netconn_close(xNetConn); 
		netconn_delete (xNetConn);
		return NULL;

	}
	netconn_write(xNetConn, channel_getStreamPath(streamID), strlen(channel_getStreamPath(streamID)), NETCONN_NOCOPY);
	netconn_write(xNetConn, " HTTP/1.0\r\nHost: ", 17, NETCONN_NOCOPY);
	netconn_write(xNetConn, channel_getStreamHost(streamID), strlen(channel_getStreamHost(streamID)), NETCONN_NOCOPY);
	netconn_write(xNetConn, "\r\n\r\n", 4, NETCONN_NOCOPY);

	//Receive response
	struct netbuf *nbuf;
	err = netconn_recv (xNetConn, &nbuf);
	if(err != ERR_OK){
		netbuf_delete(nbuf);
		printf("Connect - Err reading\n");
		netconn_close(xNetConn); 
		netconn_delete (xNetConn);
		return NULL;
	}
	n = (int)netbuf_len(nbuf);
	if (n<=0){
		netbuf_delete(nbuf);
		printf("Connect - Err reading\n");
		netconn_close(xNetConn); 
		netconn_delete (xNetConn);
		return NULL;
	}
	char *wbuf;
	netbuf_data(nbuf, (void**)&wbuf, (u16_t*)&n);	
	netbuf_delete(nbuf);


	printf("Connect - %s%s\n", channel_getStreamHost(streamID), channel_getStreamPath(streamID));

	return xNetConn;

}




//Reader task. This will try to read data from a TCP socket into the SPI fifo buffer.
void ICACHE_FLASH_ATTR tskreader(void *pvParameters) {

	int madRunning=0;
	int n, l, inBuf;
	struct netconn * fd;
	int t = 0;
	int ret;

	
	while(1) { 

		//Read selected channel and start connection
		channel_read();
		printf("reader - Start stream %d\n", channel_getStreamCounter());	
		fd=openConn_netconn(channel_getStreamCounter()); 
		channel_setCurrentStream(channel_getStreamCounter());

		if(fd == NULL){	//Reconnect, if connection failed...
			vTaskDelay(1000/portTICK_RATE_MS);
			continue;
		}

		printf("reader - Start Reading\n");
		t = 0;
		do{
			

			//Read from Netconn
			struct netbuf *nbuf;
			err_t err;
			err = netconn_recv (fd, &nbuf);
			if(err != ERR_OK){
				netbuf_delete(nbuf);
				printf("Err reading\n");
				break;
			}
			n = (int)netbuf_len(nbuf);

			//if there are data available, copy them to SPI-buff
			if (n>0){
				
				do {	
					char *wbuf;			
					netbuf_data(nbuf, (void**)&wbuf, (u16_t*)&n);	
					spiRamFifoWrite(wbuf, n);
				} while(netbuf_next(nbuf) >= 0);
			}
			netbuf_delete(nbuf);


			//Running MAD-Task, if not running
			if ((!madRunning) && (spiRamFifoFree()<spiRamFifoLen()/2)) {
				//Buffer is filled. Start up the MAD task. 
				//Yes, the 2100 bytes of stack is a fairly large amount but MAD seems to need it.
				if (xTaskCreate(tskmad, "tskmad", 2100, NULL, PRIO_MAD, NULL)!=pdPASS) 
					printf("play - ERR create MADTask\n");
				madRunning=1;
			}
			

			//Read channel-count every 7 read-steps to avoid laggy sound
			t+=1;
			if(t >= 20){
				t = 0;
				printf("play - Buff: %d\n", spiRamFifoFill());

				//change channel
				channel_read();
				if(channel_getStreamCounter() != channel_getCurrentStream()){
					printf("play - Change to %d\n", channel_getStreamCounter());					
					
					//If channel is greater than maximum available channel, reboot for Server-Mode (config-mode)					
					if(channel_getStreamCounter() >= channel_getStreamCount())
						system_restart();		
					break;
				}
			}
			
		//Stop, if no more data available for reading...			
 		} while (n>0);
		

		//Close connection
		netconn_close(fd); 
		netconn_delete(fd);

	}

	vTaskDelete(NULL);
}



//Simple task to connect to an access point, initialize i2s and fire up the reader task.
void ICACHE_FLASH_ATTR tskconnect(void *pvParameters) {
	
	//Wait a few secs for the stack to settle down
	//vTaskDelay(3000/portTICK_RATE_MS);
	

	//Init Channels
	if(flash_readConfig() != 0){

		//if reading of flash failed (due to wrong header or other reason), set up default values
		printf("init - Faild to read Flash\n");
		printf("init - Write WLAN-Data\n");

		//Set default Wifi_Datas	
		wifi_setSSID(DEFAULT_SSID, strlen(DEFAULT_SSID));
		wifi_setPass(DEFAULT_PASS, strlen(DEFAULT_PASS));

		printf("init - Write Channel-Data\n");

		
		channel_setStreamCount(1);
		channel_setStreamHost(0, DEFAULT_CHANNEL_HOST, strlen(DEFAULT_CHANNEL_HOST));
		channel_setStreamPath(0, DEFAULT_CHANNEL_PATH, strlen(DEFAULT_CHANNEL_PATH));
		channel_setStreamPort(0, DEFAULT_CHANNEL_PORT);

	}

	//Read channel-setting
	channel_read();

	//If channel is greater than maximum available channel, start SERVER-MODE (config-mode)
	if(channel_getStreamCounter() >= channel_getStreamCount()){

		printf("START SERVER MODE\n");
		channel_setCurrentStream(channel_getStreamCounter());

		//Go to AP-mode
		wifi_station_disconnect();
		if (wifi_get_opmode() != SOFTAP_MODE) { 
			wifi_set_opmode(SOFTAP_MODE);
		}		

		//Set AP-Settings
		struct softap_config *config=malloc(sizeof(struct softap_config));
		memset(config, 0x00, sizeof(struct softap_config));
		sprintf(config->ssid, "myRadio");
		config->authmode = AUTH_OPEN;
		config->max_connection=2;
		wifi_softap_set_config(config);
		free(config);
		

		//Start Server-Task
		if (xTaskCreate(tskserver, "tskserver", 2100, NULL, PRIO_READER, NULL)!=pdPASS) printf("ERR create serverTask!\n");

	}
	else{
	
		printf("START CLIENT MODE\n");
		channel_setCurrentStream(channel_getStreamCounter());

		//Go to station mode
		wifi_station_disconnect();
		if (wifi_get_opmode() != STATION_MODE) { 
			wifi_set_opmode(STATION_MODE);
		}

		//Connect to the defined access point.
		struct station_config *config=malloc(sizeof(struct station_config));
		memset(config, 0x00, sizeof(struct station_config));
		sprintf(config->ssid, (const char*)wifi_getSSID());
		sprintf(config->password, (const char*)wifi_getPass());
		printf("WIFI SSID %s set\n", config->ssid);
		printf("WIFI Pass %s set\n", config->password);
		wifi_station_set_config(config);
		wifi_station_connect();
		free(config);

		//wait until IP is valid
		while(wifi_station_get_connect_status() != STATION_GOT_IP){
			vTaskDelay(500/portTICK_RATE_MS);	
		}

		//Fire up the reader task. The reader task will fire up the MP3 decoder as soon
		//as it has read enough MP3 data.
		if (xTaskCreate(tskreader, "tskreader", 200, NULL, PRIO_READER, NULL)!=pdPASS) printf("ERR create readerTask!\n");
		//We're done. Delete this task.
	}

	vTaskDelete(NULL);
}


//We need this to tell the OS we're running at a higher clock frequency.
extern void os_update_cpu_frequency(int mhz);

void ICACHE_FLASH_ATTR user_init(void) {
	//Tell hardware to run at 160MHz instead of 80MHz
	//This actually is not needed in normal situations... the hardware is quick enough to do
	//MP3 decoding at 80MHz. It, however, seems to help with receiving data over long and/or unstable
	//links, so you may want to turn it on. Also, the delta-sigma code seems to need a bit more speed
	//than the other solutions to keep up with the output samples, so it's also enabled there.

//Always enable full speed...
//#if defined(DELTA_SIGMA_HACK)
	SET_PERI_REG_MASK(0x3ff00014, BIT(0));
	os_update_cpu_frequency(160);
//#endif
	
	//Set the UART to 115200 baud
	UART_SetBaudrate(0, 115200);

	//Initialize the SPI RAM chip communications and see if it actually retains some bytes. If it
	//doesn't, warn user.
	if (!spiRamFifoInit()) {
		printf("\n\nSPI chip fail\n");
		while(1);
	}


	//Start conneciton -task
	printf("\n\nHW init. Wait for WIFI\n");
	xTaskCreate(tskconnect, "tskconnect", 500, NULL, 3, NULL);

}

