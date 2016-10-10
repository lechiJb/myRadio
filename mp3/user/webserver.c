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



//Define homepage and HTTP-response
#define HTTP_OK "HTTP/1.1 200 OK\r\n"
#define HTTP_SERVER "Server: myRadio (esp RTOS)\r\n"
#define HTTP_CONTENT_TYPE "Content-Type: text/html\r\n"
#define HTTP_CONNECTION "Connection: close\r\n"
#define HTTP_FINISHED "\r\n\r\n"

#define HTML_HEADER 		"<body><h1>myRadio - Settings</h1><hr>"
#define HTML_HEAD_WIFI		"<hr><h2>Wifi</h2>"
#define HTML_WIFI_SSID		"<form>Wifi SSID:<input type=\"text\" name=\"ssid\" value=\"%s\"><br><input type=\"submit\" value=\"OK\"></form>"
#define HTML_WIFI_PASS		"<form>Wifi password:<input type=\"text\" name=\"pass\" value=\"\"><br><input type=\"submit\" value=\"OK\"></form>"
#define HTML_HEAD_CHANNEL 	"<hr><h2>Channel</h2>"
#define HTML_CHANNELCOUNT	"<form>How many radio-channels do you like to use?<input type=\"text\" name=\"channel\" value=\"%d\"><br><input type=\"submit\" value=\"OK\"></form>"
#define HTML_CHANNELLINK	"<form>Link to channel %d:<input type=\"text\" name=\"link%d\" value=\"%s%s\"><input type=\"submit\" value=\"OK\"></form>"
#define HTML_CHANNELPORT	"<form>Port to channel %d:<input type=\"text\" name=\"port%d\" value=\"%d\"><input type=\"submit\" value=\"OK\"></form><br>"
#define HTML_WRITE_FLASH	"<hr><form><button type=\"submit\" name=\"flash\" value=\"1\">Write data to flash</button></form>"
#define HTML_END "</body></html>"

#define HTML_WRITE_OK		"<body><h1>myRadio - Response</h1><hr><hr><h2>Settings OK</h2><form><button type=\"submit\" name=\"main\" value=\"1\">Back to main-page</button></form></body></html>"	

#define HTML_WRITE_NOK		"<body><h1>myRadio - Response</h1><hr><hr><h2>Settings Not OK</h2><form><button type=\"submit\" name=\"main\" value=\"1\">Back to main-page</button></form></body></html>"


//Define Server-settings
#define SERVER_PORT 80
#define MAX_SERVER_BUFF 1024
#define MAX_CHAR_LEN 512


//Write HTTP-response to socket
int writeResponse(int sock){

	//Response:
	if(write(sock, HTTP_OK, strlen(HTTP_OK)) <0) return -1;
	if(write(sock, HTTP_SERVER, strlen(HTTP_SERVER)) <0) return -1;
	if(write(sock, HTTP_CONTENT_TYPE, strlen(HTTP_CONTENT_TYPE)) <0) return -1;
	if(write(sock, HTTP_CONNECTION, strlen(HTTP_CONNECTION)) <0) return -1;
	if(write(sock, HTTP_FINISHED, strlen(HTTP_FINISHED)) <0) return -1;

	return 0;

}


//Write HTML-OK-Page
int writeHTMLCodeOK(int sock){

	if(write(sock, HTML_WRITE_OK, strlen(HTML_WRITE_OK)) <0) return -1;
	
	return 0;
}

//Write HTML-NOK-Page
int writeHTMLCodeNOK(int sock){

	if(write(sock, HTML_WRITE_NOK, strlen(HTML_WRITE_NOK)) <0) return -1;
	
	return 0;
}


//Write HTML-Settings-Page
int writeHTMLCode(int sock){

	char buf[512];

	//header
	if(write(sock, HTML_HEADER, strlen(HTML_HEADER)) <0) return -1;
	if(write(sock, HTML_HEAD_WIFI, strlen(HTML_HEAD_WIFI)) <0) return -1;
	//SSID
	char* htmlWifiSSID = buf;
	if(strlen(HTML_WIFI_SSID) + strlen(wifi_getSSID()) > MAX_CHAR_LEN) return -1;
	sprintf(htmlWifiSSID, HTML_WIFI_SSID, wifi_getSSID());
	if(write(sock, htmlWifiSSID, strlen(htmlWifiSSID)) <0) return -1;
	//Pass
	if(write(sock, HTML_WIFI_PASS, strlen(HTML_WIFI_PASS)) <0) return -1;
	//Header
	if(write(sock, HTML_HEAD_CHANNEL, strlen(HTML_HEAD_CHANNEL)) <0) return -1;
	//Count
	char* htmlChannelCount = buf;
	if(strlen(HTML_CHANNELCOUNT) > MAX_CHAR_LEN) return -1;
	sprintf(htmlChannelCount, HTML_CHANNELCOUNT, channel_getStreamCount());
	if(write(sock, htmlChannelCount, strlen(htmlChannelCount)) <0) return -1;
	//Link
	int i;
	for(i=0; i< channel_getStreamCount(); i++){
		char* htmlChannelLink = buf;
		if(strlen(HTML_CHANNELLINK) + strlen(channel_getStreamHost(i)) + strlen(channel_getStreamPath(i)) > MAX_CHAR_LEN) return -1;
		sprintf(htmlChannelLink, HTML_CHANNELLINK, i+1, i+1, channel_getStreamHost(i), channel_getStreamPath(i));
		if(write(sock, htmlChannelLink, strlen(htmlChannelLink)) <0) return -1;

		sprintf(htmlChannelLink, HTML_CHANNELPORT, i+1, i+1, channel_getStreamPort(i));
		if(write(sock, htmlChannelLink, strlen(htmlChannelLink)) <0) return -1;
	}
	//Wirte flash
	if(write(sock, HTML_WRITE_FLASH, strlen(HTML_WRITE_FLASH)) <0) return -1;
	//Finish
	if(write(sock, HTML_END, strlen(HTML_END)) <0) return -1;
	if(write(sock, HTTP_FINISHED, strlen(HTTP_FINISHED)) <0) return -1;

	return 0;

}


//Parse Stream-Path out of Stream URL
void getStreamPathStart(char* value, int len, int* host_len, int* path_start, int* path_len){
	int i = 0;	
	for(i = 0; i < len; i++){
		if(	(value[i] == '%'  &&  (len-i) > 2  &&  value[i+1] == '2'  &&  value[i+2] == 'F') ||
			(value[i] == '%'  &&  (len-i) > 2  &&  value[i+1] == '5'  &&  value[i+2] == 'C')){
			if(*path_start == 0){			
				*host_len = i;
				*path_start = i+2;
				*path_len = len-(*path_start);
			}
			value[i]   = ' ';
			value[i+1] = ' ';			
			value[i+2] = '/';
			i+=2;
			return;
		}

		if(value[i] == '\\'  ||  value[i] == '/'){
			value[i] = '/';
			*host_len = i;
			*path_start = i;
			*path_len = len-(*path_start);
			return;
		}
			
	}
	
	*host_len = len;
	*path_start = 0;
	*path_len = 0;

	return;
}


//Parse Path out of html-request
int parseCommand(char*path, int path_len){

	int i;

	int i_cmd_start = 0;
	int i_cmd_end = 0;
	int i_value_start = 0;
	int i_value_end = path_len;	

	printf("webserver - Path len=%d:\n", path_len);

	//Search for Start of command and start of value ('=' separates command and value)
	//Start parsing at i=1, because of question-mark in url
	for(i=1; i < path_len; i++){
		printf("%c", path[i]);
		
		if(i == 1)
			i_cmd_start = 1;

		if(i_cmd_start == 1 && path[i] == '=')
			i_cmd_end = i;

	}
	i_value_start = i_cmd_end+1;

	printf("\nPathEnd\n");
	
	char* cmd = path+i_cmd_start;
	char* value = path + i_value_start;
	int cmd_len = i_cmd_end - i_cmd_start;
	int value_len = i_value_end - i_value_start;
	
	printf("webserver - cmd_len=%d, value_len=%d\n", cmd_len, value_len);
	printf("webserver - cmd_start=%d, cmd_end=%d, value_start=%d, value_end=%d\n", i_cmd_start, i_cmd_end, i_value_start, i_value_end);


	//Start executing commands
	int ret = 99;

	//Set StreamCount
	if(cmd_len >= 7 && cmd[0]=='c' && cmd[1]=='h' && cmd[2]=='a' && cmd[3]=='n' && cmd[4]=='n' && cmd[5]=='e' && cmd[6]=='l'){
	
		printf("webserver - Set StreamCount\n");

		if(value_len >= 1){
			//Get set channel count
			int count = ((int)value[0]) - 48;
			if(value_len > 1){
				int second = ((int)value[1]) - 48;
				count *= 10;
				count += second;
			}		

			ret = channel_setStreamCount(count);
		}

	}
	//Set Channel Link
	else if(cmd_len >=5 && cmd[0]=='l' && cmd[1]=='i' && cmd[2]=='n' && cmd[3]=='k'){

		//Get Channel-ID
		int channel = ((int)cmd[4]) - 48;
		if(cmd[5] != '='){
			int second = ((int)cmd[5]) - 48;
			channel *= 10;
			channel += second;
		}

		printf("webserver - Set Channel %d\n", channel);

		if(value_len >= 1){
			
			//Get Stream path and host
			int i_path_start = 0;
			int stream_path_len = 0;
			int host_len = 0;
			int i_host_start = 0;

			getStreamPathStart(value, value_len, &host_len, &i_path_start, &stream_path_len);
			
			ret = channel_setStreamPath(channel-1, value + i_path_start, stream_path_len);
			ret += channel_setStreamHost(channel-1, value + i_host_start, host_len);			

		}

	}
	//Set Channel Port
	else if(cmd_len >=5 && cmd[0]=='p' && cmd[1]=='o' && cmd[2]=='r' && cmd[3]=='t'){

		//Get Channel-ID
		int channel = ((int)cmd[4]) - 48;
		if(cmd[5] != '='){
			int second = ((int)cmd[5]) - 48;
			channel *= 10;
			channel += second;
		}
		
		printf("webserver - Set Port %d\n", channel);

		if(value_len >=1){
			//Get stream port
			i = 0;
			int port = 0;
			for(i=0; i< value_len; i++){
				port *= 10;
				port += ((int)value[i])-48;	
			}

			ret = channel_setStreamPort(channel-1, port);
		}


	}
	//Set SSID
	else if(cmd_len >=4 && cmd[0]=='s' && cmd[1]=='s' && cmd[2]=='i' && cmd[3]=='d'){		
		
		printf("webserver - Set SSID\n");

		wifi_setSSID(value, value_len);
		ret = 0;

	}
	//Set password
	else if(cmd_len >=4 && cmd[0]=='p' && cmd[1]=='a' && cmd[2]=='s' && cmd[3]=='s'){

		printf("webserver - Set Pass\n");

		wifi_setPass(value, value_len);
		ret = 0;

	}
	//Write data to flash
	else if(cmd_len >=5 && cmd[0]=='f' && cmd[1]=='l' && cmd[2]=='a' && cmd[3]=='s' && cmd[4]=='h'){
		
		printf("webserver - Write data to flash\n");

		ret = flash_writeConfig();

	}
	//Go back to main page
	else if(cmd_len >=4 && cmd[0]=='m' && cmd[1]=='a' && cmd[2]=='i' && cmd[3]=='n'){
		ret = 99;
	}
	//Invalid-command
	else{
		printf("webserver - Command invalid\n");
		ret = -1;
	}

	return ret;


}


//Start of config-server
void ICACHE_FLASH_ATTR tskserver(void *pvParameters) {

	char wbuf[MAX_SERVER_BUFF];
	int ret, err_return, len;

	//Create socket for server
	int socket_server=socket(PF_INET, SOCK_STREAM, 0);
	if (socket_server==-1) {
		printf("webserver - ERR create sock\n");
		vTaskDelete(NULL);;
	}

	//Setup socket-settings
	struct sockaddr_in addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(SERVER_PORT);
	struct sockaddr addr_client;
	socklen_t sock_len = sizeof(struct sockaddr);

	//Bind socket to port
	ret = bind(socket_server, (struct sockaddr *)(&addr), sizeof(struct sockaddr));
	if(ret != 0){
		getsockopt(socket_server, SOL_SOCKET, SO_ERROR, &err_return, (socklen_t *)&len);
		close(socket_server);
		printf("webserver - ERR bind err= %d\n", err_return);
		vTaskDelete(NULL);;
	}

	//Listen to port...
	ret = listen(socket_server, 5);
	if(ret != 0){
		getsockopt(socket_server, SOL_SOCKET, SO_ERROR, &err_return, (socklen_t *)&len);
		close(socket_server);
		printf("webserver - ERR listen err= %d\n", err_return);
		vTaskDelete(NULL);;
	}

	while(1) {

		printf("webserver - Wait for con\n");

		//accept connection
		int socket_client = accept(socket_server, (struct sockaddr *)(&addr_client), (socklen_t *)&sock_len);
		printf("webserver - Connection accepted\n");

		//receive data
		printf("webserver - Wait for data...\n");
		int rcv_len = recv(socket_client, wbuf, MAX_SERVER_BUFF, 0);
		
		if(rcv_len > 0){			
			//Check HTTP request for path
			int i;
			int i_start = 0;
			int i_end = 0;
			printf("webserver - Data len %d:\n", rcv_len);
			for(i=0; i < rcv_len; i++){

				if(wbuf[i] != '\r') printf("%c", wbuf[i]);	
			
				if(wbuf[i] == '/') {
					i_start = i+1;
				}				
				else if(wbuf[i] == ' '  &&  i_start != 0) {
					i_end = i;
					break;
				}	
		
			}
			printf("\nDataEnd\n");


			//get command
			int cmd_len = i_end - i_start;
			char* command = wbuf + i_start;
			
			//parse command
			int ret = parseCommand(command, cmd_len);

			//Send back response...
			if(writeResponse(socket_client) < 0){
				getsockopt(socket_client, SOL_SOCKET, SO_ERROR, &err_return, (socklen_t *)&len);
				close(socket_client);
				printf("webserver - ERR wirte response, err= %d\n", err_return);
				continue;
			}


			//Check, if main-page, NOK-page or OK-page is required
			if(ret == 99  ||  cmd_len == 0){
				//HTML-Homepage
				if(writeHTMLCode(socket_client) < 0){
					getsockopt(socket_client, SOL_SOCKET, SO_ERROR, &err_return, (socklen_t *)&len);
					close(socket_client);
					printf("webserver - ERR wirte response, err= %d\n", err_return);
					continue;
				}
			}
			else if(ret == 0){
				//HTML - OK
				if(writeHTMLCodeOK(socket_client) < 0){
					getsockopt(socket_client, SOL_SOCKET, SO_ERROR, &err_return, (socklen_t *)&len);
					close(socket_client);
					printf("webserver - ERR wirte response, err= %d\n", err_return);
					continue;
				}
			}
			else{
				//HTML - NOK
				if(writeHTMLCodeNOK(socket_client) < 0){
					getsockopt(socket_client, SOL_SOCKET, SO_ERROR, &err_return, (socklen_t *)&len);
					close(socket_client);
					printf("webserver - ERR wirte response, err= %d\n", err_return);
					continue;
				}
			}
			

		}

		//Restart on Channel-change...
		channel_read();
		if(channel_getStreamCounter() != channel_getCurrentStream()){
			printf("webserver - Change to %d\n", channel_getStreamCounter());					
			system_restart();		
		}
		
		closesocket(socket_client); 

	}
	
	closesocket(socket_server);
	vTaskDelete(NULL);

}
