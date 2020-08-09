/*
 * external definitions inside the UTF library
 */
extern int	utf_printf(const char *fmt, ...);
extern utofu_stadd_t	utf_mem_reg(utofu_vcq_hdl_t vcqh, void *buf, size_t size);
extern void	utf_mem_dereg(utofu_vcq_id_t vcqh, utofu_stadd_t stadd);
extern void	*utf_malloc(size_t);
extern void	utf_free(void*);
extern int	utf_rank(int *rank);
extern int	utf_nprocs(int *psize);
extern int	utf_vcqh(utofu_vcq_hdl_t *vp);
extern void	utf_req_wipe();
extern void	utf_fence();
extern void	utf_setmsgmode(int);
extern void	utf_mem_init();
extern void	utf_mem_finalize();
extern void	utf_procmap_finalize();

extern int	utf_initialized;
