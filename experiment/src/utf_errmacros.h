extern void utofu_get_last_error(const char*);
extern int mypid;

#define SYSERRCHECK_EXIT(rc, cond, val, msg) do {			\
    if (rc cond val) {							\
	printf("[%d] error: %s @ %d in %s\n",				\
	       mypid, msg, __LINE__, __FILE__);				\
	fprintf(stderr, "[%d] error: %s  @ %d in %s\n",			\
	        mypid, msg, __LINE__, __FILE__);			\
	perror("");							\
	fflush(NULL);							\
    }									\
} while(0);

#define LIBERRCHECK_EXIT(rc, cond, val, msg) do {			\
    if (rc cond val) {							\
	printf("[%d] error: %s @ %d in %s\n",				\
	       mypid, msg, __LINE__, __FILE__);				\
	fprintf(stderr, "[%d] error: %s  @ %d in %s\n",			\
	        mypid, msg, __LINE__, __FILE__);			\
	fflush(NULL);							\
    }									\
} while(0);

#define UTOFU_ERRCHECK_EXIT(rc) do {					\
    if (rc != UTOFU_SUCCESS) {						\
	char msg[1024];							\
	utofu_get_last_error(msg);					\
	printf("[%d] error: %s (code:%d) @ %d in %s\n",			\
	       mypid, msg, rc, __LINE__, __FILE__);			\
	fprintf(stderr, "[%d] error: %s (code:%d) @ %d in %s\n",	\
	        mypid, msg, rc, __LINE__, __FILE__);			\
	fflush(stdout); fflush(stderr);					\
	abort();							\
    }									\
} while(0);

#define UTOFU_ERRCHECK_EXIT_IF(abrt, rc) do {				\
    if (rc != UTOFU_SUCCESS) {						\
	char msg[1024];							\
	utofu_get_last_error(msg);					\
	printf("[%d] error: %s (code:%d) @ %d in %s\n",			\
	       mypid, msg, rc, __LINE__, __FILE__);			\
	fprintf(stderr, "[%d] error: %s (code:%d) @ %d in %s\n",	\
	        mypid, msg, rc, __LINE__, __FILE__);			\
	fflush(stdout); fflush(stderr);					\
	if (abrt) abort();						\
    }									\
} while(0);

#define UTOFU_CALL(abrt, func, ...) do {				\
    char msg[256];							\
    int rc;								\
    DEBUG(DLEVEL_UTOFU) {						\
	snprintf(msg, 256, "%s: calling %s(%s)\n", __func__, #func, #__VA_ARGS__); \
	utf_printf("%s", msg);						\
    }									\
    rc = func(__VA_ARGS__);						\
    UTOFU_ERRCHECK_EXIT_IF(abrt, rc);					\
} while (0);

#define UTOFU_CALL_RC(rc, func, ...) do {				\
    char msg[256];							\
    DEBUG(DLEVEL_UTOFU) {						\
	snprintf(msg, 256, "%s: calling %s(%s)\n", __func__, #func, #__VA_ARGS__); \
	utf_printf("%s", msg);						\
    }									\
    rc = func(__VA_ARGS__);						\
    return rc;								\
} while (0);

#define JTOFU_ERRCHECK_EXIT_IF(abrt, rc) do {				\
    if (rc != JTOFU_SUCCESS) {						\
	char msg[1024];							\
	utofu_get_last_error(msg);					\
	printf("[%d] error: %s (code:%d) @ %d in %s\n",			\
	       mypid, msg, rc, __LINE__, __FILE__);			\
	fprintf(stderr, "[%d] error: %s (code:%d) @ %d in %s\n",	\
	        mypid, msg, rc, __LINE__, __FILE__);			\
	fflush(stdout); fflush(stderr);					\
	if (abrt) abort();						\
    }									\
} while(0);

#define JTOFU_CALL(abrt, func, ...) do {				\
    char msg[256];							\
    int rc;								\
    DEBUG(DLEVEL_UTOFU) {						\
	snprintf(msg, 256, "%s: calling %s(%s)\n", __func__, #func, #__VA_ARGS__); \
	utf_printf("%s", msg);						\
    }									\
    rc = func(__VA_ARGS__);						\
    JTOFU_ERRCHECK_EXIT_IF(abrt, rc);					\
} while (0);

#define ERRCHECK_RETURN(val1, op, val2, rc) do {			\
    if (val1 op val2) return rc;					\
} while (0);

#if 0
#define INITCHECK() do {						\
    if (utf_initialized_1 == 0 || utf_initialized_2 == 0) return -1;	\
} while (0);
#endif
#define INITCHECK() do {						\
    if (myinitialized == 0) return -1;					\
} while (0);


struct utf_dbg_info {
    uint64_t	rstadd;
    char	*file;
    uint64_t	etc;
    int		cmd;
    int		line;
};

#ifdef UTF_DEBUG
extern struct utf_dbg_info	utf_dbg_info[10];
#define DBG_UTF_CMDINFO(idx, cm, addr, val)	\
{						\
    utf_dbg_info[idx].cmd = cm;			\
    utf_dbg_info[idx].rstadd = addr;		\
    utf_dbg_info[idx].etc = (uint64_t) val;	\
    utf_dbg_info[idx].file = __FILE__;		\
    utf_dbg_info[idx].line = __LINE__;		\
} while (0);
#else
#define DBG_UTF_CMDINFO(idx, cm, addr, val)
#endif /* UTF_DEBUG */

#define UTOFU_MSIZE_CHECK(size)						\
{									\
    if (size > TOFU_PUTGET_SIZE) {					\
	utf_printf("%s: reqsize(0x%lx) TOFU_PUTGET_SIZE(0x%lx)\n",	\
		   __func__, size, TOFU_PUTGET_SIZE);			\
	abort();							\
    }									\
} while (0);

