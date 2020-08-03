/* fccpx -Nclang -ffj-fjprof -o test_profile test_profile.c */
#include "fj_tool/fapp.h"

int	ar[1024][1024];

int
main()
{
    int i, j;

    fapp_start("section-1", 1, 0);
    for (i = 0; i < 1024; i++) {
	for (j = 0; j < 1024; j++) {
	    ar[i][j] = i;
	}
    }
    fapp_stop("section-1", 1, 0);
    fapp_start("section-2", 1, 0);
    for (i = 0; i < 1024; i++) {
	for (j = 0; j < 1024; j++) {
	    ar[i][j] = i;
	}
    }
    fapp_stop("section-2", 1, 0);
    return 0;
}
