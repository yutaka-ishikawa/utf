/*
 * Copyright (C) 2020 RIKEN, Japan. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 */

/**
 * @file   utf_bg_init.c
 * @brief  Implementation of utf_bg_init()
 */

#include "utf_bg_internal.h"
#include "utf_bg_init.h"

#if defined(UTF_INTERNAL_DEBUG)

/* Flag for drawing barrier network */
bool flg_draw_barrier_network=false;
/* Flag for checking the environment */
static bool flg_draw_check=false;
/* File name for drawing barrier network */
static char *draw_filename;

/**
 * Start drawing the barrier network
 *
 * @param none
 * @return none
 */
void utf_bg_start_drawing(void) {
    flg_draw_barrier_network = true;
}

/**
 * End drawing the barrier network
 *
 * @param none
 * @return none
 */
void utf_bg_end_drawing(void) {
    flg_draw_barrier_network = false;
}

/**
 * Draw the configuration diagram of the barrier network (for internal debug).
 *
 * @param[in] vbg_val  An array of structures that stores the VBG information.
 * @param[in] my_index Rank of own process
 * @return none
 */
static void draw_barrier_network(struct utofu_vbg_setting *vbg_val,
                                 size_t my_index) {
    utf_bg_vbg_info_t vbg, sl_vbg, sr_vbg, dl_vbg, dr_vbg;
    utf_rmt_src_dst_index_t *rindex;
    bool sl_is_null, sr_is_null, dl_is_null, dr_is_null;
    size_t i, rrow, npos;
    FILE *ofp;

    if (!flg_draw_barrier_network) {
        return;
    }

    /* Check the environment variable. */
    if (!flg_draw_check) {
        flg_draw_check = true;
        if (NULL == (draw_filename=getenv("UTF_DRAW_BARRIER_NETWORK"))) {
            return;
        }
    }
    if (NULL == (ofp=fopen(draw_filename, "a"))) {
        return;
    }

    rrow = utf_bg_detail_info->num_vbg - 1;
    rindex = utf_bg_detail_info->rmt_src_dst_seqs;
    npos = my_index;
#if defined(DEBUGLOG)
    fprintf(stderr,
            "%s: my_index=%zu num vbg=%zu utf_bg_alloc_count=%zu size=%zu npos=%zu\n",
            __func__, my_index, utf_bg_detail_info->num_vbg, utf_bg_alloc_count,
            utf_bg_detail_info->intra_node_info->size, npos);
    fflush(stderr);
#endif /* DEBUGLOG */
    for (i = 0; i < utf_bg_detail_info->num_vbg; i++, vbg_val++) {
        /* If the VBG ID is not UTOFU_VBG_ID_NULL, set each VBG information and
           rank in the structure for drawing. */
        if (vbg_val->vbg_id == UTOFU_VBG_ID_NULL) {
            continue;
        }
        vbg.rank   = my_index;
        vbg.bgid   = UTF_BG_VBG_BGID(vbg_val->vbg_id);
        vbg.tni    = UTF_BG_VBG_TNI(vbg_val->vbg_id);
        UTF_BG_VBG_COORDS(vbg_val->vbg_id, vbg.coords);
        /* Input signal source local VBG */
        if (vbg_val->src_lcl_vbg_id == UTOFU_VBG_ID_NULL) {
            sl_is_null = true;
        } else {
            sl_is_null = false;
            sl_vbg.rank   = my_index;
            sl_vbg.bgid   = UTF_BG_VBG_BGID(vbg_val->src_lcl_vbg_id);
            sl_vbg.tni    = UTF_BG_VBG_TNI(vbg_val->src_lcl_vbg_id);
            UTF_BG_VBG_COORDS(vbg_val->src_lcl_vbg_id, sl_vbg.coords);
        }
        /* Input packet source remote VBG */
        if (vbg_val->src_rmt_vbg_id == UTOFU_VBG_ID_NULL) {
            sr_is_null = true;
        } else {
            sr_is_null = false;
            sr_vbg.rank   = rindex[i].src_rmt_index;
            sr_vbg.bgid   = UTF_BG_VBG_BGID(vbg_val->src_rmt_vbg_id);
            sr_vbg.tni    = UTF_BG_VBG_TNI(vbg_val->src_rmt_vbg_id);
            UTF_BG_VBG_COORDS(vbg_val->src_rmt_vbg_id, sr_vbg.coords);
        }
        /* Output signal destination local VBG */
        if (vbg_val->dst_lcl_vbg_id == UTOFU_VBG_ID_NULL) {
            dl_is_null = true;
        } else {
            dl_is_null = false;
            dl_vbg.rank   = my_index;
            dl_vbg.bgid   = UTF_BG_VBG_BGID(vbg_val->dst_lcl_vbg_id);
            dl_vbg.tni    = UTF_BG_VBG_TNI(vbg_val->dst_lcl_vbg_id);
            UTF_BG_VBG_COORDS(vbg_val->dst_lcl_vbg_id, dl_vbg.coords);
        }
        /* Output packet destination remote VBG */
        if (vbg_val->dst_rmt_vbg_id == UTOFU_VBG_ID_NULL) {
            dr_is_null = true;
        } else {
            dr_is_null = false;
            dr_vbg.rank   = rindex[i].dst_rmt_index;
            dr_vbg.bgid   = UTF_BG_VBG_BGID(vbg_val->dst_rmt_vbg_id);
            dr_vbg.tni    = UTF_BG_VBG_TNI(vbg_val->dst_rmt_vbg_id);
            UTF_BG_VBG_COORDS(vbg_val->dst_rmt_vbg_id, dr_vbg.coords);
        }

        /* Draw the arrows and the nodes. */
        if (UTF_BG_MAX_START_BG_NUMBER >= vbg.bgid) {
            /* Draw the the start/end gate node. */
            UTF_BG_DRAW_NODE(ofp, vbg, I_START, npos,
                             utf_bg_detail_info->num_vbg, "shape=invhouse");
            UTF_BG_DRAW_NODE(ofp, vbg, I_END, npos, 0L, "shape=house");
            /* Draw the arrows if there is a source/destination VBG. */
            if (!sl_is_null) {
                UTF_BG_DRAW_EDGE(ofp, sl_vbg, I_RELAY, vbg, I_END, ATTR_E_SL);
            }
            if (!sr_is_null) {
                UTF_BG_DRAW_EDGE(ofp, sr_vbg, I_RELAY, vbg, I_END, ATTR_E_SR);
            }
            if (!dl_is_null) {
                UTF_BG_DRAW_EDGE(ofp, vbg, I_START, dl_vbg, I_RELAY, ATTR_E_DL);
            }
            if (!dr_is_null) {
                UTF_BG_DRAW_EDGE(ofp, vbg, I_START, dr_vbg, I_RELAY, ATTR_E_DR);
            }
        } else {
            /* Draw the relay gate node. */
            UTF_BG_DRAW_NODE(ofp, vbg, I_RELAY, npos, rrow--, "color=dimgray");
            /* Draw the arrows if there is a source/destination VBG. */
            /* The identification information of the node is determined
               according to whether the source/destination VBG is the start/end
               BG or the relay BG. */
            if (!sl_is_null) {
                UTF_BG_DRAW_EDGE(ofp, sl_vbg,
                                 (UTF_BG_MAX_START_BG_NUMBER >= sl_vbg.bgid ?
                                  I_START:I_RELAY),
                                 vbg, I_RELAY, ATTR_E_SL);
            }
            if (!sr_is_null) {
                UTF_BG_DRAW_EDGE(ofp, sr_vbg,
                                 (UTF_BG_MAX_START_BG_NUMBER >= sr_vbg.bgid ?
                                  I_START:I_RELAY),
                                 vbg, I_RELAY, ATTR_E_SR);
            }
            if (!dl_is_null) {
                UTF_BG_DRAW_EDGE(ofp, vbg, I_RELAY, dl_vbg, 
                                 (UTF_BG_MAX_START_BG_NUMBER >= dl_vbg.bgid ?
                                  I_END:I_RELAY),
                                 ATTR_E_DL);
            }
            if (!dr_is_null) {
                UTF_BG_DRAW_EDGE(ofp, vbg, I_RELAY, dr_vbg,
                                 (UTF_BG_MAX_START_BG_NUMBER >= dr_vbg.bgid ?
                                  I_END:I_RELAY),
                                 ATTR_E_DR);
            }
        }
    } /* for */

    fclose(ofp);
    return;
}
#endif /* UTF_INTERNAL_DEBUG */


/**
 * Call the uTofu function utofu_set_vbg() to set up the barrier network.

 * @param[in] rankset       An array of ranks in MPI_COMM_WORLD for the processes
                            belonging to the barrier network
 * @param[in] len           Total number of the process.
 * @param[in] my_index      Rank of the process.
 * @param[in] bginfo        A pointer to the array of the VBG information of
 *                          the process.
 * @param[out] group_struct A pointer to the structure that stores the VBG
 *                          information.
 * @return UTF_SUCCESS, UTF_ERR_RESOURCE_BUSY, UTF_ERR_INTERNAL, or
 *         the return codes of utofu_set_vbg()
 */
int utf_bg_init(
                uint32_t rankset[],
                size_t            len,
                size_t            my_index,
                utf_bg_info_t     bginfo[],
                utf_coll_group_t *group_struct) {

    int rc = UTF_SUCCESS;
    int wrc;
    size_t i;
    utf_bg_info_t disable = UTF_BIT_BGINFO_DISABLE;
    struct utofu_vbg_setting *settings;
    utf_rmt_src_dst_index_t *rindex;
    utofu_vbg_id_t src_vbg, dst_vbg;
    size_t src_idx, dst_idx;

    DEBUG(DLEVEL_PROTO_VBG) {
        utf_printf("%s: rankset=%p len=%zu my_index=%zu bginfo=%p group_struct=%p\n",
                   __func__, rankset, len, my_index, bginfo, group_struct);
    }

    /* Check the arguments. */
    if (NULL == rankset || NULL == bginfo || NULL == group_struct) {
        rc = UTF_ERR_INVALID_POINTER;
        return rc;
    }
    if (0 == len) {
        rc = UTF_ERR_INVALID_NUMBER;
        return rc;
    }
    if (NULL == utf_bg_detail_info ||
        rankset  != utf_bg_detail_info->arg_info.rankset ||
        len      != utf_bg_detail_info->arg_info.len     ||
        my_index != utf_bg_detail_info->arg_info.my_index) {
        /* Different from the arguments of utf_bg_alloc(). */
        rc = UTF_ERR_INVALID_POINTER;
        return rc;
    }
    if (NULL == utf_bg_detail_info->intra_node_info) {
        /* The required resource have not been created at utf_bg_alloc(). */
        DEBUG(DLEVEL_PROTO_VBG) {
            utf_printf("%s: utf_bg_detail_info->intra_node_info == NULL: my_index=%zu\n",
                       __func__, my_index);
        }
        return UTF_ERR_INTERNAL;
    }

    if (utf_bg_detail_info_first_alloc == utf_bg_detail_info) {
        /* The worker process in a node opens the mmap files and calls mmap(). */
        if (UTF_BG_INTRA_NODE_WORKER ==
                utf_bg_detail_info->intra_node_info->state) {
            if (!utf_bg_mmap_file_info.disable_utf_shm) {
#if defined(TOFUGIJI)
                if (0 != (wrc = utf_shm_init(utf_bg_mmap_file_info.size,
                                             (void **)(&utf_bg_mmap_file_info.mmaparea)))) {
                    utf_printf("%s: utf_shm_init() FAILED: my_index=%zu size=%zu wrc=%d\n",
                               __func__, my_index, utf_bg_mmap_file_info.size, wrc);
                    return UTF_ERR_INTERNAL;
                }
#else
#if defined(DEBUGLOG)
                fprintf(stderr,
                        "%s: utf_shm_init() was already executed at utf_bg_alloc:"
                        " my_index=%zu size=%zu\n",
                        __func__, my_index, utf_bg_mmap_file_info.size);
#endif /* DEBUGLOG */
#endif
            } else {
                utf_bg_mmap_file_info.fd = open(utf_bg_mmap_file_info.file_name,
                                                O_RDWR, 0666);
                if (-1 == utf_bg_mmap_file_info.fd) {
                    utf_printf("%s: open() FAILED: my_index=%zu file_name=%s errno=%d\n",
                               __func__, my_index, utf_bg_mmap_file_info.file_name, errno);
                    free(utf_bg_mmap_file_info.file_name);
                    utf_bg_mmap_file_info.file_name = NULL;
                    return UTF_ERR_INTERNAL;
                }
     
                utf_bg_mmap_file_info.mmaparea = mmap(0,
                                                      utf_bg_mmap_file_info.size,
                                                      PROT_READ | PROT_WRITE,
                                                      MAP_SHARED,
                                                      utf_bg_mmap_file_info.fd, 0);
                if (MAP_FAILED == utf_bg_mmap_file_info.mmaparea) {
                    utf_printf("%s: mmap() FAILED: my_index=%zu size=%zu fd=%d errno=%d\n",
                               __func__, my_index, utf_bg_mmap_file_info.size,
                               utf_bg_mmap_file_info.fd, errno);
                    close(utf_bg_mmap_file_info.fd);
                    free(utf_bg_mmap_file_info.file_name);
                    utf_bg_mmap_file_info.file_name = NULL;
                    return UTF_ERR_OUT_OF_MEMORY;
                }
            }
        }
    }
    if (NULL == utf_bg_mmap_file_info.mmaparea) {
        /* The required resource have not been created. */
        DEBUG(DLEVEL_PROTO_VBG) {
            utf_printf("%s: utf_bg_mmap_file_info.mmaparea == NULL: my_index=%zu\n",
                       __func__, my_index);
        }
        return UTF_ERR_INTERNAL;
    }

    /* Check the availability of the barreir network. */
    for (i = 0; i < len; i++) {
        if (bginfo[i] & disable) {
            /* Barrier network cannot be available.
               Only post-processing is perfomed. */
            rc = UTF_ERR_RESOURCE_BUSY;

            /* Notify that there is a process that could not allocate VBG
               at the first utf_bg_alloc(). */
            if(utf_bg_detail_info_first_alloc == utf_bg_detail_info) {
                utf_bg_mmap_file_info.first_alloc_is_failed = true;
            }

            /* Set the group_struct.
               (Consider that utf_bg_free() will be called later.) */
            *group_struct = (utf_coll_group_t)utf_bg_detail_info;

            DEBUG(DLEVEL_PROTO_VBG) {
                utf_printf("%s: RESOURCE BUSY: my_index=%zu bginfo[%zu]=0x%lx\n",
                           __func__, my_index, i, bginfo[i]);
            }
            goto init_end;
        }
    }

    /* Do the processing for barrier network. */
    if (utf_bg_intra_node_barrier_is_tofu ||
        UTF_BG_INTRA_NODE_MANAGER == utf_bg_detail_info->intra_node_info->state) {

        /* Set the utofu_vbg_setting. */
        /* The local information is set by utf_bg_alloc(), so this function set
           the remote information. */
        settings = utf_bg_detail_info->vbg_settings;
        rindex = utf_bg_detail_info->rmt_src_dst_seqs;
        for (i = 0; i < utf_bg_detail_info->num_vbg; i++) {
            /* Use the bginfo reference indexes (set by utf_bg_alloc()). */
            src_idx = rindex[i].src_rmt_index;
            dst_idx = rindex[i].dst_rmt_index;
            if (src_idx == UTF_BG_BGINFO_INVALID_INDEX) {
                /* There is no source VBG. */
                src_vbg = UTOFU_VBG_ID_NULL;
            } else {
                /* Create the VBG ID from bginfo(TNI ID, compute node coordinates,
                   and the source BG ID). */
                src_vbg = UTF_GET_BGINFO_TNI_CNC(bginfo[src_idx]);
                src_vbg |= UTF_GET_BGINFO_SRCBGID(bginfo[src_idx]);
            }
            if (dst_idx == UTF_BG_BGINFO_INVALID_INDEX) {
                /* There is no destination VBG. */
                dst_vbg = UTOFU_VBG_ID_NULL;
            } else {
                /* Create the VBG ID from bginfo(TNI ID, compute node coordinates,
                   and the destination BG ID). */
                dst_vbg = UTF_GET_BGINFO_TNI_CNC(bginfo[dst_idx]);
                dst_vbg |= UTF_GET_BGINFO_DSTBGID(bginfo[dst_idx]);
            }
            settings[i].src_rmt_vbg_id = src_vbg;
            settings[i].dst_rmt_vbg_id = dst_vbg;
            settings[i].dst_path_coords[0] = UTOFU_PATH_COORD_NULL;
        }

        DEBUG(DLEVEL_PROTO_VBG) {
            for (i = 0; i < utf_bg_detail_info->num_vbg; i++) {
                utf_printf("%s: my_index=%zu: num vbg=%zu/%zu vbg_id=%016lx "
                           "src_lcl_vbg_id=%016lx src_rmt_vbg_id=%016lx "
                           "dst_lcl_vbg_id=%016lx dst_rmt_vbg_id=%016lx\n",
                           __func__, my_index, i, utf_bg_detail_info->num_vbg,
                           settings[i].vbg_id,
                           settings[i].src_lcl_vbg_id, settings[i].src_rmt_vbg_id,
                           settings[i].dst_lcl_vbg_id, settings[i].dst_rmt_vbg_id);
            }
        }

#if defined(UTF_INTERNAL_DEBUG)
        /* for internal debug */
        draw_barrier_network(settings, my_index);
#endif /* UTF_INTERNAL_DEBUG */

        /* Call utofu_set_vbg(). */
        /* Set the return code of the uTofu function as the return code of
           this function. */
        rc = utofu_set_vbg(settings, utf_bg_detail_info->num_vbg);
        if (rc != UTF_SUCCESS) {
            /* Cannot continue the processing.
               Only post-processing is performed. */
            DEBUG(DLEVEL_PROTO_VBG) {
                utf_printf("%s: Call utofu_set_vbg: rc=%d\n", __func__, rc);
            }
            goto init_end;
        }

    } /* processing for barrier communication */

    /* Shared memory processing.
       In case of hard barrier, or in case of soft barrier and there are no
       multiple processes in a node, shared memory is used only to store the
       next TNI ID.
       Othrewise, shared memory is used to store some information needed by
       soft barrier. */
#if defined(DEBUGLOG)
    fprintf(stderr,
            "%s: Shared memory processing: my_index=%zu state=% ld first_call=%p/%p "
            "is_tofu=%u mmaparea=%p\n",
            __func__, my_index, (long)utf_bg_detail_info->intra_node_info->state,
            utf_bg_detail_info_first_alloc, utf_bg_detail_info, 
            utf_bg_intra_node_barrier_is_tofu, utf_bg_mmap_file_info.mmaparea);
#endif /* DEBUGLOG */

    /* The manager process in a node sets the next TNI ID. */
    if (UTF_BG_INTRA_NODE_MANAGER ==
            utf_bg_detail_info->intra_node_info->state) {
        utf_bg_mmap_file_info.mmaparea->next_tni_id = utf_bg_next_tni_id;
        msync((void *)utf_bg_mmap_file_info.mmaparea,
              (size_t)UTF_BG_MMAP_HEADER_SIZE, MS_SYNC);
#if defined(DEBUGLOG)
        fprintf(stderr, "%s: my_index=%zu next_tni_id=%u\n", __func__,
                my_index, utf_bg_mmap_file_info.mmaparea->next_tni_id);
#endif /* DEBUGLOG */
    }

    /* In case of soft barrier and there are multiple processes in a node,
       calculate the buffer size used by the process and save the address. */
    if (!utf_bg_intra_node_barrier_is_tofu &&
        2 <= utf_bg_detail_info->intra_node_info->size) {
        char *target_tni;
        
        /* Get the index for manager's TNI/BG */
        for (i=0UL; i<UTF_BG_MAX_PROC_IN_NODE; i++) {
            if (rankset[my_index] == utf_bg_alloc_intra_world_indexes[i]) {
                break;
            }
        }
        if (i >= UTF_BG_MAX_PROC_IN_NODE) {
            DEBUG(DLEVEL_PROTO_VBG) {
                utf_printf("%s: index could not found: my_index=%zu\n", __func__, my_index);
            }
            return UTF_ERR_INTERNAL;
        }

        /* Get the target TNI buffer address. */
        target_tni = (char *)(utf_bg_mmap_file_info.mmaparea) +
                     UTF_BG_MMAP_HEADER_SIZE +
                     (UTF_BG_MMAP_TNI_INF_SIZE *
                      utf_bg_mmap_file_info.mmaparea->manager_info[i].tni);
        /* Get and save the target gate buffer address. */
        utf_bg_detail_info->intra_node_info->mmap_buf = (uint64_t *)
           (target_tni + UTF_BG_MMAP_GATE_INF_SIZE *
                         utf_bg_mmap_file_info.mmaparea->manager_info[i].start_bg);

#if defined(DEBUGLOG)
        fprintf(stderr, 
            "%s: my_index=%zu manager_index=%zu manager_tni_id=%u manager_start_bg_id=%u mmap_buf=%p(%+ld)\n",
            __func__, my_index,
            i, 
            utf_bg_mmap_file_info.mmaparea->manager_info[i].tni,
            utf_bg_mmap_file_info.mmaparea->manager_info[i].start_bg,
            utf_bg_detail_info->intra_node_info->mmap_buf,
            ((char *)utf_bg_detail_info->intra_node_info->mmap_buf -
             (char *)utf_bg_mmap_file_info.mmaparea));
#endif /* DEBUGLOG */
    }

    if (utf_bg_detail_info_first_alloc == utf_bg_detail_info) {
        /* Allocate the temporary data buffer for reduction operations. */
        /*  need size = data size(8) * max element number(6) * in/out(2) = 96 */
        /* (Allocate lager than need size.) */
        if (0 != (wrc = posix_memalign(&(poll_info.utf_bg_poll_idata),
                                       UTF_BG_CACHE_LINE_SIZE, UTF_BG_CACHE_LINE_SIZE*2))) {
            utf_printf("%s: posix_memalign() FAILED: my_index=%zu wrc=%d\n",
                       __func__, my_index, wrc);
            if (utf_bg_mmap_file_info.file_name) {
                close(utf_bg_mmap_file_info.fd);
                free(utf_bg_mmap_file_info.file_name);
                utf_bg_mmap_file_info.file_name = NULL;
            }
            return (wrc==ENOMEM ? UTF_ERR_OUT_OF_MEMORY:UTF_ERR_INTERNAL);
        }
        poll_info.utf_bg_poll_odata =
            (char *)poll_info.utf_bg_poll_idata + UTF_BG_CACHE_LINE_SIZE;
#if defined(DEBUGLOG)
        fprintf(stderr, "%s: my_index=%zu poll_idata=%p, poll_odata=%p\n",
                __func__, my_index, poll_info.utf_bg_poll_idata, poll_info.utf_bg_poll_odata);
#endif /* DEBUGLOG */
    }

    /* Set the group_struct. */
    *group_struct = (utf_coll_group_t)utf_bg_detail_info;

init_end:
    DEBUG(DLEVEL_PROTO_VBG) {
        utf_printf("%s: my_index=%zu rc=%d, *group_struct=%p\n",
                   __func__, my_index, rc, *group_struct);
    }

    /* Post-processiong */
    /* Freeing and initialize the memories allocated by utf_bg_alloc() */
    utf_bg_detail_info->arg_info.rankset = NULL;
    utf_bg_detail_info->arg_info.len = 0;
    if (NULL != utf_bg_detail_info->vbg_settings) {
        free(utf_bg_detail_info->vbg_settings);
        utf_bg_detail_info->vbg_settings = NULL;
    }
    if (NULL != utf_bg_detail_info->rmt_src_dst_seqs) {
        free(utf_bg_detail_info->rmt_src_dst_seqs);
        utf_bg_detail_info->rmt_src_dst_seqs = NULL;
    }
    /* Initialize the unnecessary variable. */
    utf_bg_detail_info = NULL;

   return rc;
}
