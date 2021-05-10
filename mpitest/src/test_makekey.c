#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "makekey.h"

#define BSIZE	1024
char	buf[BSIZE];

int
main(int argc, char **argv)
{
    FILE	*fp;
    int success = 0, fail = 0;
    uint64_t stadd, virt, key, calc, ext_stadd, off = 0x2000;
    if (argc != 2) {
	fprintf(stderr, "%s: <file>\n", argv[0]);
	exit(-1);
    }
    if ((fp = fopen(argv[1], "r")) == NULL) {
	fprintf(stderr, "Cannot open file: %s\n", argv[1]);
	exit(-1);
    }
    while(fgets(buf, BSIZE, fp) != NULL) {
	if (buf[0] == '#') continue;
	sscanf(buf, "0x%llx,0x%llx\n", &virt, &stadd);
	key = make_key(stadd, (char*) virt);

	calc = calc_stadd(key, (char*)virt + off);
	ext_stadd = extrct_stadd(key);
	if (stadd == ext_stadd
	    && calc == (stadd + off)) {
	    success++;
	    printf("SUCCESS: virt = 0x%llx staddr = 0x%llx, key = 0x%llx, ext_stadd = 0x%llx, calc+%llx=%llx\n", virt, stadd, key, ext_stadd, off, calc);
	} else {
	    fail++;
	    printf("FAIL: virt = 0x%llx staddr = 0x%llx, key = 0x%llx, ext_stadd = 0x%llx, calc+%llx=%llx\n", virt, stadd, key, ext_stadd, off, calc);
	}
    }
    printf("#SUCCESS: %d\n", success);
    printf("#FAIL: %d\n", fail);
    return 0;
}
