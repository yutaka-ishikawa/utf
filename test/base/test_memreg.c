#include <string.h>
#include <utf.h>
#include <utf_timer.h>
#include <utf_debug.h>
#include <utf.h>
#include "testlib.h"
#include <utofu.h>
#include <utf_conf.h>
#include <utf_tofu.h>

extern struct utf_info utf_info;
extern utofu_stadd_t utf_mem_reg(utofu_vcq_hdl_t vcqh, void *buf, size_t size);
extern void *utf_malloc(size_t sz);
extern void utf_free(void *ptr);

#define MAKE_KEY(stadd, virt)	(((stadd) & 0xfffffff000000000LL) | ((uint64_t)(virt)&0x0000000fffffffffLL))
#define CALC_STADD(key, virt)	(((key) & 0xfffffff000000000LL) | (((((uint64_t)(virt)&0x0000000fffffffffLL)) -  ((key)&0x0000000fffffffffLL)) + ((key)&0xffffLL)))
#define EXTRCT_STADD(key) ((uint64_t)(key)&0xfffffff00000ffffLL)

/* bss */
char	bssbuf[1024];
char	bssbuf2[1024*1024*4];
char	bss1g[1024*1024*512];
char	bss2buf[1024];
char	bss2g[1024*1024*512];

/* data */
uint64_t	databuf = 0;
char	data1g[1024*1024*1024] = {0};
uint64_t	data2buf = 0;
char	data2g[1024*1024*1024];

int
main(int argc, char **argv)
{
    char	stackbuf[1024];
    utofu_stadd_t	staddr[128], key;
    void	*virt[128];
    void	*mp0, *maddr[128];
    int	i, off;

    test_init(argc, argv);
    memset(maddr, 0, sizeof(maddr));

    off = 0;
    maddr[0] = utf_malloc(1024*sizeof(int));
    virt[off] = maddr[0];
    staddr[off] = utf_mem_reg(utf_info.vcqh, maddr[0], 1024*sizeof(int));
    printf("malloc 4K = %p stadd = 0x%lx\n", maddr[0], staddr[off]);
    utf_free(maddr[0]);

    off++;
    virt[off] = stackbuf;
    staddr[off] = utf_mem_reg(utf_info.vcqh, stackbuf, 1024);
    printf("stack buf = %p stadd = 0x%lx\n", stackbuf, staddr[off]);

    off++;
    virt[off] = bssbuf;
    staddr[off] = utf_mem_reg(utf_info.vcqh, bssbuf, 1024);
    printf("bssbuf = %p stadd = 0x%lx\n", bssbuf, staddr[off]);

    off++;
    virt[off] = bssbuf;
    staddr[off] = utf_mem_reg(utf_info.vcqh, bssbuf2, 1024*1024*4);
    printf("bssbuf2 = %p stadd = 0x%lx\n", bssbuf2, staddr[off]);

    off++;
    virt[off] = bss1g;
    staddr[off] = utf_mem_reg(utf_info.vcqh, bss1g, 1024*1024*1024);
    printf("bss1g = %p stadd = 0x%lx\n", bss1g, staddr[off]);
    off++;
    virt[off] = bss2buf;
    staddr[off] = utf_mem_reg(utf_info.vcqh, bss2buf, 1024);
    printf("bss2buf = %p stadd = 0x%lx\n", bss2buf, staddr[off]);

    for (i = 0; i < 10; i++) {
	off++;
	virt[off] = mp0 = (char*)bss2g + 32*1024*i;
	staddr[off] = utf_mem_reg(utf_info.vcqh, mp0, 64*1024);
	printf("bss2g + 0x%x = %p stadd = 0x%lx\n", 64*1024*i, mp0, staddr[off]);
    }
    off++;
    virt[off] = &databuf;
    staddr[off] = utf_mem_reg(utf_info.vcqh, &databuf, 8);
    printf("databuf = %p stadd = 0x%lx\n", &databuf, staddr[off]);
    off++;
    virt[off] = data1g;
    staddr[off] = utf_mem_reg(utf_info.vcqh, data1g, 1024*1024*1024);
    printf("data1g = %p stadd = 0x%lx\n", data1g, staddr[off]);
    off++;
    virt[off] = &data2buf;
    staddr[off] = utf_mem_reg(utf_info.vcqh, &data2buf, 8);
    printf("data2buf = %p stadd = 0x%lx\n", &data2buf, staddr[off]);
    off++;
    virt[off] = data2g;
    staddr[off] = utf_mem_reg(utf_info.vcqh, data2g, 1024*1024*1024);
    printf("data2g = %p stadd = 0x%lx\n", data2g, staddr[off]);

    posix_memalign((void*) &mp0, 256, 1024);
    off++;
    virt[off] = mp0;
    staddr[off] = utf_mem_reg(utf_info.vcqh, mp0, 1024);
    printf("posix_memalign = %p stadd = 0x%lx\n", mp0, staddr[off]);

    maddr[0] = utf_malloc(1024);
    off++;
    virt[off] = maddr[0];
    staddr[off] = utf_mem_reg(utf_info.vcqh, maddr[0], 1024);
    printf("malloc 1K = %p stadd = 0x%lx\n", maddr[0], staddr[off]);

    for (i = 1; i < 28; i++) {
	maddr[i] = utf_malloc(1024*1024*1024);
	if (maddr[i] == NULL) break;
	off++;
	virt[off] = maddr[i];
	staddr[off] = utf_mem_reg(utf_info.vcqh, maddr[i], 1024*1024*512);
	printf("[%d GB] malloc 1G = %p stadd = 0x%lx\n", i, maddr[i], staddr[off]);
    }

    /**/
    for (i = 0; i <= off; i++) {
	key = MAKE_KEY(staddr[i], virt[i]);
	printf("virt=0x%lx, key=0x%lx, stadd(0)=0x%lx, "
	       "stadd(0x1000)=0x%lx, stadd(0x11000)=0x%lx, key->stadd=0x%lx\n",
	       virt[i], key, CALC_STADD(key, virt[i]),
	       CALC_STADD(key, virt[i] + 0x1000),
	       CALC_STADD(key, virt[i] + 0x11000),
	       EXTRCT_STADD(key));
    }

    for (i = 0; i < 28; i++) {
	if (maddr[i] != NULL) utf_free(maddr[i]);
    }
    return 0;
}
