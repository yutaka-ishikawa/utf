/*
 * external definitions inside the UTF library
 */
extern int	utf_printf(const char *fmt, ...);
extern utofu_stadd_t	utf_mem_reg(utofu_vcq_hdl_t vcqh, void *buf, size_t size);
extern void	utf_mem_dereg(utofu_vcq_id_t vcqh, utofu_stadd_t stadd);
extern void	*utf_malloc(size_t);
extern void	utf_free(void*);
extern struct utf_rma_cq *utf_rmacq_alloc();
extern void	utf_rmacq_free(struct utf_rma_cq *cq);
extern int	utf_rank(int *rank);
extern int	utf_nprocs(int *psize);
extern int	utf_vcqh(utofu_vcq_hdl_t *vp);
extern void	utf_req_wipe();
extern void	utf_fence();
extern void	utf_setmsgmode(int);
extern void	utf_settransmode(int);
extern void	utf_setinjcnt(int);
extern void	utf_setasendcnt(int);
extern void	utf_setarmacnt(int);
extern void	utf_mem_init();
extern void	utf_mem_finalize();
extern void	utf_procmap_finalize();
extern void	*utf_cqselect_init(int ppn, int nrnk, int ntni, utofu_tni_id_t *tnis, utofu_vcq_hdl_t *vcqhp);
extern int	utf_cqselect_finalize();
extern void	utf_mem_show(FILE*);
extern void	utf_vname_show(FILE*);
extern void	utf_cqtab_show(FILE*);
extern void	utf_egrrbuf_show(FILE*);
extern void	utf_egrbuf_show(FILE*);
extern void	utf_statistics(FILE*);

/**/
struct tofu_vname;
extern void	utf_vname_vcqid(struct tofu_vname *vnmp);

extern int	utf_initialized;
extern int	utf_mode_msg;
extern int	utf_mode_trans;
extern int	utf_mode_mrail;
