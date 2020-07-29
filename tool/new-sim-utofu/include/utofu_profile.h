#ifndef UTOFU_PROFILE_H
#define UTOFU_PROFILE_H

#include "utofu.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int putofu_get_onesided_tnis(
    utofu_tni_id_t **tni_ids,
    size_t          *num_tnis);

extern int putofu_get_barrier_tnis(
    utofu_tni_id_t **tni_ids,
    size_t          *num_tnis);

extern int putofu_query_onesided_caps(
    utofu_tni_id_t               tni_id,
    struct utofu_onesided_caps **tni_caps);

extern int putofu_query_barrier_caps(
    utofu_tni_id_t              tni_id,
    struct utofu_barrier_caps **tni_caps);

extern int putofu_create_vcq(
    utofu_tni_id_t     tni_id,
    unsigned long int  flags,
    utofu_vcq_hdl_t   *vcq_hdl);

extern int putofu_create_vcq_with_cmp_id(
    utofu_tni_id_t     tni_id,
    utofu_cmp_id_t     cmp_id,
    unsigned long int  flags,
    utofu_vcq_hdl_t   *vcq_hdl);

extern int putofu_free_vcq(
    utofu_vcq_hdl_t vcq_hdl);

extern int putofu_query_vcq_id(
    utofu_vcq_hdl_t  vcq_hdl,
    utofu_vcq_id_t  *vcq_id);

extern int putofu_construct_vcq_id(
    uint8_t         coords[],
    utofu_tni_id_t  tni_id,
    utofu_cq_id_t   cq_id,
    utofu_cmp_id_t  cmp_id,
    utofu_vcq_id_t *vcq_id);

extern int putofu_set_vcq_id_path(
    utofu_vcq_id_t *vcq_id,
    uint8_t         path_coords[]);

extern int putofu_query_vcq_info(
    utofu_vcq_id_t  vcq_id,
    uint8_t         coords[],
    utofu_tni_id_t *tni_id,
    utofu_cq_id_t  *cq_id,
    uint16_t       *extra_val);

extern int putofu_alloc_vbg(
    utofu_tni_id_t    tni_id,
    size_t            num_vbgs,
    unsigned long int flags,
    utofu_vbg_id_t    vbg_ids[]);

extern int putofu_free_vbg(
    utofu_vbg_id_t vbg_ids[],
    size_t         num_vbgs);

extern int putofu_set_vbg(
    struct utofu_vbg_setting vbg_settings[],
    size_t                   num_vbg_settings);

extern int putofu_query_vbg_info(
    utofu_vbg_id_t  vbg_id,
    uint8_t         coords[],
    utofu_tni_id_t *tni_id,
    utofu_bg_id_t  *bg_id,
    uint16_t       *extra_val);

extern int putofu_get_path_id(
    utofu_vcq_id_t   vcq_id,
    uint8_t          path_coords[],
    utofu_path_id_t *path_id);

extern int putofu_get_path_coords(
    utofu_vcq_id_t  vcq_id,
    utofu_path_id_t path_id,
    uint8_t         path_coords[]);

extern int putofu_reg_mem(
    utofu_vcq_hdl_t    vcq_hdl,
    void              *addr,
    size_t             size,
    unsigned long int  flags,
    utofu_stadd_t     *stadd);

extern int putofu_reg_mem_with_stag(
    utofu_vcq_hdl_t    vcq_hdl,
    void              *addr,
    size_t             size,
    unsigned int       stag,
    unsigned long int  flags,
    utofu_stadd_t     *stadd);

extern int putofu_query_stadd(
    utofu_vcq_id_t  vcq_id,
    unsigned int    stag,
    utofu_stadd_t  *stadd);

extern int putofu_dereg_mem(
    utofu_vcq_hdl_t   vcq_hdl,
    utofu_stadd_t     stadd,
    unsigned long int flags);

extern int putofu_put(
    utofu_vcq_hdl_t    vcq_hdl,
    utofu_vcq_id_t     rmt_vcq_id,
    utofu_stadd_t      lcl_stadd,
    utofu_stadd_t      rmt_stadd,
    size_t             length,
    uint64_t           edata,
    unsigned long int  flags,
    void              *cbdata);

extern int putofu_put_gap(
    utofu_vcq_hdl_t    vcq_hdl,
    utofu_vcq_id_t     rmt_vcq_id,
    utofu_stadd_t      lcl_stadd,
    utofu_stadd_t      rmt_stadd,
    size_t             length,
    uint64_t           edata,
    unsigned long int  flags,
    size_t             mtu,
    size_t             gap,
    void              *cbdata);

extern int putofu_put_stride(
    utofu_vcq_hdl_t    vcq_hdl,
    utofu_vcq_id_t     rmt_vcq_id,
    utofu_stadd_t      lcl_stadd,
    utofu_stadd_t      rmt_stadd,
    size_t             length,
    size_t             stride,
    size_t             num_blocks,
    uint64_t           edata,
    unsigned long int  flags,
    void              *cbdata);

extern int putofu_put_stride_gap(
    utofu_vcq_hdl_t    vcq_hdl,
    utofu_vcq_id_t     rmt_vcq_id,
    utofu_stadd_t      lcl_stadd,
    utofu_stadd_t      rmt_stadd,
    size_t             length,
    size_t             stride,
    size_t             num_blocks,
    uint64_t           edata,
    unsigned long int  flags,
    size_t             mtu,
    size_t             gap,
    void              *cbdata);

extern int putofu_put_piggyback(
    utofu_vcq_hdl_t    vcq_hdl,
    utofu_vcq_id_t     rmt_vcq_id,
    void              *lcl_data,
    utofu_stadd_t      rmt_stadd,
    size_t             length,
    uint64_t           edata,
    unsigned long int  flags,
    void              *cbdata);

extern int putofu_put_piggyback8(
    utofu_vcq_hdl_t    vcq_hdl,
    utofu_vcq_id_t     rmt_vcq_id,
    uint64_t           lcl_data,
    utofu_stadd_t      rmt_stadd,
    size_t             length,
    uint64_t           edata,
    unsigned long int  flags,
    void              *cbdata);

extern int putofu_get(
    utofu_vcq_hdl_t    vcq_hdl,
    utofu_vcq_id_t     rmt_vcq_id,
    utofu_stadd_t      lcl_stadd,
    utofu_stadd_t      rmt_stadd,
    size_t             length,
    uint64_t           edata,
    unsigned long int  flags,
    void              *cbdata);

extern int putofu_get_gap(
    utofu_vcq_hdl_t    vcq_hdl,
    utofu_vcq_id_t     rmt_vcq_id,
    utofu_stadd_t      lcl_stadd,
    utofu_stadd_t      rmt_stadd,
    size_t             length,
    uint64_t           edata,
    unsigned long int  flags,
    size_t             mtu,
    size_t             gap,
    void              *cbdata);

extern int putofu_get_stride(
    utofu_vcq_hdl_t    vcq_hdl,
    utofu_vcq_id_t     rmt_vcq_id,
    utofu_stadd_t      lcl_stadd,
    utofu_stadd_t      rmt_stadd,
    size_t             length,
    size_t             stride,
    size_t             num_blocks,
    uint64_t           edata,
    unsigned long int  flags,
    void              *cbdata);

extern int putofu_get_stride_gap(
    utofu_vcq_hdl_t    vcq_hdl,
    utofu_vcq_id_t     rmt_vcq_id,
    utofu_stadd_t      lcl_stadd,
    utofu_stadd_t      rmt_stadd,
    size_t             length,
    size_t             stride,
    size_t             num_blocks,
    uint64_t           edata,
    unsigned long int  flags,
    size_t             mtu,
    size_t             gap,
    void              *cbdata);

extern int putofu_armw4(
    utofu_vcq_hdl_t     vcq_hdl,
    utofu_vcq_id_t      rmt_vcq_id,
    enum utofu_armw_op  armw_op,
    uint32_t            op_value,
    utofu_stadd_t       rmt_stadd,
    uint64_t            edata,
    unsigned long int   flags,
    void               *cbdata);

extern int putofu_armw8(
    utofu_vcq_hdl_t     vcq_hdl,
    utofu_vcq_id_t      rmt_vcq_id,
    enum utofu_armw_op  armw_op,
    uint64_t            op_value,
    utofu_stadd_t       rmt_stadd,
    uint64_t            edata,
    unsigned long int   flags,
    void               *cbdata);

extern int putofu_cswap4(
    utofu_vcq_hdl_t    vcq_hdl,
    utofu_vcq_id_t     rmt_vcq_id,
    uint32_t           old_value,
    uint32_t           new_value,
    utofu_stadd_t      rmt_stadd,
    uint64_t           edata,
    unsigned long int  flags,
    void              *cbdata);

extern int putofu_cswap8(
    utofu_vcq_hdl_t    vcq_hdl,
    utofu_vcq_id_t     rmt_vcq_id,
    uint64_t           old_value,
    uint64_t           new_value,
    utofu_stadd_t      rmt_stadd,
    uint64_t           edata,
    unsigned long int  flags,
    void              *cbdata);

extern int putofu_nop(
    utofu_vcq_hdl_t    vcq_hdl,
    unsigned long int  flags,
    void              *cbdata);

extern int putofu_prepare_put(
    utofu_vcq_hdl_t    vcq_hdl,
    utofu_vcq_id_t     rmt_vcq_id,
    utofu_stadd_t      lcl_stadd,
    utofu_stadd_t      rmt_stadd,
    size_t             length,
    uint64_t           edata,
    unsigned long int  flags,
    void              *desc,
    size_t            *desc_size);

extern int putofu_prepare_put_gap(
    utofu_vcq_hdl_t    vcq_hdl,
    utofu_vcq_id_t     rmt_vcq_id,
    utofu_stadd_t      lcl_stadd,
    utofu_stadd_t      rmt_stadd,
    size_t             length,
    uint64_t           edata,
    unsigned long int  flags,
    size_t             mtu,
    size_t             gap,
    void              *desc,
    size_t            *desc_size);

extern int putofu_prepare_put_stride(
    utofu_vcq_hdl_t    vcq_hdl,
    utofu_vcq_id_t     rmt_vcq_id,
    utofu_stadd_t      lcl_stadd,
    utofu_stadd_t      rmt_stadd,
    size_t             length,
    size_t             stride,
    size_t             num_blocks,
    uint64_t           edata,
    unsigned long int  flags,
    void              *desc,
    size_t            *desc_size);

extern int putofu_prepare_put_stride_gap(
    utofu_vcq_hdl_t    vcq_hdl,
    utofu_vcq_id_t     rmt_vcq_id,
    utofu_stadd_t      lcl_stadd,
    utofu_stadd_t      rmt_stadd,
    size_t             length,
    size_t             stride,
    size_t             num_blocks,
    uint64_t           edata,
    unsigned long int  flags,
    size_t             mtu,
    size_t             gap,
    void              *desc,
    size_t            *desc_size);

extern int putofu_prepare_put_piggyback(
    utofu_vcq_hdl_t    vcq_hdl,
    utofu_vcq_id_t     rmt_vcq_id,
    void              *lcl_data,
    utofu_stadd_t      rmt_stadd,
    size_t             length,
    uint64_t           edata,
    unsigned long int  flags,
    void              *desc,
    size_t            *desc_size);

extern int putofu_prepare_put_piggyback8(
    utofu_vcq_hdl_t    vcq_hdl,
    utofu_vcq_id_t     rmt_vcq_id,
    uint64_t           lcl_data,
    utofu_stadd_t      rmt_stadd,
    size_t             length,
    uint64_t           edata,
    unsigned long int  flags,
    void              *desc,
    size_t            *desc_size);

extern int putofu_prepare_get(
    utofu_vcq_hdl_t    vcq_hdl,
    utofu_vcq_id_t     rmt_vcq_id,
    utofu_stadd_t      lcl_stadd,
    utofu_stadd_t      rmt_stadd,
    size_t             length,
    uint64_t           edata,
    unsigned long int  flags,
    void              *desc,
    size_t            *desc_size);

extern int putofu_prepare_get_gap(
    utofu_vcq_hdl_t    vcq_hdl,
    utofu_vcq_id_t     rmt_vcq_id,
    utofu_stadd_t      lcl_stadd,
    utofu_stadd_t      rmt_stadd,
    size_t             length,
    uint64_t           edata,
    unsigned long int  flags,
    size_t             mtu,
    size_t             gap,
    void              *desc,
    size_t            *desc_size);

extern int putofu_prepare_get_stride(
    utofu_vcq_hdl_t    vcq_hdl,
    utofu_vcq_id_t     rmt_vcq_id,
    utofu_stadd_t      lcl_stadd,
    utofu_stadd_t      rmt_stadd,
    size_t             length,
    size_t             stride,
    size_t             num_blocks,
    uint64_t           edata,
    unsigned long int  flags,
    void              *desc,
    size_t            *desc_size);

extern int putofu_prepare_get_stride_gap(
    utofu_vcq_hdl_t    vcq_hdl,
    utofu_vcq_id_t     rmt_vcq_id,
    utofu_stadd_t      lcl_stadd,
    utofu_stadd_t      rmt_stadd,
    size_t             length,
    size_t             stride,
    size_t             num_blocks,
    uint64_t           edata,
    unsigned long int  flags,
    size_t             mtu,
    size_t             gap,
    void              *desc,
    size_t            *desc_size);

extern int putofu_prepare_armw4(
    utofu_vcq_hdl_t     vcq_hdl,
    utofu_vcq_id_t      rmt_vcq_id,
    enum utofu_armw_op  armw_op,
    uint32_t            op_value,
    utofu_stadd_t       rmt_stadd,
    uint64_t            edata,
    unsigned long int   flags,
    void               *desc,
    size_t             *desc_size);

extern int putofu_prepare_armw8(
    utofu_vcq_hdl_t     vcq_hdl,
    utofu_vcq_id_t      rmt_vcq_id,
    enum utofu_armw_op  armw_op,
    uint64_t            op_value,
    utofu_stadd_t       rmt_stadd,
    uint64_t            edata,
    unsigned long int   flags,
    void               *desc,
    size_t             *desc_size);

extern int putofu_prepare_cswap4(
    utofu_vcq_hdl_t    vcq_hdl,
    utofu_vcq_id_t     rmt_vcq_id,
    uint32_t           old_value,
    uint32_t           new_value,
    utofu_stadd_t      rmt_stadd,
    uint64_t           edata,
    unsigned long int  flags,
    void              *desc,
    size_t            *desc_size);

extern int putofu_prepare_cswap8(
    utofu_vcq_hdl_t    vcq_hdl,
    utofu_vcq_id_t     rmt_vcq_id,
    uint64_t           old_value,
    uint64_t           new_value,
    utofu_stadd_t      rmt_stadd,
    uint64_t           edata,
    unsigned long int  flags,
    void              *desc,
    size_t            *desc_size);

extern int putofu_prepare_nop(
    utofu_vcq_hdl_t    vcq_hdl,
    unsigned long int  flags,
    void              *desc,
    size_t            *desc_size);

extern int putofu_post_toq(
    utofu_vcq_hdl_t  vcq_hdl,
    void            *desc,
    size_t           desc_size,
    void            *cbdata);

extern int putofu_poll_tcq(
    utofu_vcq_hdl_t     vcq_hdl,
    unsigned long int   flags,
    void              **cbdata);

extern int putofu_poll_mrq(
    utofu_vcq_hdl_t          vcq_hdl,
    unsigned long int        flags,
    struct utofu_mrq_notice *notice);

extern int putofu_query_num_unread_tcq(
    utofu_vcq_hdl_t  vcq_hdl,
    size_t          *count);

extern int putofu_query_num_unread_mrq(
    utofu_vcq_hdl_t  vcq_hdl,
    size_t          *count);

extern int putofu_barrier(
    utofu_vbg_id_t    vbg_id,
    unsigned long int flags);

extern int putofu_reduce_uint64(
    utofu_vbg_id_t       vbg_id,
    enum utofu_reduce_op op,
    uint64_t             data[],
    size_t               num_data,
    unsigned long int    flags);

extern int putofu_reduce_double(
    utofu_vbg_id_t       vbg_id,
    enum utofu_reduce_op op,
    double               data[],
    size_t               num_data,
    unsigned long int    flags);

extern int putofu_poll_barrier(
    utofu_vbg_id_t    vbg_id,
    unsigned long int flags);

extern int putofu_poll_reduce_uint64(
    utofu_vbg_id_t    vbg_id,
    unsigned long int flags,
    uint64_t          data[]);

extern int putofu_poll_reduce_double(
    utofu_vbg_id_t    vbg_id,
    unsigned long int flags,
    double            data[]);

extern void putofu_query_tofu_version(
    int *major_ver,
    int *minor_ver);

extern void putofu_query_utofu_version(
    int *major_ver,
    int *minor_ver);

extern int putofu_query_my_coords(
    uint8_t coords[]);

#ifdef __cplusplus
}
#endif

#endif /* UTOFU_PROFILE_H */
