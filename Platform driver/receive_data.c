#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>



#define MEM_BASE1 0x20000000
#define REG_BASE1  0x20001000

#define MEM_BASE2 0x20010000
#define REG_BASE2 0x20011000

#define MEM_BASE3  0x20002000
#define REG_BASE3  0x20003000

#define MEM_BASE4 0x20012000
#define REG_BASE4 0x20013000

#define MEM_SIZE	0x1000
#define REG_SIZE	8

#define PLAT_IO_FLAG_REG		(0) /*Offset of flag register*/
#define PLAT_IO_SIZE_REG		(4) /*Offset of flag register*/
#define PLAT_IO_DATA_READY	(1) /*IO data ready flag */

#define MAX_DEVICES	2

extern int errno;

struct my_device {
	uint32_t memrx_base;
	uint32_t mem_size;
	uint32_t regrx_base;
	uint32_t reg_size;
	uint32_t memtx_base;
	uint32_t regtx_base;		
};

static struct my_device my_devices[MAX_DEVICES] = {{
	.memrx_base = MEM_BASE1,
	.mem_size = MEM_SIZE,
	.regrx_base = REG_BASE1,
	.reg_size = REG_SIZE,
	.memtx_base = MEM_BASE3,
	.regtx_base = REG_BASE3,
	},
	{
	.memrx_base = MEM_BASE2,
	.mem_size = MEM_SIZE,
	.regrx_base = REG_BASE2,
	.reg_size = REG_SIZE,
	.memtx_base = MEM_BASE4,
	.regtx_base = REG_BASE4,
	},
};
int usage(char **argv)
{
	printf("Program sends file to the specific device\n");
	printf("Usage: %s <device>\n", argv[0]);
	return -1;
}

int main(int argc, char **argv)
{
	volatile unsigned int *regrx_addr = NULL, *regtx_addr = NULL, *countrx_addr, *flagrx_addr, *counttx_addr, *flagtx_addr;
	volatile unsigned char *memrx_addr = NULL, *memtx_addr = NULL;
	unsigned int i, device, ret, len, count;
	struct stat st;
	uint8_t *buf;
	FILE *f;

	if (argc != 2) {
		return usage(argv);
	}

	device = atoi(argv[1]);
	if (device >= MAX_DEVICES)
		return usage(argv);



	int fd = open("/dev/mem",O_RDWR|O_SYNC);
	if(fd < 0)
	{
		printf("Can't open /dev/mem\n");
		return -1;
	}

	memrx_addr = (unsigned char *) mmap(0, my_devices[device].mem_size,
				PROT_WRITE, MAP_SHARED, fd, my_devices[device].memrx_base);
	if (memrx_addr == NULL) {
		printf("Can't mmap\n");
		return -1;
	}

	memtx_addr = (unsigned char *) mmap(0, my_devices[device].mem_size,
		PROT_WRITE, MAP_SHARED, fd, my_devices[device].memtx_base);
	if (memtx_addr == NULL) {
	printf("Can't mmap\n");
	return -1;
	}

	regrx_addr = (unsigned int *) mmap(0, my_devices[device].reg_size,
			PROT_WRITE | PROT_READ, MAP_SHARED, fd, my_devices[device].regrx_base);

	regtx_addr = (unsigned int *) mmap(0, my_devices[device].reg_size,
			PROT_WRITE | PROT_READ, MAP_SHARED, fd, my_devices[device].regtx_base);

	flagtx_addr = regtx_addr;
    counttx_addr = regtx_addr;
    counttx_addr++;

	if (*flagtx_addr) {
		*flagtx_addr = 0;
		printf("Size = %d\n", *counttx_addr);
		for (i=0; i < *counttx_addr; i++) {
            printf("%c", *memtx_addr++);
    }
	}
	else
		printf("No data available");
		
	printf("\n");		

	return 0;
}
