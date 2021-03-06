#include <stdio.h>
#include <stdlib.h>

#include <stdint.h>

#include <iostream>
#include <fstream>
#include <vector>

/********************  SETTINGS  ********************/
// SDIO Settings
#define BLOCK_SIZE              512   /* SD card block size in Bytes (512 for a normal SD card) */

// Logger settings
#define RX_BUFFER_NUM_BLOCKS    20 /* 20 blocks * 512 = 10 KB RAM required per buffer*/
/**************   END OF SETTINGS   *****************/

#define BUFF_SIZE  (BLOCK_SIZE*RX_BUFFER_NUM_BLOCKS)

void read_disk_raw(char* volume_name, unsigned long disk_size);

inline void wordswap(int32_t *i)
{
	uint8_t *p = (uint8_t*) i;
	uint8_t tmp = p[0];
	p[0] = p[1];
	p[1] = tmp;
	tmp = p[2];
	p[2] = p[3];
	p[3] = tmp;
}

int main(int argc, char** argv)
{
    printf("PARSE DISK:\n----------\n");
	printf("highspeedloggerbinaryparser <DRIVE>     : eg. '/dev/sdb' or '\\\\.\\G:'  \n");
	printf("highspeedloggerbinaryparser <DUMPFILE>  : eg. foo.diskdump \n");

	if (argc >= 2)
	{

		if (argc == 2)
		{
			read_disk_raw(argv[1], 128UL*1024UL*1024UL);
			exit(0);
		}
	}
	else
	{

#ifdef WIN32
		read_disk_raw("\\\\.\\G:", 128UL*1024UL*1024UL);
	    // read_disk_raw("d:\\cessnaLogFull.dd", 128UL*1024UL*1024UL);
#else
		read_disk_raw("/dev/sdb", 128UL*1024UL*1024UL);
#endif

		exit(0);
	}

	exit(-1);
}


#if 0

#define WIN32_LEAN_AND_MEAN
#define BUFFER_SIZE 32

#include <windows.h>

void read_disk_raw(void)
{
	HANDLE hFile; 
	DWORD dwBytesRead = 0;
	char ReadBuffer[BUFFER_SIZE] = {0};

	hFile = CreateFile("\\\\.\\G:",GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);

	if (hFile == INVALID_HANDLE_VALUE) 
	{ 
		printf("Could not open file (error %d)\n", GetLastError());
		return; 
	}

	// Read one character less than the buffer size to save room for
	// the terminating NULL character.

	if( FALSE == ReadFile(hFile, ReadBuffer, BUFFER_SIZE-2, &dwBytesRead, NULL) )
	{
		printf("Could not read from file (error %d)\n", GetLastError());
		CloseHandle(hFile);
		return;
	}

	if (dwBytesRead > 0)
	{
		ReadBuffer[dwBytesRead+1]='\0'; // NULL character

		printf(TEXT("Text read from %s (%d bytes): \n"), dwBytesRead);
		printf("%s\n", ReadBuffer);
	}
	else
	{
		printf(TEXT("No data read from file %s\n"));
	}

	CloseHandle(hFile);

}

#endif



// Read from sector //
void read_disk_raw(char* volume_name, unsigned long disk_size)
{
    FILE *volume;
    int k = 0;
    char buf[BUFF_SIZE] = {0};
  
    volume = fopen(volume_name, "r+b");
    if(!volume)
    {
        printf("Cant open Drive '%s'\n", volume_name);
        return;
    }
    setbuf(volume, NULL);       // Disable buffering

	// fseek(volume, 0, SEEK_END);
	// long size = ftell(volume);
 
	int log = -1;
	int cnt = 0;

	FILE* of = 0;

	printf("\nSearching for logfiles in '%s': \n...\r",volume_name);

	unsigned long addr = 0;
    // read what is in sector and put in buf //
	while (addr < disk_size)
	{
		if(fseek(volume, addr, SEEK_SET) != 0)
		{
			printf("Cant move to sector\n");
			return;
		}
 
		size_t r = fread(buf, 1, BUFF_SIZE, volume);

		if (  ((unsigned char)buf[0] == 0x1a) && ((unsigned char)buf[1] == 0x1b) && ((unsigned char)buf[2] == 0x1c))
        {
            if ((unsigned char)buf[3] == 0xaa)
            {
				int tt = 0;
				int nr = 0;
				tt = (int) ((uint8_t)buf[6]);
				nr = (int)  ((uint8_t) buf[5] ) + ((uint16_t) buf[4] )*256;
				char filename[32];
				char logtype[16];
				log++;
				switch(tt)
				{
				case 0:
					sprintf(logtype, "SPI1");
					break;
				case 1:
					sprintf(logtype, "SPI2");
					break;
				case 2:
					sprintf(logtype, "UART2");
					break;
				case 3:
					sprintf(logtype, "UART3");
					break;
				default:
					sprintf(logtype, "UNKNOWN");
					break;
				}
				sprintf(filename, "sd_hs_log_%05d_%s.bin", log, logtype);
				if (of > 0)
				{
					fclose(of);
					of = 0;
				}
				of = fopen(filename, "w+b");
				fwrite(buf,1,BUFF_SIZE,of);
				if (log > 0)
					printf("%d x 10k\nLog %d [#%d]: type [%x=%s] addr: <%ld> ",cnt, log, nr, tt, logtype, addr/1024);
				else
	                printf("Log %d [#%d]: type [%x=%s] addr: <%ld> ",log, nr, tt, logtype, addr/1024);
				cnt = 0;
            }
            else if ((unsigned char)buf[3] == 0xbb)
            {
				cnt++;
				if (of > 0)
					fwrite(buf,1,BUFF_SIZE,of);

            }
            else
            {
				if (of > 0)
				{
					fclose(of);
					of = 0;
				}
                printf("? (%x)",(unsigned char)buf[3]);
            }
        }
        else
        {
			if (of > 0)
			{
				fclose(of);
				of = 0;
			}
			if (cnt > 0)
			{
				printf("%d x 10k\nEnd <%ld>\n",cnt, addr/1024);
				cnt = 0;
			}
    		//printf(".");
        }

		int f = feof(volume);
		int e = ferror(volume);

		if (f!=0)
		{
			printf("%d x 10k\nEof <%ld>\n",cnt, addr/1024);
			cnt = 0;
			break;
		}

		addr += BUFF_SIZE;
		
		//printf("\n %ld: %d %d %d ",addr, f, e, r);
	}
 
    // Print out what wiat in sector //
    // for(k=0;k<BUFFER_SIZE;k++)
    //    printf("%02x ", (unsigned char) buf[k]);
 
    fclose(volume);
 
    return;
}

