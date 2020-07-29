#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>	/* for snprintf() */
#include <ctype.h>
#include "./include/pmix.h"
#include "pmix_fjext.h"
#include "tsim_debug.h"

int	tsim_dflag;
extern int _pmix_myrank;
extern int _pmix_nprocs;

#define B_SIZE 1024
#define F_SIZE 1024
static char buf[B_SIZE];
static char fname[F_SIZE];
static int  myjobid;

struct intrn_keyval {
    pmix_key_t          key;    /* char key[PMIX_MAX_KEYLEN+1] */
    pmix_value_t        *val;
};
#define _KEYVALMAX      129
#define _KEY_FIND       1
#define _KEY_REG        2
static struct intrn_keyval  _keyvaltab[_KEYVALMAX];

static int
MPL_str_get_binary_arg(const char *str, const char *flag, char *buffer,
                       int maxlen, int *out_length);

static pmix_value_t *
_pmix_value_copy(pmix_value_t *src)
{
    pmix_value_t *dst = NULL;

    dst = (pmix_value_t*)malloc(sizeof(pmix_value_t));
    if (src->type == PMIX_STRING) {
        dst->data.string = malloc(strlen(src->data.string)+1);
        strcpy(dst->data.string, src->data.string);
        dst->type = src->type;
    } else {
        fprintf(stderr, "%s: needs to handle this type (%d)\n",
                __func__, src->type); fflush(stderr);
    }
    return dst;
}

void
_show_pmix_value(pmix_value_t *val)
{
    switch (val->type) {
    case PMIX_STRING:
        fprintf(stderr, "\tval=%s\n", val->data.string); fflush(stderr);
        break;
    default:
        fprintf(stderr, "\tneeds to handle this type: %d\n", val->type); fflush(stderr);
    }
}

static int
_keyval_op(int cmd, const pmix_key_t key, pmix_value_t **val)
{
    int                 i;
    for (i = 0; i < _KEYVALMAX; i++) {
        if (_keyvaltab[i].key[0] == 0) goto find;
        if (strcmp(_keyvaltab[i].key, key) == 0) goto find;
    }
    if (cmd == _KEY_FIND) {
        return PMIX_ERR_NOT_FOUND;
    }
    /* put but no memory */
    return PMIX_ERROR;
find:
    if(cmd == _KEY_FIND) {
        *val = _keyvaltab[i].val;
        return PMIX_SUCCESS;
    }
    /* register */
    strcpy(_keyvaltab[i].key, key);
    _keyvaltab[i].val = _pmix_value_copy(*val);
    return PMIX_SUCCESS;
}

/*
 * PMIx_Resolve_nodes
 */
pmix_status_t
PMIx_Resolve_nodes(const pmix_nspace_t nspace, char **nodelist)
{
    unsigned int  nprocs = _pmix_nprocs;
    int         sz, i;
    char        *lst, *ptr;
#define NODE_NAME       "host%05x,"
#define NODE_NAME_FMT   "host%05x"
#define NODE_NM_LEN     10

    DEBUG {
	fprintf(stderr, "[%d] %s: jid=%d nprocs=%d\n", _pmix_myrank, __func__, atoi(nspace), nprocs); fflush(stderr);
    }
    sz = (sizeof(NODE_NAME)+1)*nprocs;
    lst = malloc(sz);
    if (lst == NULL) {
        fprintf(stderr, "%s: Cannot allocate memory size(%d)\n", __func__, sz); fflush(stderr);
        return -1;
    }
    memset(lst, 0, sz);
    ptr = lst;
    for (i = 0; i < nprocs; i++) {
        snprintf(ptr, sizeof(NODE_NAME) + 1, NODE_NAME, i);
        ptr += NODE_NM_LEN;
    }
    *(ptr - 1) = 0;
    DEBUG {
	fprintf(stderr, "%s: list=%s\n", __func__, lst); fflush(stderr);
    }
    *nodelist = lst;
    return PMIX_SUCCESS;
}

const char* PMIx_Scope_string(pmix_scope_t scope)
{
    switch(scope) {
        case PMIX_SCOPE_UNDEF:
            return "UNDEFINED";
        case PMIX_LOCAL:
            return "SHARE ON LOCAL NODE ONLY";
        case PMIX_REMOTE:
            return "SHARE ON REMOTE NODES ONLY";
        case PMIX_GLOBAL:
            return "SHARE ACROSS ALL NODES";
        case PMIX_INTERNAL:
            return "STORE INTERNALLY";
        default:
            return "UNKNOWN SCOPE";
    }
}

static int
MPL_str_get_binary_arg(const char *str, const char *flag, char *buffer,
                    int maxlen, int *out_length);
/*
 *      pmix_scope_t: PMIX_LOCAL, PMIX_REMOTE, PMIX_GLOBAL
 *      PMIX_STRING     3
 */
pmix_status_t
PMIx_Put(pmix_scope_t scope, const pmix_key_t key, pmix_value_t *val)
{
    FILE        *fp;
    fprintf(stderr, "[%d] %s:scope(%s) key(%s) type(%d)\n", _pmix_myrank, __func__, PMIx_Scope_string(scope), key, val->type); fflush(stderr);
    // _show_pmix_value(val);
    if(val->type == PMIX_STRING) {
        int     outlen;
        MPL_str_get_binary_arg(val->data.string, "mpi", buf, B_SIZE, &outlen);
        fprintf(stderr, "\tstring(%s) DECODED VAL = \"%s\"\n", val->data.string, buf);
    }
    switch (scope) {
    case PMIX_LOCAL:
        _keyval_op(_KEY_REG, key, &val);
        break;
    case PMIX_REMOTE:
    case PMIX_GLOBAL:
	fprintf(stderr, "[%d] %s:PMIX_REMOTE|PMIX_GLOBAL\n", _pmix_myrank, __func__); fflush(stderr);
        if (val->type == PMIX_STRING) {
            snprintf(fname, F_SIZE, "%d.%04d.%s", myjobid, _pmix_myrank, key);
            if ((fp = fopen(fname, "w")) == NULL) {
                fprintf(stderr, "Cannot create the file %s\n", fname);
                break;
            }
            fprintf(stderr, "val=%s\n", val->data.string); fflush(stderr);
            fprintf(fp, "%s\n", val->data.string); fflush(fp);
            fclose(fp);
        } else {
            fprintf(stderr, "\t Only string can be registered at this time\n");
        }
        break;
    }
    return PMIX_SUCCESS;
}

pmix_status_t
PMIx_Commit(void)
{
    //fprintf(stderr, "YI***** %s: Needs to implement\n", __func__); fflush(stderr);
    return PMIX_SUCCESS;
}

void
pmix_value_load(pmix_value_t *v, const void *data, pmix_data_type_t type)
{
    fprintf(stderr, "[%d] %s: type(%d)\n", _pmix_myrank, __func__, type); fflush(stderr);
    switch (type) {
    case PMIX_BOOL:
        memcpy(&(v->data.flag), data, 1);
        break;
    case PMIX_STRING:
        v->type = PMIX_STRING;
        v->data.string = strdup(data);
        break;
    default:
        fprintf(stderr, "[%d] %s: **** needs to implement type(%d)\n",
		_pmix_myrank, __func__, type);
        fflush(stderr);
    }
    return;
}

/*
 *      nodename comes from PMIx_Resolve_nodes
 *      nspace is jid in Tofu
 */
pmix_status_t
PMIx_Resolve_peers(const char *nodename,
                   const pmix_nspace_t nspace,
                   pmix_proc_t **procs, size_t *nprocs)
{
    int         srank;
    unsigned int         np = _pmix_nprocs;
    int         i;
    pmix_proc_t *pr;

    DEBUG {
	fprintf(stderr, "[%d] %s: jid=%d nodename=%s\n", _pmix_myrank, __func__, atoi(nspace), nodename);  fflush(stderr);
    }
    /*
     * See mpich/src/util/nodemap/build_nodemap.h for caller
     * in case of 1 process per 1 node, srank is rank of node.
     */
    sscanf(nodename, NODE_NAME_FMT, &srank);
    DEBUG {
	fprintf(stderr, ":\t srank=%d nspace=%s\n", srank, nspace);
    }
    np = 1;
    PMIX_PROC_CREATE(pr, np);
    for (i = 0; i < np; i++) {
        PMIX_PROC_LOAD(&pr[i], nspace, srank + i);
    }
    *procs = pr;
    *nprocs = np;
    DEBUG {
	fprintf(stderr, "%s: ends\n", __func__); fflush(stderr);
    }
    return PMIX_SUCCESS;
}

extern int	fbarrier(char *path, int nprocs, int myrank);
#define LOCK_PATH	"./barrier.lock"

pmix_status_t
PMIx_Fence(const pmix_proc_t procs[], size_t nprocs,
           const pmix_info_t info[], size_t ninfo)
{
    int	rc;
    fprintf(stderr, "[%d] PMIx_Fence: nprocs(%ld) ninfo(%ld) _pmix_nprocs(%d)\n",
	    _pmix_myrank, nprocs, ninfo, _pmix_nprocs); fflush(stderr);
    fprintf(stderr, "[%d] PMIx_Fence: before fbarrier\n", _pmix_myrank); fflush(stderr);
    rc = fbarrier(LOCK_PATH, _pmix_nprocs, _pmix_myrank);
    fprintf(stderr, "[%d] PMIx_Fence: after fbarrier(%d)\n", _pmix_myrank, rc); fflush(stderr);
    if (rc != 0) {
	return PMIX_ERROR;
    } else {
	return PMIX_SUCCESS;
    }
}


/*
 * See pmix_common.h
 */
void  *
_pmix_calloc(size_t nmemb, size_t size)
{
    return calloc(nmemb, size);
}

void  *
_pmix_malloc(size_t size)
{
    return malloc(size);
}

void
_pmix_free(void *p)
{
    free(p);
}


/*
 * Import from MPICH: src/pm/hydra/mpl/src/str/mpl_argstr.c
 */
#define MPL_STR_SUCCESS    0
#define MPL_STR_FAIL       1
#define MPL_STR_NOMEM      2
#define MPL_STR_TRUNCATED  3

#define MPL_STR_QUOTE_CHAR     '\"'
#define MPL_STR_QUOTE_STR      "\""
#define MPL_STR_DELIM_CHAR     '#'
#define MPL_STR_DELIM_STR      "#"
#define MPL_STR_ESCAPE_CHAR    '\\'
#define MPL_STR_HIDE_CHAR      '*'
#define MPL_STR_SEPAR_CHAR     '$'
#define MPL_STR_SEPAR_STR      "$"

#if 0
static
int MPL_snprintf(char *str, size_t size, const char *format, ...)
{
    int n;
    const char *p;
    char *out_str = str;
    va_list list;

    va_start(list, format);

    p = format;
    while (*p && size > 0) {
        char *nf;

        nf = strchr(p, '%');
        if (!nf) {
            /* No more format characters */
            while (size-- > 0 && *p) {
                *out_str++ = *p++;
            }
        } else {
            int nc;
            int width = -1;

            /* Copy until nf */
            while (p < nf && size-- > 0) {
                *out_str++ = *p++;
            }
            /* p now points at nf */
            /* Handle the format character */
            nc = nf[1];
            if (isdigit(nc)) {
                /* Get the field width */
                /* FIXME : Assumes ASCII */
                width = nc - '0';
                p = nf + 2;
                while (*p && isdigit(*p)) {
                    width = 10 * width + (*p++ - '0');
                }
                /* When there is no longer a digit, get the format
                 * character */
                nc = *p++;
            } else {
                /* Skip over the format string */
                p += 2;
            }

            switch (nc) {
                case '%':
                    *out_str++ = '%';
                    size--;
                    break;

                case 'd':
                    {
                        int val;
                        char tmp[20];
                        char *t = tmp;
                        /* Get the argument, of integer type */
                        val = va_arg(list, int);
                        sprintf(tmp, "%d", val);
                        if (width > 0) {
                            size_t tmplen = strlen(tmp);
                            /* If a width was specified, pad with spaces on the
                             * left (on the right if %-3d given; not implemented yet */
                            while (size-- > 0 && width-- > tmplen)
                                *out_str++ = ' ';
                        }
                        while (size-- > 0 && *t) {
                            *out_str++ = *t++;
                        }
                    }
                    break;

                case 'x':
                    {
                        int val;
                        char tmp[20];
                        char *t = tmp;
                        /* Get the argument, of integer type */
                        val = va_arg(list, int);
                        sprintf(tmp, "%x", val);
                        if (width > 0) {
                            size_t tmplen = strlen(tmp);
                            /* If a width was specified, pad with spaces on the
                             * left (on the right if %-3d given; not implemented yet */
                            while (size-- > 0 && width-- > tmplen)
                                *out_str++ = ' ';
                        }
                        while (size-- > 0 && *t) {
                            *out_str++ = *t++;
                        }
                    }
                    break;

                case 'p':
                    {
                        void *val;
                        char tmp[20];
                        char *t = tmp;
                        /* Get the argument, of pointer type */
                        val = va_arg(list, void *);
                        sprintf(tmp, "%p", val);
                        if (width > 0) {
                            size_t tmplen = strlen(tmp);
                            /* If a width was specified, pad with spaces on the
                             * left (on the right if %-3d given; not implemented yet */
                            while (size-- > 0 && width-- > tmplen)
                                *out_str++ = ' ';
                        }
                        while (size-- > 0 && *t) {
                            *out_str++ = *t++;
                        }
                    }
                    break;

                case 's':
                    {
                        char *s_arg;
                        /* Get the argument, of pointer to char type */
                        s_arg = va_arg(list, char *);
                        while (size-- > 0 && s_arg && *s_arg) {
                            *out_str++ = *s_arg++;
                        }
                    }
                    break;

                default:
                    /* Error, unknown case */
                    return -1;
                    break;
            }
        }
    }

    va_end(list);

    if (size-- > 0)
        *out_str++ = '\0';

    n = (int) (out_str - str);
    return n;
}

static int encode_buffer(char *dest, int dest_length, const char *src,
                         int src_length, int *num_encoded)
{
    int num_used;
    int n = 0;
    if (src_length == 0) {
        if (dest_length > 2) {
            *dest = MPL_STR_QUOTE_CHAR;
            dest++;
            *dest = MPL_STR_QUOTE_CHAR;
            dest++;
            *dest = '\0';
            *num_encoded = 0;
            return MPL_STR_SUCCESS;
        } else {
            return MPL_STR_TRUNCATED;
        }
    }
    while (src_length && dest_length) {
        num_used = MPL_snprintf(dest, dest_length, "%02X", (unsigned char) *src);
        if (num_used < 0) {
            *num_encoded = n;
            return MPL_STR_TRUNCATED;
        }
        /*MPL_DBG_MSG_FMT(STRING,VERBOSE,(MPL_DBG_FDEST," %c = %c%c",
         * ch, dest[0], dest[1])); */
        dest += num_used;
        dest_length -= num_used;
        src++;
        n++;
        src_length--;
    }
    *num_encoded = n;
    return src_length ? MPL_STR_TRUNCATED : MPL_STR_SUCCESS;
}
#endif /* 0 */

static int decode_buffer(const char *str, char *dest, int length, int *num_decoded)
{
    char hex[3];
    int value;
    int n = 0;

    if (str == NULL || dest == NULL || num_decoded == NULL)
        return MPL_STR_FAIL;
    if (length < 1) {
        *num_decoded = 0;
        if (*str == '\0')
            return MPL_STR_SUCCESS;
        return MPL_STR_TRUNCATED;
    }
    if (*str == MPL_STR_QUOTE_CHAR)
        str++;
    hex[2] = '\0';
    while (*str != '\0' && *str != MPL_STR_SEPAR_CHAR && *str != MPL_STR_QUOTE_CHAR && length) {
        hex[0] = *str;
        str++;
        hex[1] = *str;
        str++;
        if (0 == sscanf(hex, "%X", &value))
            return MPL_STR_TRUNCATED;
        *dest = (char) value;
        /*MPL_DBG_MSG_FMT(STRING,VERBOSE,(MPL_DBG_FDEST," %s = %c",
         * hex, *dest)); */
        dest++;
        n++;
        length--;
    }
    *num_decoded = n;
    if (length == 0) {
        if (*str != '\0' && *str != MPL_STR_SEPAR_CHAR && *str != MPL_STR_QUOTE_CHAR)
            return MPL_STR_TRUNCATED;
    }
    return MPL_STR_SUCCESS;
}

static const char *first_token(const char *str)
{
    if (str == NULL)
        return NULL;
    /* isspace is defined only if isascii is true */
    while (/*isascii(*str) && isspace(*str) */ *str == MPL_STR_SEPAR_CHAR)
        str++;
    if (*str == '\0')
        return NULL;
    return str;
}

static int compare_token(const char *token, const char *str)
{
    if (token == NULL || str == NULL)
        return -1;

    if (*token == MPL_STR_QUOTE_CHAR) {
        /* compare quoted strings */
        token++;        /* move over the first quote */
        /* compare characters until reaching the end of the string or the
         * end quote character */
        for (;;) {
            if (*token == MPL_STR_ESCAPE_CHAR) {
                if (*(token + 1) == MPL_STR_QUOTE_CHAR) {
                    /* move over the escape character if the next character
                     * is a quote character */
                    token++;
                }
                if (*token != *str)
                    break;
            } else {
                if (*token != *str || *token == MPL_STR_QUOTE_CHAR)
                    break;
            }
            if (*str == '\0')
                break;
            token++;
            str++;
        }
        if (*str == '\0' && *token == MPL_STR_QUOTE_CHAR)
            return 0;
        if (*token == MPL_STR_QUOTE_CHAR)
            return 1;
        if (*str < *token)
            return -1;
        return 1;
    }

    /* compare DELIM token */
    if (*token == MPL_STR_DELIM_CHAR) {
        if (*str == MPL_STR_DELIM_CHAR) {
            str++;
            if (*str == '\0')
                return 0;
            return 1;
        }
        if (*token < *str)
            return -1;
        return 1;
    }

    /* compare literals */
    while (*token == *str &&
           *str != '\0' && *token != MPL_STR_DELIM_CHAR && (*token != MPL_STR_SEPAR_CHAR)) {
        token++;
        str++;
    }
    if ((*str == '\0') &&
        (*token == MPL_STR_DELIM_CHAR || (*token == MPL_STR_SEPAR_CHAR) || *token == '\0'))
        return 0;
    if (*token == MPL_STR_DELIM_CHAR || (*token == MPL_STR_SEPAR_CHAR) || *token < *str)
        return -1;
    return 1;
}

static const char *next_token(const char *str)
{
    if (str == NULL)
        return NULL;
    str = first_token(str);
    if (str == NULL)
        return NULL;
    if (*str == MPL_STR_QUOTE_CHAR) {
        /* move over string */
        str++;  /* move over the first quote */
        if (*str == '\0')
            return NULL;
        while (*str != MPL_STR_QUOTE_CHAR) {
            /* move until the last quote, ignoring escaped quotes */
            if (*str == MPL_STR_ESCAPE_CHAR) {
                str++;
                if (*str == MPL_STR_QUOTE_CHAR)
                    str++;
            } else {
                str++;
            }
            if (*str == '\0')
                return NULL;
        }
        str++;  /* move over the last quote */
    } else {
        if (*str == MPL_STR_DELIM_CHAR) {
            /* move over the DELIM token */
            str++;
        } else {
            /* move over literal */
            while (/*(isascii(*str) &&
                         * !isspace(*str)) && */
                      *str != MPL_STR_SEPAR_CHAR && *str != MPL_STR_DELIM_CHAR && *str != '\0')
                str++;
        }
    }
    return first_token(str);
}

static int
MPL_str_get_binary_arg(const char *str, const char *flag, char *buffer,
                   int maxlen, int *out_length)
{
    if (maxlen < 1)
        return MPL_STR_FAIL;

    /* line up with the first token */
    str = first_token(str);
    if (str == NULL)
        return MPL_STR_FAIL;

    /* This loop will match the first instance of "flag = value" in the string. */
    do {
        if (compare_token(str, flag) == 0) {
            str = next_token(str);
            if (compare_token(str, MPL_STR_DELIM_STR) == 0) {
                str = next_token(str);
                if (str == NULL)
                    return MPL_STR_FAIL;
                return decode_buffer(str, buffer, maxlen, out_length);
            }
        } else {
            str = next_token(str);
        }
    } while (str);
    return MPL_STR_FAIL;
}

int
_mpich_interp_pmixget(const pmix_proc_t *proc, const char key[],
		      pmix_value_t **val)
{
    pmix_value_t    *vv;
    pmix_value_t    pmval;
    int             xc;
        /* */
    fprintf(stderr, "key = %s\n", key); fflush(stderr);
    if(proc->rank >= 0) {
	char        *cp;
	size_t	    sz;
	FILE        *fp;
	/*
	 * if key is "bc", we should create encoded Tofu address
	 * e.g.
	 * mpi#743A2F2F302E302E302E302E302E302E372F3B713D302E30000000000000
	 * 0000000000000000000000000000000000000000000000000000000000000000
	 * 0000$
	 *
	 */
	snprintf(fname, F_SIZE, "%d.%04d.%s", myjobid, proc->rank, key);
    retry:
	while ((fp = fopen(fname, "r")) == NULL) {
	    usleep(100000); /* 100 msec sleep */
	}
	fgets(buf, B_SIZE, fp); fflush(stderr);
	if ((cp = index(buf, '\n')) != NULL) *cp = 0;
	if ((sz = strlen(buf)) == 0) {
	    //printf("DATA has not been visible\n"); fflush(stdout);
	    fclose(fp);
	    goto retry;
	}
	cp = malloc(sz + 1);
	strcpy(cp, buf);
	pmval.type = PMIX_STRING;
	pmval.data.string = cp;
	vv = &pmval;
	xc = PMIX_SUCCESS;
	fclose(fp);
    } else {
	xc = _keyval_op(_KEY_FIND, key, &vv);
    }
    if (xc == PMIX_SUCCESS) {
	int outlen;
	*val = _pmix_value_copy(vv);
	MPL_str_get_binary_arg(vv->data.string, "mpi",
			       buf, B_SIZE, &outlen);
	fprintf(stderr, "\tDECODED VAL = \"%s\"\n", buf); fflush(stderr);
    }
    return xc;
}

void
_myenv_init()
{
    char	*cp;
    cp = getenv("MYJOBID");
    if (cp != NULL) {
	myjobid = atoi(cp);
    } else {
	myjobid = 0;
    }
    cp = getenv("PMIX_DEBUG");
    if (cp) {
	tsim_dflag = 1;
    }
    DEBUG {
	fprintf(stderr, "PMIx_Init: YI***** %d *******\n", myjobid); fflush(stderr);
    }
}
