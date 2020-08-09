extern void	utf_engine_init();
extern int	utf_progress();
extern utofu_stadd_t	utf_mem_reg(utofu_vcq_hdl_t, void *buf, size_t size);
extern void		utf_mem_dereg(utofu_vcq_id_t, utofu_stadd_t stadd);
extern void	utf_stadd_free();

/* for debugging */
extern int	utf_initialized_1, utf_initialized_2;
extern size_t	utf_pig_size;	/* piggyback size */
extern utofu_stadd_t	erbstadd;
extern utofu_stadd_t	egrmgtstadd;
extern struct msg_q_check	*utf_expq_chk;
extern struct msg_q_check	*utf_uexpq_chk;
extern utofu_stadd_t	rmacmplistadd;
