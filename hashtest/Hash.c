#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/mman.h>
#include<fcntl.h>

static inline int rol32(int word, unsigned int shift)
{
	return (word << shift) | (word >> (32 - shift));
}

/* An arbitrary initial parameter */
#define JHASH_INITVAL		0xdeadbeef
/* __jhash_mix -- mix 3 32-bit values reversibly. */
#define __jhash_mix(a, b, c)			\
{						\
	a -= c;  a ^= rol32(c, 4);  c += b;	\
	b -= a;  b ^= rol32(a, 6);  a += c;	\
	c -= b;  c ^= rol32(b, 8);  b += a;	\
	a -= c;  a ^= rol32(c, 16); c += b;	\
	b -= a;  b ^= rol32(a, 19); a += c;	\
	c -= b;  c ^= rol32(b, 4);  b += a;	\
}
/* __jhash_final - final mixing of 3 32-bit values (a,b,c) into c */
#define __jhash_final(a, b, c)			\
{						\
	c ^= b; c -= rol32(b, 14);		\
	a ^= c; a -= rol32(c, 11);		\
	b ^= a; b -= rol32(a, 25);		\
	c ^= b; c -= rol32(b, 16);		\
	a ^= c; a -= rol32(c, 4);		\
	b ^= a; b -= rol32(a, 14);		\
	c ^= b; c -= rol32(b, 24);		\
}

unsigned int jhash32(const int *k, int length, int initval){
    unsigned int a, b, c;

	/* Set up the internal state */
	a = b = c = JHASH_INITVAL + (length<<2) + initval;

	/* Handle most of the key */
	while (length > 3) {
		a += k[0];
		b += k[1];
		c += k[2];
		__jhash_mix(a, b, c);
		length -= 3;
		k += 3;
	}

	/* Handle the last 3 u32's: all the case statements fall through */
	switch (length) {
	case 3: c += k[2];
	case 2: b += k[1];
	case 1: a += k[0];
		__jhash_final(a, b, c);
	case 0:	/* Nothing left to add */
		break;
	}

	return c;
}

int main(){
    /*如果文件不算大，就通过mmap方式映射到内存，在做hash*/
   /* char filepath[100];
    printf("please input file path:\n");
    scanf("%s",filepath);*/
    char *filepath="./LinuxShell.pdf";
    printf("filepath:%s\n",filepath);
    FILE *filep;
    int size=0;
    void *addr;
    /*get file size*/
    filep=fopen(filepath,"r");
    if(filep==NULL){
        printf("error!\n");
        goto error;
    }
    fseek(filep,0,SEEK_END);
    size=ftell(filep);
    fclose(filep);
    /*mmap file*/
    int fd;
    fd=open(filepath,O_RDWR,0);
    addr=mmap(NULL,size,PROT_READ|PROT_WRITE ,MAP_SHARED,fd,0);
    printf("addr:%p\n",addr);
    /*hash  page*/
    int k;
    unsigned int a;
    void *p=addr;
    for(k=0;k<(size/4096+1);k++){
        a=jhash32(addr,1024,17);
	printf("%d hash value:%08x\n",k,a);
	addr+=4096;
    }
    munmap(p,size);
    close(fd);
    printf("size of file:%d kb\n",size/1024);

error:
    return 0;

    return 1;
}
