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
	utf_printf("\t%s msl(%p) src(%d) tag(0x%llx) size(%ld) flgs(0x%x) rvignr(0x%llx) reqidx(%d) req(%p)\n",
		   msg, msl, msl->hdr.src, msl->hdr.tag, msl->hdr.size, msl->hdr.flgs, ~msl->fi_ignore,
		   msl->reqidx, utf_idx2msgreq(msl->reqidx));
    }
}

void
utf_msgreq_show(utfslist_t *slst)
{
    utfslist_entry_t	*cur;
    utfslist_foreach(slst, cur) {
	struct utf_msgreq *req = container_of(cur, struct utf_msgreq, busyslst);
	utf_printf("\tscur(%p) req(%p) busyslst->next(%p) src(%d) tag(0x%lx), size(%ld), state(%d) fi_flgs(0x%lx)\n",
		   cur, req, req->busyslst.next, req->hdr.src, req->hdr.tag, req->hdr.size, req->state, req->fi_flgs);
    }
}


#define LOGSZ	(1024*8)
#define LNSZ	64
static char	logbuff[LOGSZ][LNSZ];
static int	logptr, logovf;

void
utf_log(const char *fmt, ...)
{
    va_list	ap;
    int	sz;

    va_start(ap, fmt);
    if (logptr >= LOGSZ) {
	logptr = 0; logovf = 1;
    }
    sz = vsnprintf(logbuff[logptr], LNSZ, fmt, ap);
    if (sz == LNSZ && logbuff[logptr][LNSZ - 1] != '\n') {
	logbuff[logptr][LNSZ - 1] = '\n';
    }
    va_end(ap);
    logptr++;
}

void
utf_log_show(FILE *out)
{
    int	i;

    {
	extern int tofu_barrier_cnt, tofu_barrier_do, tofu_barrier_src, tofu_barrier_dst, tofu_gatherv_cnt, tofu_gatherv_do;
	fprintf(out, "@@@@@ TOFU DEBUG @@@@@\n");
	fprintf(out, "tofu_gatherv_do: %d\n", tofu_gatherv_do);
	fprintf(out, "tofu_gatherv_cnt: %d\n", tofu_gatherv_cnt);
	fprintf(out, "tofu_barrier_do: %d\n", tofu_barrier_do);
	fprintf(out, "tofu_barrier_cnt: %d\n", tofu_barrier_cnt);
	fprintf(out, "tofu_barrier_src: %d\n", tofu_barrier_src);
	fprintf(out, "tofu_barrier_dst: %d\n", tofu_barrier_dst);
    }
    fprintf(out, "<<<<< UTF LOG (%d:%d) >>>>>\n", logptr, logovf);
    if (logovf) {
	for (i = logptr; i < LOGSZ; i++) {
	    fprintf(out, "%s", logbuff[i]);
	}
    }
    for (i = 0; i < logptr; i++) {
	fprintf(out, "%s", logbuff[i]);
    }
    fprintf(out, "<<<<< END OF UTF LOG >>>>>\n");
}
