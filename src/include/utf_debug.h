#include "utf_list.h"
extern int	utf_printf(const char *fmt, ...);
extern int	utf_fprintf(FILE*, const char *fmt, ...);
extern int	myprintf(const char *fmt, ...);
extern int	utf_getenvint(char *envp);
extern void	utf_mem_show();
extern void	utf_vname_show(FILE*);
extern void	utf_cqtab_show();
extern void	utf_recvcntr_show(FILE*);
extern void	utf_redirect();
extern void	utf_utf_vname_show(FILE*);
extern void	utf_egrrbuf_show();
extern void	utf_nullfunc();
extern void	utf_egrbuf_show();
extern void	utf_msglist_show(char*, utfslist_t *);

extern int	utf_dflag, utf_rflag;
extern int	utf_nokeep;
