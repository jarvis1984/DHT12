#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>
 
#define BUFFER_LENGTH 8		///< The number of datas from dht12 are 5, size 8 is big enough
static char receive[BUFFER_LENGTH];
 
int main()
{
	int ret = 0, fd = 0;

	fd = open("/dev/dht12_i2c", O_RDWR);
	if (fd < 0){
		perror("Failed to open the device...");
		return errno;
	}

	ret = read(fd, receive, BUFFER_LENGTH);
	if (ret < 0){
		perror("Failed to read the message from the device.");
		close(fd);
		return errno;
	}

	int i = 0;
	for (i = 0; i < ret; ++i)
		printf("received[%d] : %d\n", i, receive[i]);
	
	close(fd);
	return 0;
}