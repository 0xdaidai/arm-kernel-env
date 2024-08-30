#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#define CMD_SMC 0x13370001
#define CMD_AAR 0x13370002
#define CMD_AAW 0x13370003
#define DRIVER_PATH "/dev/etx_device"

typedef size_t UINT64 ;
typedef unsigned int UINT32;
typedef unsigned char UINT8;
typedef void *PVOID;
typedef int BOOL;
#define PAGE_SIZE 0x1000

typedef struct {
    unsigned long long int smc_fid;
    unsigned long long int x1;
    unsigned long long int x2;
    unsigned long long int x3;
    unsigned long long int x4;
} SmcArgs;

typedef struct {
    size_t addr;  // 用于存储物理地址
    size_t size;       // 用于存储读取的大小
    size_t recv_addr;
}AAR_arg;

typedef struct {
    size_t addr;  // 用于存储物理地址
    size_t size;       // 用于存储读取的大小
    size_t val;
}AAW_arg;

int devfd;
//print_memory(cur_address, (char *)physmem_addr, auxSize);

void print_memory(UINT64 address, char *buffer, UINT32 size)
{
    UINT32 i, j;
    unsigned char *ptr = (UINT32 *)buffer;
    for (i = 0; i < size / 4; i++)
    {
        if (i % 4 == 0)
        {
            printf("\n%p: ", (void *)(address + i * 4));
        }
        for(int j = 0;j < 4;j++)
            printf("%2x ", ptr[i+j]);
    }
    printf("\n");
}

void read_physical_memory(UINT64 address, UINT32 size, PVOID buffer, BOOL bPrint)
{
    if (size > 0)
    {

        int i = 0;
        UINT32 auxSize = size;
        UINT64 cur_address;

        while (auxSize > PAGE_SIZE)
        {
            cur_address = address + (i * PAGE_SIZE);

            void *physmem_addr = mmap(NULL, PAGE_SIZE, PROT_READ, MAP_SHARED, devfd, cur_address);

            if (physmem_addr != MAP_FAILED)
            {
                printf("-> %08x bytes read from physical memory %p\n", PAGE_SIZE, cur_address);
                if (buffer)
                {
                    memcpy((char *)buffer + (i * PAGE_SIZE), physmem_addr, PAGE_SIZE);
                }
                if (bPrint)
                {
                    print_memory(cur_address, (char *)physmem_addr, PAGE_SIZE);
                }
                munmap(physmem_addr, PAGE_SIZE);
            }

            auxSize = auxSize - PAGE_SIZE;
            i++;
        }

        cur_address = address + (i * PAGE_SIZE);

        void *physmem_addr = mmap(NULL, PAGE_SIZE, PROT_READ, MAP_SHARED, devfd, cur_address);

        if (physmem_addr != MAP_FAILED)
        {
            printf("-> %08x bytes read from physical memory %p\n", PAGE_SIZE, cur_address);
            if (buffer)
            {
                memcpy((char *)buffer + (i * PAGE_SIZE), physmem_addr, auxSize);
            }

            if (bPrint)
            {
                print_memory(cur_address, (char *)physmem_addr, auxSize);
            }

            munmap(physmem_addr, PAGE_SIZE);
        }
    }
}

int main()
{
    devfd = open(DRIVER_PATH, 2);
    printf("devfd: %d\n", devfd);
    int status = 0;
    AAW_arg aaw_arg;
    memset(&aaw_arg, 0, sizeof(AAW_arg));

    aaw_arg.addr = 0x42000000;
    aaw_arg.size = 16;
    aaw_arg.val = 0xdeadbeef;

    status = ioctl(devfd, CMD_AAR, &aaw_arg);

    return 0;
}