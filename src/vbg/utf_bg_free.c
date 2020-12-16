/*
 * Copyright (C) 2020 RIKEN, Japan. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 */

/**
 * @file   utf_bg_free.c
 * @brief  Implementation of utf_bg_free()
 */

/*
 * include header files
 */
#include "utf_bg_internal.h"

/**
 * Release the barrier network secured by the processes participating in
 * collective communication.
 * Call the uTofu function utofu_free_vbg().
 *
 * @param[in] group_struct A pointer to the structure that stores the VBG
 *                         information.
 * @return UTF_SUCCESS, UTF_ERR_INVALID_POINTER, UTF_ERR_INTERNAL, or
 *         the return codes of utofu_free_vbg()
 */
int utf_bg_free(utf_coll_group_t group_struct) {
    int rc = UTF_SUCCESS;
    int wrc;
    utf_coll_group_detail_t *detail_p;

    DEBUG(DLEVEL_PROTO_VBG) {
        utf_printf("%s: group_struct=%p\n", __func__, group_struct);
    }

    /* Check the arguments */
    if (NULL == group_struct) {
        rc = UTF_ERR_INVALID_POINTER;
        return rc;
    }
    detail_p = (utf_coll_group_detail_t *)group_struct;
    if (0 == detail_p->num_vbg && NULL == detail_p->vbg_ids) {
        /* If the VBG has been allready released by utf_bg_alloc(),
           do not call utofu_free_vbg(). */
        goto free_end;
    }
    else if((0 == detail_p->num_vbg && NULL != detail_p->vbg_ids) ||
            (1 <= detail_p->num_vbg && NULL == detail_p->vbg_ids)) {
        rc = UTF_ERR_INTERNAL;
        goto free_end;
    }

    /* Call utofu_free_vbg() to release the VBG. */
    /* Set the return code of the uTofu function as the return code of this
       function. */
    rc = utofu_free_vbg(detail_p->vbg_ids, detail_p->num_vbg);

    DEBUG(DLEVEL_PROTO_VBG) {
        utf_printf("%s: Call utofu_free_vbg: rc=%d\n", __func__, rc);
    }

free_end:
    /* Post-processiong */
    /* Freeing the memories allocated by utf_bg_alloc() */
    if (utf_bg_detail_info_first_alloc == detail_p) {
        /* If the start gate of MPI_COMM_WORLD, freeing shared memory. */
        if (!utf_bg_mmap_file_info.disable_utf_shm) {
            utf_shm_finalize();
            utf_bg_mmap_file_info.mmaparea = NULL;
        } else {
            if (NULL != utf_bg_mmap_file_info.file_name) {
                munmap((void *)utf_bg_mmap_file_info.mmaparea,
                       utf_bg_mmap_file_info.size);
                close(utf_bg_mmap_file_info.fd);
                if (UTF_BG_INTRA_NODE_MANAGER == detail_p->intra_node_info->state) {
                    do {
                        wrc = unlink(utf_bg_mmap_file_info.file_name);
                    } while(EBUSY == wrc);
                }
                free(utf_bg_mmap_file_info.file_name);
                utf_bg_mmap_file_info.file_name = NULL;
                utf_bg_mmap_file_info.mmaparea = NULL;
            }
        }
        if (1 <= detail_p->num_vbg && NULL != detail_p->vbg_ids) {
            free(detail_p->vbg_ids);
        }
        /* Freeing the temporary data buffer. */
        if (NULL != poll_info.utf_bg_poll_idata) {
            free(poll_info.utf_bg_poll_idata);
            poll_info.utf_bg_poll_idata = NULL;
        }
        if (NULL != utf_bg_alloc_intra_world_indexes) {
            free(utf_bg_alloc_intra_world_indexes);
            utf_bg_alloc_intra_world_indexes=NULL;
        }
        utf_bg_detail_info_first_alloc = NULL;
    }
    detail_p->vbg_ids = NULL;
    detail_p->num_vbg = 0;
    if (NULL != detail_p->intra_node_info){
        free(detail_p->intra_node_info);
        detail_p->intra_node_info = NULL;
    }

    free(detail_p);

    return rc;
}
