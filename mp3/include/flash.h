#ifndef _FLASH_H
#define _FLASH_H


	//Write / Read-config
	int flash_writeConfig();
	int flash_readConfig();

	//Low-level write-functions
	int flash_erase_sector();

	int flash_write_header(int* flash_offset);
	int flash_write_int(int* ptr, int* flash_offset);
	int flash_write_string(char* ptr, int* flash_offset);

	int flash_read_header(int* flash_offset);
	int flash_read_int(int* ptr, int* flash_offset);
	int flash_read_string(char* ptr, int* flash_offset, int* len);


#endif

