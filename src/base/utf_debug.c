#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <limits.h>
#include "utf_conf.h"
#include "utf_debug.h"
#include "utf_tofu.h"
#include "utf_queue.h"
#include "utf_externs.h"
#include "utf_errmacros.h"
#include "utf_msgmacros.h"

static FILE	*logfp;
static char	logname[PATH_MAX];

void
utf_redirect()
{
    if (utf_info.myrank == -1) {
	fprintf(stderr, "Too early for calling %s\n", __func__);
	return;
    }
    if (stderr != logfp) {
        char    *cp1 = getenv("TOFULOG_DIR");
        char    *cp2 = getenv("PJM_JOBID");
        if (cp1) {
	    if (cp2) {
		sprintf(logname, "%s/tofulog-%s-%d", cp1, cp2, utf_info.myrank);
	    } else {
		sprintf(logname, "%s/tofulog-%d", cp1, utf_info.myrank);
	    }
        } else {
	    if (cp2) {
		sprintf(logname, "tofulog-%s-%d", cp2, utf_info.myrank);
	    } else {
		sprintf(logname, "tofulog-%d", utf_info.myrank);
	    }
        }
        if ((logfp = fopen(logname, "w")) == NULL) {
            /* where we have to print ? /dev/console ? */
            fprintf(stderr, "Cannot create the logfile: %s\n", logname);
        } else {
            fprintf(stderr, "stderr output is now stored in the logfile: %s\n", logname);
            fclose(stderr);
            stderr = logfp;
        }
    }
}

int
utf_printf(const char *fmt, ...)
{
    va_list	ap;
    int		rc;

    fprintf(stderr, "[%d] ", utf_info.myrank);
    va_start(ap, fmt);
    rc = vfprintf(stderr, fmt, ap);
    va_end(ap);
    fflush(stderr);
    return rc;
}

int
myprintf(const char *fmt, ...)
{
    va_list	ap;
    int		rc;
    printf("[%d] ", utf_info.myrank); fprintf(stderr, "[%d] ", utf_info.myrank);
    va_start(ap, fmt);
    rc = vprintf(fmt, ap);

    va_start(ap, fmt);
    rc = vfprintf(stderr, fmt, ap);
    va_end(ap);

    fflush(stdout); fflush(stderr);
    return rc;
}

int
utf_getenvint(char *envp)
{
    int	val = 0;
    char	*cp = getenv(envp);
    if (cp) {
	if (!strncmp(cp, "0x", 2)) {
	    sscanf(cp, "%x", &val);
	} else {
	    val = atoi(cp);
	}
    }
    return val;
}

void
utf_msglist_show(char *msg, utfslist_t *lst)
{
    struct utf_msglst	*msl;
    utfslist_entry_t	*cur;
    utfslist_foreach(lst, cur) {
	msl = container_of(cur, struct utf_msglst, slst);
	utf_printf("\t%s msl(%p) src(%d) tag(%x) rvignr(%lx) reqidx(%d) req(%p)\n",
		   msg, msl, msl->hdr.src, msl->hdr.tag, ~msl->fi_ignore,
		   msl->reqidx, utf_idx2msgreq(msl->reqidx));
    }
}

void
utf_allmsglist_show()
{
    utf_printf("***** Tagged Message Expected List *****\n");
    utf_msglist_show("exp-tagged", &tfi_tag_explst);
    utf_printf("***** Multi-recv Message Expected List *****\n");
    utf_msglist_show("exp-multi", &tfi_msg_explst);

    utf_printf("***** Tagged Message Unexpected list *****\n");
    utf_msglist_show("unexp-tagged", &tfi_tag_uexplst);
    utf_printf("***** Multi-recv Unexpected List *****\n");
    utf_msglist_show("unexp-multi", &tfi_msg_uexplst);
    {
	extern void tofu_cq_show();
	tofu_cq_show();
    }
}


#include <signal.h>
#include<sys/time.h>
int		utf_dbg_timer;
int		utf_dbg_timact;

void
handle_sigtimer(int signum, siginfo_t *info, void *p)
{
    utf_sendctr_show();
    utf_recvcntr_show(stderr);
    utf_allmsglist_show();
}

void
utf_dbg_init()
{
    struct sigaction	sigact;
    struct itimerval	tim;
    int ret;

    if (utf_dbg_timer == 0) return;

    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_SIGINFO;
    sigact.sa_sigaction = handle_sigtimer;
    sigaction(SIGALRM, &sigact, NULL);
    tim.it_value.tv_sec = utf_dbg_timer;
    tim.it_value.tv_usec = 0;
    tim.it_interval.tv_sec = utf_dbg_timer;
    tim.it_interval.tv_usec = 0;
    ret = setitimer(ITIMER_REAL, &tim, NULL);
    if (ret < 0) {
	utf_printf("%s: setimer cannot set\n", __func__);
	perror(__func__);
    } else {
	DEBUG(DLEVEL_INIFIN) {
	    if (utf_info.myrank == 0) {
		utf_printf("%s: setimer %d sec, action is %d\n", __func__, utf_dbg_timer, utf_dbg_timact);
	    }
	}
    }
}
