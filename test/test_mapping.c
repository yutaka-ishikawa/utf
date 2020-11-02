#include <string.h>
#include <utf.h>
#include "testlib.h"

int
main(int argc, char **argv)
{
    test_init(argc, argv);

    if (myrank == 0) {
	utf_vname_show(stdout);
    }
    utf_fence();
    if (myrank == 0) {
	printf("Success\n");
    }
    return 0;
}
