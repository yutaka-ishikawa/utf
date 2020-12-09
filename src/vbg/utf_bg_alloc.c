/*
 * Copyright (C) 2020 RIKEN, Japan. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

/*
 * @file   utf_bg_alloc.c
 * @brief  Implementation of utf_bg_alloc()
 */

/*
 * Includes of header files
 */
#include "utf_bg_alloc.h"

/*
 * Declares variables
 */
static bool utf_bg_first_call=true;
            /* Whether it is the first call to this function.
             * Expects the first call to be to MPI_COMM_WORLD.
             * The first call does the following:
             * - Creates mmap buffer
             * - Saves the start gate of the barrier network
             *   for MPI_COMM_WORLD
             */
bool utf_bg_intra_node_barrier_is_tofu=true;
            /* Whether the barrier in the node is Tofu barrier or not
             * true :  Tofu barrier
             * false : Software barrier using shared memory
             */
utf_coll_group_detail_t *utf_bg_detail_info;
            /* Information for one barrier network handing between functions */
utf_coll_group_detail_t *utf_bg_detail_info_first_alloc;
            /* Information about the barrier network created
             * by the first utf_bg_alloc function call
             */
static int my_jobid=(-1);                      /* Own job ID */
static char my_hostname[64];
static int ppn_job;

utf_bg_mmap_file_info_t utf_bg_mmap_file_info; /* Information about a mmap file */

static unsigned long int utf_bg_alloc_flags = 0UL;
static int max_intra_node_procs_for_hb = UTF_BG_ALLOC_MAX_PROC_IN_NODE_FOR_HB;
utofu_tni_id_t utf_bg_next_tni_id;
uint64_t utf_bg_alloc_count;
            /* The number of times the utf_bg_alloc function was called, or
             * the number of times the utf_bg_alloc function returned in UTF_SUCCESS
             */

size_t *utf_bg_alloc_intra_world_indexes=NULL; /* vpid list in my node */

#if defined(DEBUGLOG)
static size_t my_rankset_index; /* Own process's rank */
static int xonlyflag = 0;
#endif
#if defined(UTF_BG_UNIT_TEST)
/*
 * Temporary macros for debugging
 * For testing until it will merge with other UTF functions
 */
int mypid;
int utf_dflag;
#endif

/*
 * Internal functions
 * Definitions of internal macros
 */

/*
 * Creates the following
 * - Data structures for each axis
 * - Information about the processes in the node
 */

/* Gets the node ID from rankset */
#define GET_NODEID_FROM_RANKSET(nodeid,ranksetposition)               \
{                                                                     \
    nodeid=0U;                                                        \
    union jtofu_phys_coords *coords =                                 \
        &((utf_info.vname + ranksetposition)->xyzabc);                \
    nodeid = ((uint32_t)coords->s.c << UTF_BG_ALLOC_BITSFT_COORD_C) | \
             ((uint32_t)coords->s.b << UTF_BG_ALLOC_BITSFT_COORD_B) | \
             ((uint32_t)coords->s.a << UTF_BG_ALLOC_BITSFT_COORD_A) | \
             ((uint32_t)coords->s.z << UTF_BG_ALLOC_BITSFT_COORD_Z) | \
             ((uint32_t)coords->s.y << UTF_BG_ALLOC_BITSFT_COORD_Y) | \
             ((uint32_t)coords->s.x << UTF_BG_ALLOC_BITSFT_COORD_X);  \
}
/* Gets the node ID from VBG ID */
#define GET_NODEID_FROM_VCQ(nodeid,ranksetposition)                  \
{                                                                    \
    nodeid = (uint32_t)( (ranksetposition & UTF_BG_ALLOC_MSK_6DADDR) \
                                 >> UTF_BG_BITSFT_BGINFO_COORD_X );  \
}



/* Compares own logical 3D address to peer's logical 3D address */
#define COMPARE_LOGADDR(my,temp) \
{ \
    if (my.s.z == temp.s.z) {                                      \
        if (my.s.y == temp.s.y) {                                  \
            if (my.s.x == temp.s.x) {                              \
                /* Detects the same node as own process */         \
                nodeinfo[i].axis_kind = UTF_BG_ALLOC_AXIS_IN_NODE; \
            }                                                      \
            else {                                                 \
                /* On the same X axis */                           \
                nodeinfo[i].axis_kind = UTF_BG_ALLOC_AXIS_ON_X;    \
            }                                                      \
        }                                                          \
        else if (my.s.x == temp.s.x) {                             \
            /* On the same Y axis */                               \
            nodeinfo[i].axis_kind = UTF_BG_ALLOC_AXIS_ON_Y;        \
        }                                                          \
        else {                                                     \
            /* Not on the same axis           */                   \
            /* because two axes are different */                   \
            nodeinfo[i].axis_kind = UTF_BG_ALLOC_AXIS_OFF_AXES;    \
        }                                                          \
    }                                                              \
    else if ( (my.s.y == temp.s.y) &&                              \
              (my.s.x == temp.s.x) ) {                             \
        /* On the same Y axis */                                   \
        nodeinfo[i].axis_kind = UTF_BG_ALLOC_AXIS_ON_Z;            \
    }                                                              \
    else {                                                         \
        /* Not on the same axis */                                 \
        nodeinfo[i].axis_kind = UTF_BG_ALLOC_AXIS_OFF_AXES;        \
    }                                                              \
    /* Records maximum and minimum values */                       \
    /* of logical 3D coordinates */                                \
    /* (i is reusable) */                                          \
    for (i=(size_t)UTF_BG_ALLOC_AXIS_ON_X;                         \
         i<=(size_t)UTF_BG_ALLOC_AXIS_ON_Z;i++) {                  \
        /* a[0->X,1->Y,2->Z] */                                    \
        if ((axisptr + i)->biggest  < temp.a[i-1]) {               \
            (axisptr + i)->biggest = temp.a[i-1];                  \
        }                                                          \
        else if (temp.a[i-1] < (axisptr + i)->smallest ) {         \
            (axisptr + i)->smallest = temp.a[i-1];                 \
        }                                                          \
    }                                                              \
}

/* Freeing temporary data buffers */
#define FREEAREA_AXES()                                     \
{                                                           \
    if (my_node_indexes != NULL) { free(my_node_indexes); } \
    if (nodeinfo != NULL)        { free(nodeinfo); }        \
    if (procinfo != NULL)        { free(procinfo); }        \
}

/*
 * Sorts nodes, which are elements of the array for each axis,
 * in physical 6D coordinate order
 *
 * node_a (IN) Information about node_a
 * node_b (IN) Information about node_a
 */
static int sort_nodeinfo_by_phy_6d_addr(const void *node_a,const void *node_b)
{
    utf_bg_alloc_axis_node_info_t *na_ptr =
        (utf_bg_alloc_axis_node_info_t *)node_a;
    utf_bg_alloc_axis_node_info_t *nb_ptr =
        (utf_bg_alloc_axis_node_info_t *)node_b;

    /* acb -> xyz */

    if (na_ptr->phy_6d_addr.s.z != nb_ptr->phy_6d_addr.s.z){
        return (na_ptr->phy_6d_addr.s.z - nb_ptr->phy_6d_addr.s.z);
    }
    if (na_ptr->phy_6d_addr.s.y != nb_ptr->phy_6d_addr.s.y){
        return (na_ptr->phy_6d_addr.s.y - nb_ptr->phy_6d_addr.s.y);
    }
    if (na_ptr->phy_6d_addr.s.x != nb_ptr->phy_6d_addr.s.x){
        return (na_ptr->phy_6d_addr.s.x - nb_ptr->phy_6d_addr.s.x);
    }
    if (na_ptr->phy_6d_addr.s.b != nb_ptr->phy_6d_addr.s.b){
        return (na_ptr->phy_6d_addr.s.b - nb_ptr->phy_6d_addr.s.b);
    }
    if (na_ptr->phy_6d_addr.s.c != nb_ptr->phy_6d_addr.s.c){
        return (na_ptr->phy_6d_addr.s.c - nb_ptr->phy_6d_addr.s.c);
    }
    if (na_ptr->phy_6d_addr.s.a != nb_ptr->phy_6d_addr.s.a){
        return (na_ptr->phy_6d_addr.s.a - nb_ptr->phy_6d_addr.s.a);
    }

    return 0;
}

/*
 * Gets the number of nodes, the number of processes in the node, and
 * the position of the own process in the node.
 *
 * rankset      (IN)  An array of ranks in MPI_COMM_WORLD for the processes
 *                    belonging to the barrier network to be created
 * len          (IN)  Total number of the processes
 * my_index     (IN)  Rank in the barrier network
 * my_nodeid    (IN)  Node ID where the own process is located
 * oneppn       (OUT) Whether the number of processes in the node is 1
 * num_nodes_job(OUT) Number of nodes
 * target_index (OUT) Index of own process in the node
 */
static int check_ppn_make_nodenum_make_target_index(
                                                    uint32_t *rankset,
                                                    size_t len,
                                                    size_t my_index,
                                                    uint32_t my_nodeid,
                                                    bool   *oneppn,
                                                    size_t *num_nodes_job,
                                                    size_t *target_index)
{

    if (2 <= ppn_job) {
        /*
         * Gets the position of own process in the node.
         * Calculates to be the index of the array of processes in the node.
         *
         * rankset
         * +-----------------------------------+
         * | a| a| b| b| c| c| d| d| e| e| f| f|
         * +-----------------------------------+
         *   0  1  2  3  4  5  6  7  8  9 10 11
         *   0  1  0  1  0  1  0  1  0  1  0  1 ->target_index
         * <--------------- len --------------->
         *  If my_index is 5, the indexes of the processes communicating
         *  between the nodes are 1,3,5 (self), 7,9,11 (target_index is 1).
         *
         */
        /*
         * Gets the number of nodes.
         * rankset
         * +-----------------------------------+
         * | a| a| b| b| b| c| c| c| a| d| d| b|
         * +-----------------------------------+
         * <--------------- len --------------->
         *
         * save_nodeids
         * +-----------------------------------+
         * | a| b| c| d|-1|-1|-1|-1|-1|-1|-1|-1|
         * +-----------------------------------+
         * <- number -->
         *   of nodes
         * <--------------- len --------------->
         *
         * 160,000 x 4 x sizeof(uint32_t) = 2,560,000 -> 2.5MB
         */
        uint32_t *save_nodeids;
        size_t s,i;
        bool found_index;
        uint32_t temp_nodeid;

        save_nodeids = (uint32_t *)malloc(sizeof(uint32_t) * len);
        if (NULL == save_nodeids) {
            return UTF_ERR_OUT_OF_MEMORY;
        }
        memset((void *)save_nodeids,UTF_BG_ALLOC_INVALID_NODEID,
            sizeof(uint32_t) * len);
        *oneppn = false;
        *num_nodes_job = 0UL;
        *target_index = 0UL;
        for (found_index=false,s=0;s<len;s++) {
            GET_NODEID_FROM_RANKSET(temp_nodeid,rankset[s]);
            if ( (!found_index) && (temp_nodeid == my_nodeid) ) {
                if (my_index == s) {
                    found_index=true;
                }
                else {
                    (*target_index)++;
                }
            }
            for (i=0;i<*num_nodes_job;i++) {
                if (save_nodeids[i] == temp_nodeid) { break; }
            }
            if (i == *num_nodes_job) {
                /* It's a new node */
                save_nodeids[*num_nodes_job] = temp_nodeid;
                (*num_nodes_job)++;
            }
        }
        free(save_nodeids);
    }
    else {
        *oneppn = true;
        *num_nodes_job = len;
        *target_index=0;
    }
    return UTF_SUCCESS;
}

/* Gets the 6D physical coordinates from the nodeid */
#define GET_PHY_6D_ADDR_FROM_NODEID(nodeid,addr)                               \
{                                                                              \
    addr.s.x = (uint8_t)                                                       \
        ((nodeid & UTF_BG_ALLOC_MSK_6DADDR_X) >> UTF_BG_ALLOC_BITSFT_COORD_X); \
    addr.s.y = (uint8_t)                                                       \
        ((nodeid & UTF_BG_ALLOC_MSK_6DADDR_Y) >> UTF_BG_ALLOC_BITSFT_COORD_Y); \
    addr.s.z = (uint8_t)                                                       \
        ((nodeid & UTF_BG_ALLOC_MSK_6DADDR_Z) >> UTF_BG_ALLOC_BITSFT_COORD_Z); \
    addr.s.a = (uint8_t)                                                       \
        ((nodeid & UTF_BG_ALLOC_MSK_6DADDR_A) >> UTF_BG_ALLOC_BITSFT_COORD_A); \
    addr.s.b = (uint8_t)                                                       \
        ((nodeid & UTF_BG_ALLOC_MSK_6DADDR_B) >> UTF_BG_ALLOC_BITSFT_COORD_B); \
    addr.s.c = (uint8_t)                                                       \
        ((nodeid & UTF_BG_ALLOC_MSK_6DADDR_C) >> UTF_BG_ALLOC_BITSFT_COORD_C); \
}

/*
 * Creates node information belonging to each axis
 * which is in "utf_bf_alloc_proc_info_t"
 *
 *   (IN)  rankset An array of ranks in MPI_COMM_WORLD for the processes
 *                 belonging to the barrier network to be created
 *   (OUT) dst  A pointer to node information belonging to each axis
 *   (IN)  src  A pointer to the array of each process information
 */
static void set_axis_node_info(
                               uint32_t *rankset,
                               utf_bg_alloc_axis_node_info_t *dst,
                               utf_bf_alloc_proc_info_t      *src)
{

    dst->index = src->index;
    dst->log_3d_addr.s.x = utf_info.vname[rankset[dst->index]].xyz.s.x;
    dst->log_3d_addr.s.y = utf_info.vname[rankset[dst->index]].xyz.s.y;
    dst->log_3d_addr.s.z = utf_info.vname[rankset[dst->index]].xyz.s.z;
    dst->phy_6d_addr.s.x = utf_info.vname[rankset[dst->index]].xyzabc.s.x;
    dst->phy_6d_addr.s.y = utf_info.vname[rankset[dst->index]].xyzabc.s.y;
    dst->phy_6d_addr.s.z = utf_info.vname[rankset[dst->index]].xyzabc.s.z;
    dst->phy_6d_addr.s.a = utf_info.vname[rankset[dst->index]].xyzabc.s.a;
    dst->phy_6d_addr.s.b = utf_info.vname[rankset[dst->index]].xyzabc.s.b;
    dst->phy_6d_addr.s.c = utf_info.vname[rankset[dst->index]].xyzabc.s.c;

    return;
}

/*
 * Creates information for each axis
 *
 * rankset      (IN)  An array of ranks in MPI_COMM_WORLD for the processes
 *                    belonging to the barrier network to be created
 *   (IN)     len       Total number of the processes.
 *   (IN)     my_index  Rank in the barrier network
 *   (IN)     max_ppn   Maximum number of processes in the node.
 *   (OUT)    axisptr   A pointer to an array of axis information.
 * intrainfo    (OUT) Pointer to process information in the node
 * errcase      (OUT) Error type
 *
 */
int make_axes_and_intra_node_infos(
                                   uint32_t       *rankset,
                                   size_t len,
                                   size_t my_index,
                                   int max_ppn,
                                   utf_bg_alloc_axis_detail_info_t *axisptr,
                                   utf_bg_intra_node_detail_info_t **intrainfo,
                                   uint8_t *errcase)
{
    int rc = UTF_SUCCESS;
    bool found,toomany,unmatch,oneppn=false,hexa;
    size_t i,s,target_index=0;
    size_t num_nodes_job=0,num_nodes=0,my_nodeinfo_index=0,procinfo_size,end_axis;
    size_t *my_node_indexes=NULL;
    uint32_t my_nodeid,nodeid;
    utf_bf_alloc_node_info_t *nodeinfo=NULL;
    utf_bf_alloc_proc_info_t *procinfo=NULL;
    utf_bg_alloc_axis_node_info_t *ninfoptrs[UTF_BG_ALLOC_AXIS_NUM_AXES];
#if defined(DEBUGLOG)
    size_t ninfoptrs_cnt[UTF_BG_ALLOC_AXIS_NUM_AXES];
#endif
    utf_bg_intra_node_detail_info_t *intraptr;

    /* Gets the node ID and logical 3D address of own process */
    GET_NODEID_FROM_RANKSET(my_nodeid,rankset[my_index]);

    /*
     * Calculates number of the nodes.
     * Creates information for each node.
     */

    /* Gets the maximum number of processes in a node */
    toomany = false;
    if (utf_bg_first_call) {
        ppn_job = max_ppn;
        utf_bg_alloc_intra_world_indexes =
            (size_t *)malloc(sizeof(size_t) * UTF_BG_MAX_PROC_IN_NODE);
        if (NULL == utf_bg_alloc_intra_world_indexes) {
             *intrainfo = NULL;
             FREEAREA_AXES();
             return UTF_ERR_OUT_OF_MEMORY;
        }
        for (i=0UL;i<UTF_BG_MAX_PROC_IN_NODE;i++) {
            utf_bg_alloc_intra_world_indexes[i] = UTF_BG_ALLOC_INVALID_INDEX;
        }
#if defined(DEBUGLOG)
        fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu: utf_bg_first_call=%d [UTF_BG_UNDER_MPICH_TOFU ver8.0] rankset is VPID version. ppn_job=%d \n"
                ,my_hostname,__func__,my_index
                ,utf_bg_first_call,ppn_job
        );
#endif
    }
    rc = check_ppn_make_nodenum_make_target_index(rankset,
                                                  len,
                                                  my_index,
                                                  my_nodeid,
                                                  &oneppn,
                                                  &num_nodes_job,
                                                  &target_index);
#if defined(DEBUGLOG)
    fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu: check_ppn_make_nodenum_make_target_index=%d oneppn=%d num_nodes_job=%zu target_index=%zu \n"
        ,my_hostname,__func__,my_index
        ,rc,oneppn,num_nodes_job,target_index
    );
#endif
    if (UTF_SUCCESS != rc) {
        *intrainfo = NULL;
        FREEAREA_AXES();
        return rc;
    }

    /* Indexes of processes on the same node as own process in rankset */
    my_node_indexes = (size_t *)malloc(sizeof(size_t) * ppn_job);
    if (NULL == my_node_indexes) {
        *intrainfo = NULL;
        FREEAREA_AXES();
        return UTF_ERR_OUT_OF_MEMORY;
    }
    for (i=0UL;i<(size_t)ppn_job;i++) {
        my_node_indexes[i] = UTF_BG_ALLOC_INVALID_INDEX;
    }

    nodeinfo = (utf_bf_alloc_node_info_t *)malloc(
        sizeof(utf_bf_alloc_node_info_t) * num_nodes_job);
    if (NULL == nodeinfo) {
        *intrainfo = NULL;
        FREEAREA_AXES();
        return UTF_ERR_OUT_OF_MEMORY;
    }
    memset((void *)nodeinfo,0,sizeof(utf_bf_alloc_node_info_t) * num_nodes_job);

    (axisptr + 0)->biggest  = 0UL;
    (axisptr + 0)->smallest = 0UL;
    for (i=(size_t)UTF_BG_ALLOC_AXIS_ON_X;
         i<=(size_t)UTF_BG_ALLOC_AXIS_ON_Z;i++) {
        /* coords[0->X,1->Y,2->Z] */
        (axisptr + i)->biggest  = utf_info.vname[rankset[my_index]].xyz.a[i-1];
        (axisptr + i)->smallest = utf_info.vname[rankset[my_index]].xyz.a[i-1];
    }

    for (s=0UL;s<len;s++) {
        GET_NODEID_FROM_RANKSET(nodeid,rankset[s]);
        for (found=false,i=0UL;i<num_nodes;i++) {
            if (nodeinfo[i].nodeid == nodeid) {
                found = true;
                break;
            }
        }
        assert(i < num_nodes_job);
        if (found) {
            /* Nodes are the same. */
            if ((size_t)UTF_BG_MAX_PROC_IN_NODE < nodeinfo[i].size) {
                /* 49 or more was specified for the number of processes in the node */
                *intrainfo = NULL;
                FREEAREA_AXES();
                return UTF_ERR_INTERNAL;
            }
            /* Whether the node ID is the same as that of own process */
            if (my_nodeid == nodeid) {
                assert(nodeinfo[i].size < ppn_job);
                my_node_indexes[nodeinfo[i].size] = s;
                if (utf_bg_first_call) {
                    utf_bg_alloc_intra_world_indexes[nodeinfo[i].size] =
                        rankset[s];
                }
            }
            /* Updates index of leader process in the node */
            if (nodeinfo[i].size == target_index) {
                nodeinfo[i].manager_index = s;
            }
            /* Updates the number of processes in the node */
            nodeinfo[i].size++;
            if ((size_t)max_intra_node_procs_for_hb <
                nodeinfo[i].size) {
                /* Exceeded the maximum number of processes in a node. */
                toomany = true;
            }
        }
        else {
            /* Node that have not appeared so far */
            nodeinfo[i].nodeid = nodeid;
            /* Whether the node ID is the same as that of own process */
            if (my_nodeid == nodeid) {
                my_nodeinfo_index = num_nodes;
                assert(nodeinfo[i].size < ppn_job);
                my_node_indexes[nodeinfo[i].size] = s;
                if (utf_bg_first_call) {
                    utf_bg_alloc_intra_world_indexes[nodeinfo[i].size] =
                        rankset[s];
                }
            }
            /* Updates index of leader process in the node */
            nodeinfo[i].manager_index = s;
            /* Updates the number of processes in the node */
            nodeinfo[i].size++;
            COMPARE_LOGADDR(utf_info.vname[rankset[my_index]].xyz,
                            utf_info.vname[rankset[s]].xyz);
            num_nodes++;
            assert(num_nodes <= num_nodes_job);
        }
    }

#if defined(DEBUGLOG)
    for (i=0UL;i<(size_t)ppn_job;i++) {
        fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu:MYNODEINFO %zu/%zu my_node_indexes=%zu utf_bg_alloc_intra_world_indexes=%zu \n"
            ,my_hostname,__func__,my_index
            ,i,(size_t)ppn_job,my_node_indexes[i]
            ,utf_bg_alloc_intra_world_indexes[i]
        );
    }
    for (i=0UL;i<num_nodes;i++) {
        fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu:NODEINFO %zu/%zu toomany=%d nodeid=%x size=%d axis_kind=%d manager_index=%zu log3daddr=%u-%u-%u \n"
            ,my_hostname,__func__,my_index
            ,i,num_nodes,toomany
            ,nodeinfo[i].nodeid,nodeinfo[i].size,nodeinfo[i].axis_kind
            ,nodeinfo[i].manager_index
            ,utf_info.vname[rankset[nodeinfo[i].manager_index]].xyz.s.x
            ,utf_info.vname[rankset[nodeinfo[i].manager_index]].xyz.s.y
            ,utf_info.vname[rankset[nodeinfo[i].manager_index]].xyz.s.z
        );
    }
#endif

    /*
     * Creates information for each process.
     */
    intraptr = (utf_bg_intra_node_detail_info_t *)malloc(
       sizeof(utf_bg_intra_node_detail_info_t));
    if (NULL == intraptr) {
        *intrainfo = NULL;
        FREEAREA_AXES();
        return UTF_ERR_OUT_OF_MEMORY;
    }
    memset((void *)intraptr,0,sizeof(utf_bg_intra_node_detail_info_t));

    procinfo = (utf_bf_alloc_proc_info_t *)malloc(
        sizeof(utf_bf_alloc_proc_info_t) * len);
    if (NULL == procinfo) {
        free(intraptr);
        *intrainfo = NULL;
        FREEAREA_AXES();
        return UTF_ERR_OUT_OF_MEMORY;
    }
    memset((void *)procinfo,0,sizeof(utf_bf_alloc_proc_info_t) * len);
    procinfo_size = 0UL;

    /* Creates UTF_BG_ALLOC_AXIS_IN_NODE and UTF_BG_ALLOC_AXIS_MYSELF */
    /* Creates process information in the node */
    intraptr->size = (size_t)nodeinfo[my_nodeinfo_index].size;
    intraptr->vpids = (size_t *)malloc(sizeof(size_t) * intraptr->size);
    if (NULL == intraptr->vpids) {
        free(intraptr);
        *intrainfo = NULL;
        FREEAREA_AXES();
        return UTF_ERR_OUT_OF_MEMORY;
    }
    for (i=0UL;i<(size_t)intraptr->size;i++) {
        *((intraptr->vpids) + i) = UTF_BG_ALLOC_INVALID_INDEX;
    }
    for (s=0UL;s<nodeinfo[my_nodeinfo_index].size;s++) {
        (procinfo + procinfo_size)->nodeid =
            nodeinfo[my_nodeinfo_index].nodeid;
        (procinfo + procinfo_size)->axis_kind =
            nodeinfo[my_nodeinfo_index].axis_kind;
        if (my_index == my_node_indexes[s]) {
            (procinfo + procinfo_size)->axis_kind = UTF_BG_ALLOC_AXIS_MYSELF;
            intraptr->state = (0 == s) ?
                 UTF_BG_INTRA_NODE_MANAGER : UTF_BG_INTRA_NODE_WORKER;
            intraptr->intra_index = s;
        }
        (procinfo + procinfo_size)->index = my_node_indexes[s];
        *((intraptr->vpids) + s) = rankset[my_node_indexes[s]];
        procinfo_size++;
        assert(procinfo_size <= len);
    }

#if defined(DEBUGLOG)
    for (i=0UL;i<(size_t)intraptr->size;i++) {
        fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu:MYNODEINFO %zu/%zu my_node_indexes=%zu vpid=%zu \n"
            ,my_hostname,__func__,my_index
            ,i,(size_t)ppn_job,my_node_indexes[i],*((intraptr->vpids) + i)
        );
    }
#endif

    /* Creates UTF_BG_ALLOC_AXIS_ON_X, UTF_BG_ALLOC_AXIS_ON_Y,
     * UTF_BG_ALLOC_AXIS_ON_Z and UTF_BG_ALLOC_AXIS_NUM_AXES */

    unmatch = false;
    for (i=0UL;i<num_nodes;i++) {
        if (0 == nodeinfo[i].size) { continue; }
        if ( (utf_bg_intra_node_barrier_is_tofu) &&
             (i) &&
             (nodeinfo[i].size != nodeinfo[i-1].size) ) {
            unmatch=true;
            break;
        }
        if (i == my_nodeinfo_index) {
            assert( nodeinfo[i].nodeid == my_nodeid);
            assert(nodeinfo[i].axis_kind == UTF_BG_ALLOC_AXIS_IN_NODE);
            continue;
        }

        /* In this line, the number of processes in the node must match. */
        (procinfo + procinfo_size)->nodeid      = nodeinfo[i].nodeid;
        (procinfo + procinfo_size)->axis_kind   = nodeinfo[i].axis_kind;
        (procinfo + procinfo_size)->index = nodeinfo[i].manager_index;
        procinfo_size++;
        assert(procinfo_size <= len);
    }

#if defined(DEBUGLOG)
    for (i=0UL;i<procinfo_size;i++) {
        fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu:PROCINFO %zu/%zu unmatch=%d nodeid=%x axis_kind=%d index=%zu log3daddr=%u-%u-%u \n"
            ,my_hostname,__func__,my_index
            ,i,procinfo_size,unmatch
            ,procinfo[i].nodeid,procinfo[i].axis_kind
            ,procinfo[i].index
            ,utf_info.vname[rankset[procinfo[i].index]].xyz.s.x
            ,utf_info.vname[rankset[procinfo[i].index]].xyz.s.y
            ,utf_info.vname[rankset[procinfo[i].index]].xyz.s.z
        );
    }
#endif

    if (unmatch) {
        *errcase |= UTF_BG_ALLOC_UNMATCH_PROC;
        rc = UTF_BG_ALLOC_ERR_UNMATCH_PROC_PER_NODE;
    }
    if ( (!utf_bg_intra_node_barrier_is_tofu) &&
         (num_nodes < (size_t)UTF_BG_ALLOC_REQUIRED_NODE) ) {
        /* Not enough nodes for shared memory communication in the node. */
        *errcase |= UTF_BG_ALLOC_TOO_FEW_NODE;
        rc = UTF_BG_ALLOC_ERR_TOO_FEW_NODE;
    }
    if ((toomany) && (utf_bg_intra_node_barrier_is_tofu)) {
        /* Too many processes in the node */
        *errcase |= UTF_BG_ALLOC_TOO_MANY_PROC;
        rc = UTF_BG_ALLOC_ERR_TOO_MANY_PROC_IN_NODE;
    }
    if (UTF_SUCCESS != rc) {
        FREEAREA_AXES(); 
        *intrainfo = intraptr; 
        return rc;
    }

    /* Gets the length of each axis */
    for (i=(size_t)UTF_BG_ALLOC_AXIS_ON_X;
         i<=(size_t)UTF_BG_ALLOC_AXIS_ON_Z;i++) {
        (axisptr + i)->size =
            (size_t)((axisptr + i)->biggest - (axisptr + i)->smallest + 1);
    }
    (axisptr + UTF_BG_ALLOC_AXIS_IN_NODE)->size = intraptr->size;
#if defined(DEBUGLOG)
    fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu:INTRAINFO ptr=%p state=%ld size=%zu intra_index=%zu log3daddr=%u-%u-%u \n"
        ,my_hostname,__func__,my_index
        ,intraptr,intraptr->state,intraptr->size,intraptr->intra_index
        ,utf_info.vname[rankset[my_index]].xyz.s.x
        ,utf_info.vname[rankset[my_index]].xyz.s.y
        ,utf_info.vname[rankset[my_index]].xyz.s.z
    );
#endif
    for (i=(size_t)UTF_BG_ALLOC_AXIS_IN_NODE;
         i<(size_t)UTF_BG_ALLOC_AXIS_NUM_AXES;i++) {
#if defined(DEBUGLOG)
        fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu:AXISINFO %zu biggest=%u smallest=%u size=%zu \n"
            ,my_hostname,__func__,my_index
            ,i,(axisptr + i)->biggest,(axisptr + i)->smallest
            ,(axisptr + i)->size
        );
        ninfoptrs_cnt[i]=0UL;
#endif
        ninfoptrs[i]=NULL;
    }

    if ( !utf_bg_intra_node_barrier_is_tofu &&
         (UTF_BG_INTRA_NODE_WORKER == intraptr->state) ) {
        FREEAREA_AXES();
        *intrainfo = intraptr;
        return UTF_SUCCESS;
    }

    /*
     * Creates node information belonging to each axis
     */
    if (num_nodes == (axisptr + UTF_BG_ALLOC_AXIS_ON_X)->size *
                     (axisptr + UTF_BG_ALLOC_AXIS_ON_Y)->size *
                     (axisptr + UTF_BG_ALLOC_AXIS_ON_Z)->size) {
        /* The shape consisting of all nodes is a hexahedron */
        end_axis = (size_t)UTF_BG_ALLOC_AXIS_NUM_AXES;
        hexa = true;
    }
    else {
        end_axis = (size_t)UTF_BG_ALLOC_AXIS_ON_Y;
        hexa = false;
    }

#if defined(DEBUGLOG)
   if (xonlyflag) {
        end_axis = (size_t)UTF_BG_ALLOC_AXIS_ON_Y;
        hexa = false;
        fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu: FORCE NOT HEXA!!! \n"
            ,my_hostname,__func__,my_index
        );
   }
#endif

    if (!hexa) {
        /* Since the topology shape of all nodes does not match a hexahedron,
         * it is considered to be on one axis (X). */
#if defined(DEBUGLOG)
        fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu: ISNOT HEXA X=%zu->%zu Y=%zu->0 Z=%zu->0 \n"
            ,my_hostname,__func__,my_index
            ,(axisptr + UTF_BG_ALLOC_AXIS_ON_X)->size,num_nodes
            ,(axisptr + UTF_BG_ALLOC_AXIS_ON_Y)->size
            ,(axisptr + UTF_BG_ALLOC_AXIS_ON_Z)->size
        );
#endif
        (axisptr + UTF_BG_ALLOC_AXIS_ON_X)->size = num_nodes;
        (axisptr + UTF_BG_ALLOC_AXIS_ON_Y)->size = 0ULL;
        (axisptr + UTF_BG_ALLOC_AXIS_ON_Z)->size = 0ULL;
    }

    for (i=(size_t)UTF_BG_ALLOC_AXIS_IN_NODE;i<end_axis;i++) {
        (axisptr + i)->nodeinfo = (utf_bg_alloc_axis_node_info_t *)malloc(
            sizeof(utf_bg_alloc_axis_node_info_t) * (axisptr + i)->size);
        if (NULL == (axisptr + i)->nodeinfo) {
            i--;
            do {
                free((axisptr + i)->nodeinfo);
            } while (!i);
            free(intraptr);
            *intrainfo = NULL;
            FREEAREA_AXES();
            return UTF_ERR_OUT_OF_MEMORY;
        }
        ninfoptrs[i]=(axisptr + i)->nodeinfo;
    }

    for (i=0UL;i<procinfo_size;i++) {
        s = (size_t)procinfo[i].axis_kind;
        if ( hexa && ((size_t)UTF_BG_ALLOC_AXIS_OFF_AXES == s) ) { continue; }
        if ((size_t)UTF_BG_ALLOC_AXIS_MYSELF != s) {
            if ( !hexa && (s != (size_t)UTF_BG_ALLOC_AXIS_IN_NODE) ) {
                /* Since the topology shape of all nodes does not match a hexahedron,
                 * it is considered to be on one axis (X). */
                s = (size_t)UTF_BG_ALLOC_AXIS_ON_X;
            }
            set_axis_node_info(
                               rankset,
                               ninfoptrs[s],&procinfo[i]);
            ninfoptrs[s]++;
#if defined(DEBUGLOG)
            ninfoptrs_cnt[s]++;
            assert(ninfoptrs_cnt[s] <= (axisptr + s)->size);
#endif
        }
        else {
            for (s=(size_t)UTF_BG_ALLOC_AXIS_IN_NODE;s<end_axis;s++) {
                set_axis_node_info(
                                   rankset,
                                   ninfoptrs[s],&procinfo[i]);
                ninfoptrs[s]++;
#if defined(DEBUGLOG)
                ninfoptrs_cnt[s]++;
                assert(ninfoptrs_cnt[s] <= (axisptr + s)->size);
#endif
            }
        }
    }

    /*
     * Sorts node information
     * Sorts for each axis (X, Y, and Z)
     */
    for (i=(size_t)UTF_BG_ALLOC_AXIS_ON_X;i<end_axis;i++) {
        qsort((axisptr + i)->nodeinfo,
              (axisptr + i)->size,
              sizeof(utf_bg_alloc_axis_node_info_t),
              sort_nodeinfo_by_phy_6d_addr);
    }

    /* After sorting, gets the position of own process from the node information
     * of each axis. */
    for (i=(size_t)UTF_BG_ALLOC_AXIS_IN_NODE;i<end_axis;i++) {
        for ((axisptr + i)->nodeinfo_index=0UL;
             (axisptr + i)->nodeinfo[(axisptr + i)->nodeinfo_index].index !=
                 my_index;
             (axisptr + i)->nodeinfo_index++) {
            if ((axisptr + i)->size <= (axisptr + i)->nodeinfo_index) {
                /* internal error */
                free(intraptr);
                *intrainfo = NULL;
                FREEAREA_AXES();
                return UTF_ERR_INTERNAL;
            }
        }
    }

#if defined(DEBUGLOG)
    for (i=(size_t)UTF_BG_ALLOC_AXIS_IN_NODE;
         i<(size_t)UTF_BG_ALLOC_AXIS_NUM_AXES;i++) {
        fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu:AXISINFO %zu size=%zu nodeinfo=%p nodeinfo_index=%zu end_axis=%zu hexa=%d \n"
            ,my_hostname,__func__,my_index
            ,i,(axisptr + i)->size,(axisptr + i)->nodeinfo
            ,(axisptr + i)->nodeinfo_index,end_axis,hexa
        );
        if ((axisptr + i)->nodeinfo) {
            utf_bg_alloc_axis_node_info_t *temp;
            for (s=0UL,temp=(axisptr + i)->nodeinfo;
                 s<(axisptr + i)->size;s++,temp++) {
                fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu:AXISINFO %zu nodeinfo=%zu/%zu index=%zu log3daddr=%u-%u-%u phy6daddr=%02u-%02u-%02u-%1u-%1u-%1u \n"
                    ,my_hostname,__func__,my_index
                    ,i,s,(axisptr + i)->size,temp->index
                    ,temp->log_3d_addr.s.x
                    ,temp->log_3d_addr.s.y
                    ,temp->log_3d_addr.s.z
                    ,temp->phy_6d_addr.s.x
                    ,temp->phy_6d_addr.s.y
                    ,temp->phy_6d_addr.s.z
                    ,temp->phy_6d_addr.s.a
                    ,temp->phy_6d_addr.s.b
                    ,temp->phy_6d_addr.s.c
                );
            }
        }
    }
#endif

    FREEAREA_AXES();
    *intrainfo = intraptr;
    return UTF_SUCCESS;
}

/*
 * Builds a butterfly network
 */

/* Frees temporary data buffers */
#define FREEAREA_BUTTERFLY()              \
{                                         \
  if (NULL != sender)  { free(sender); }  \
  if (NULL != reciver) { free(reciver); } \
}

/* Gets the peer process of own process according to the rules
 * of butterfly communication */
#define GET_PAIR_OF_POWEROFTWO(index,on,gates,i,result)           \
{                                                                 \
    result = ( (index - on) ^  (1 << (gates - i - 1)) ) + on ;    \
}

/* Get the send / receive peer process of own process
 * according to the rules of butterfly communication */
#define SET_PAIR_POWEROFTWO(on_val)                               \
for (i=0;i<axisptr->number_gates_of_poweroftwo;i++) {             \
    GET_PAIR_OF_POWEROFTWO(axisptr->nodeinfo_index,               \
                           on_val,                                \
                           axisptr->number_gates_of_poweroftwo,   \
                           i,                                     \
                           *sendtable);                           \
    *recvtable = *sendtable;                                      \
    sendtable++;                                                  \
    recvtable++;                                                  \
}

/* Sets "No operation" for send / receive sequences
 * whose butterfly network zone is "EAR" */
#define SET_NOOP_FOR_EAR()                                        \
for (i=0;i<(axisptr->number_gates_of_poweroftwo + 1);i++) {       \
    *sendtable = UTF_BG_ALLOC_INVALID_INDEX;                      \
    *recvtable = UTF_BG_ALLOC_INVALID_INDEX;                      \
    sendtable++;                                                  \
    recvtable++;                                                  \
}

#if defined(DEBUGLOG)
#define UTF_BG_ALLOC_INVALID_6DADDR ((uint8_t)0xff)
/*
 * Prints the send / receive sequences of the butterfly network
 *
 * seqptr           (IN) Pointer to the send / receive sequences
 * number_gates_max (IN) Maximum number of gates
 * print_opt        (IN) Whether to display the optimized result
 */
static void print_sequence(utf_bf_alloc_butterfly_sequence_info_t *seqptr,
                           size_t number_gates_max,
                           bool   print_opt)
{
    size_t i;
    utf_bf_alloc_butterfly_sequence_info_t *temp = seqptr;
    char recvaddr[14];
    char sendaddr[14];

    fprintf(stderr,"***************************************** \n");
    for (i=0;i<number_gates_max;i++,seqptr++) {

        if (seqptr->recvinfo.phy_6d_addr) {
            sprintf(recvaddr,"%02u-%02u-%02u-%1u-%1u-%1u"
                ,seqptr->recvinfo.phy_6d_addr->s.x
                ,seqptr->recvinfo.phy_6d_addr->s.y
                ,seqptr->recvinfo.phy_6d_addr->s.z
                ,seqptr->recvinfo.phy_6d_addr->s.a
                ,seqptr->recvinfo.phy_6d_addr->s.b
                ,seqptr->recvinfo.phy_6d_addr->s.c
            );
        }
        else { strcpy(&recvaddr[0],"NULL"); }

        if (seqptr->sendinfo.phy_6d_addr) {
            sprintf(sendaddr,"%02u-%02u-%02u-%1u-%1u-%1u"
                ,seqptr->sendinfo.phy_6d_addr->s.x
                ,seqptr->sendinfo.phy_6d_addr->s.y
                ,seqptr->sendinfo.phy_6d_addr->s.z
                ,seqptr->sendinfo.phy_6d_addr->s.a
                ,seqptr->sendinfo.phy_6d_addr->s.b
                ,seqptr->sendinfo.phy_6d_addr->s.c
            );
        }
        else { strcpy(&sendaddr[0],"NULL"); }

        fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu: [ALL]number_gates_max=%zu/%zu : seqptr=%p :vbg_id=%016lx : Recv vbg_id=%016lx nodeinfo_index=%ld index=%ld phy6daddr=%s : Send vbg_id=%016lx nodeinfo_index=%ld index=%ld phy6daddr=%s : nextptr=%p \n"
            ,my_hostname,__func__,my_rankset_index
            ,i,number_gates_max
            ,seqptr
            ,seqptr->vbg_id
            ,seqptr->recvinfo.vbg_id
            ,seqptr->recvinfo.nodeinfo_index
            ,seqptr->recvinfo.index
            ,&recvaddr[0]
            ,seqptr->sendinfo.vbg_id
            ,seqptr->sendinfo.nodeinfo_index
            ,seqptr->sendinfo.index
            ,&sendaddr[0]
            ,seqptr->nextptr
        );
    }

    if (!print_opt) { return; }

    seqptr = temp;
    i=0;
    fprintf(stderr,"***************************************** \n");
    while (seqptr) {

        if (seqptr->recvinfo.phy_6d_addr) {
            sprintf(recvaddr,"%02u-%02u-%02u-%1u-%1u-%1u"
                ,seqptr->recvinfo.phy_6d_addr->s.x
                ,seqptr->recvinfo.phy_6d_addr->s.y
                ,seqptr->recvinfo.phy_6d_addr->s.z
                ,seqptr->recvinfo.phy_6d_addr->s.a
                ,seqptr->recvinfo.phy_6d_addr->s.b
                ,seqptr->recvinfo.phy_6d_addr->s.c
            );
        }
        else { strcpy(&recvaddr[0],"NULL"); }

        if (seqptr->sendinfo.phy_6d_addr) {
            sprintf(sendaddr,"%02u-%02u-%02u-%1u-%1u-%1u"
                ,seqptr->sendinfo.phy_6d_addr->s.x
                ,seqptr->sendinfo.phy_6d_addr->s.y
                ,seqptr->sendinfo.phy_6d_addr->s.z
                ,seqptr->sendinfo.phy_6d_addr->s.a
                ,seqptr->sendinfo.phy_6d_addr->s.b
                ,seqptr->sendinfo.phy_6d_addr->s.c
            );
        }
        else { strcpy(&sendaddr[0],"NULL"); }

        fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu: [OPT]number_gates_max=%zu/%zu : seqptr=%p :vbg_id=%016lx : Recv vbg_id=%016lx nodeinfo_index=%ld index=%ld phy6daddr=%s : Send vbg_id=%016lx nodeinfo_index=%ld index=%ld phy6daddr=%s : nextptr=%p \n"
            ,my_hostname,__func__,my_rankset_index
            ,i,number_gates_max
            ,seqptr
            ,seqptr->vbg_id
            ,seqptr->recvinfo.vbg_id
            ,seqptr->recvinfo.nodeinfo_index
            ,seqptr->recvinfo.index
            ,&recvaddr[0]
            ,seqptr->sendinfo.vbg_id
            ,seqptr->sendinfo.nodeinfo_index
            ,seqptr->sendinfo.index
            ,&sendaddr[0]
            ,seqptr->nextptr
        );
        seqptr = seqptr->nextptr;
        i++;
    }

    return;
}
#endif

/*
 * Identifies the zone to which own process belongs
 *
 * nodeinfo_index (IN) Index of own process in information for each axis (nodeinfo)
 *  (IN)    axisptr    Pointer to an array of each axis information
 */
/*
 * To build a butterfly network, identifies zones by
 * whether the node in in the range of powers of 2 from the center of each axis.
 *  - UTF_BG_ALLOC_POWEROFTWO_EAR:
 *    Zone "ear" is out of the above range.
 *  - UTF_BG_ALLOC_POWEROFTWO_CHEEK:
 *    Zone "cheek" is within the above range and communicate with zone "ear".
 *  - UTF_BG_ALLOC_POWEROFTWO_FACE:
 *    Zone "face" is within the above range and does not communicate with zone "ear".
 *
 *      +----------+
 *     +            +
 *     +  ^      ^  +
 *  +--|  e      e  |--+
 *  |  |    |  |    |  |
 *  +--|     ~~     |--+
 *  a  |      b     |  c
 *   E |C  F    F  C| E
 *     |            |
 *      +   ====   +
 *       +--------+
 *
 * E → UTF_BG_ALLOC_POWEROFTWO_EAR
 * C → UTF_BG_ALLOC_POWEROFTWO_CHEEK
 * F → UTF_BG_ALLOC_POWEROFTWO_FACE
 *
 */
static int get_my_zone(utf_bg_alloc_axis_detail_info_t *axisptr,
                       size_t nodeinfo_index)
{
    if ( (nodeinfo_index < axisptr->on_poweroftwo) ||
         (axisptr->off_poweroftwo <= nodeinfo_index) ) {
        return UTF_BG_ALLOC_POWEROFTWO_EAR;
    }
    if ( ((axisptr->on_poweroftwo * 2) <=  nodeinfo_index) &&
         (nodeinfo_index <
          (axisptr->size - ((axisptr->size - axisptr->off_poweroftwo)*2)))
       ) {
        return UTF_BG_ALLOC_POWEROFTWO_FACE;
    }
    return UTF_BG_ALLOC_POWEROFTWO_CHEEK;

}

/*
 *  Optimizes the send/receive sequence.
 *
 *  seqptr           (OUT)    pointer to the send/receive sequence
 *  number_gates_max (IN/OUT) Maximum number of gates
 */
static void optimized_sequence(utf_bf_alloc_butterfly_sequence_info_t **seqptr,
                               size_t *number_gates_max)
{
    utf_bf_alloc_butterfly_sequence_info_t *curr,*next;
    size_t i;

    if ( (UTF_BG_ALLOC_INVALID_INDEX == (*seqptr)->sendinfo.nodeinfo_index) &&
         (UTF_BG_ALLOC_INVALID_INDEX == (*seqptr)->recvinfo.nodeinfo_index) &&
         (UTF_BG_ALLOC_INVALID_INDEX == (*seqptr + 1)->recvinfo.nodeinfo_index) )
    {
        /*
         *  The first step in the sequence is deleted
         *  because it does not send or receive.
         *  However, if the zone of own process is "cheek", it is not deleted
         *  because it is the start gate to the local VBG.
         */
        (*seqptr)++;
        (*number_gates_max)--;
    }

    /*
     *
     * step1
     *   Creates a list structure. Sets the value to a nextptr.
     *
     * max_gates_number = 8 (2x3x2)
     *
     *            recv         send
     *  -----------------------------------
     *  0          -1           1
     *  1           1          -1
     *  2          -1          -1
     *  3          -1           1
     *  4           1          -1
     *  5          -1          -1 ← Up to here is the optimization target
     *  6          -1           1
     *  7           1          -1
     *
     *  for(i=0;i<number_gates_max - 2;i++)
     *    0->nextptr = 1
     *    1->nextptr = 3  -> Both 2's send and receive are -1(NOP)
     *    3->nextptr = 4
     *    4->nextptr = 6  -> Both 5's send and receive are -1(NOP)
     *
     * If both 7's send / receive are -1 and 6's send is -1,
     *  - Because the sequence ends at 6,
     *    - 6-> nextptr = NULL.
     * else,
     *  - Because the sequence continues up to 7,
     *     - 6->nextptr = 7
     *     - 7->nextptr = NULL
     *
     *            recv         send
     *  -----------------------------------
     *  0          -1           1 0/8 : seqptr=0x10bba80  nextptr=0x10bbad0
     *  1           1          -1 1/8 : seqptr=0x10bbad0  nextptr=0x10bbb70
     *  3          -1           1 3/8 : seqptr=0x10bbb70  nextptr=0x10bbbc0
     *  4           1          -1 4/8 : seqptr=0x10bbbc0  nextptr=0x10bbc60
     *  6          -1           1 6/8 : seqptr=0x10bbc60  nextptr=0x10bbcb0
     *  7           1          -1 7/8 : seqptr=0x10bbcb0  nextptr=(nil)
     *
     */

    curr = next = (*seqptr);
    next++;
    for (i=0;i<(*number_gates_max - 2);i++) {
        if ( (UTF_BG_ALLOC_INVALID_INDEX == next->sendinfo.nodeinfo_index) &&
             (UTF_BG_ALLOC_INVALID_INDEX == next->recvinfo.nodeinfo_index) ) {
            /* Skips this step, because both send and receive are NOP */
            next++;
            continue;
        }
        curr->nextptr = next;  /* Updates nextptr of the current step */
        curr          = next;
        next++;
    }

    /* Checks the last two steps */
    if ( (UTF_BG_ALLOC_INVALID_INDEX == next->sendinfo.nodeinfo_index) &&
         (UTF_BG_ALLOC_INVALID_INDEX == next->recvinfo.nodeinfo_index) &&
         (UTF_BG_ALLOC_INVALID_INDEX == curr->sendinfo.nodeinfo_index) ) {
        /* next ->last step , curr ->current step */
        curr->nextptr = NULL;  /* Terminates in the current step */
    }
    else {
        /* Send / receive continues until the final step */
        curr->nextptr = next;
        next->nextptr = NULL;
    }

    /*
     *
     * step2
     *   Optimizes the send/receive gates
     *
     *          recv       send    nextptr
     *    ----------------------------------
     *    0      1         -1        1
     *    1     -1          1        2
     *
     *    If the following conditions are met in any 2 steps send/receive sequence,
     *     - First step -> recv != -1 and send == -1
     *     - 2nd   step -> recv == -1 and send != -1
     *
     *      a. Writes the 2nd step's send to the first step's send.
     *         ->Integrates the 2nd step into the first step.
     *      b. Writes the 2nd step's nextptr to the first step's nextptr.
     *         ->Deletes the second step from the list structure
     *
     *          recv       send    nextptr
     *    ----------------------------------
     *    0      1          1        2
     *
     */

    curr = (*seqptr);
    while (curr->nextptr) {
        next = curr->nextptr;
        if ( (curr->recvinfo.nodeinfo_index != UTF_BG_ALLOC_INVALID_INDEX) &&
             (curr->sendinfo.nodeinfo_index == UTF_BG_ALLOC_INVALID_INDEX) &&
             (next->recvinfo.nodeinfo_index == UTF_BG_ALLOC_INVALID_INDEX) &&
             (next->sendinfo.nodeinfo_index != UTF_BG_ALLOC_INVALID_INDEX) ) {
            curr->sendinfo.vbg_id         = next->sendinfo.vbg_id;
            curr->sendinfo.nodeinfo_index = next->sendinfo.nodeinfo_index;
            curr->sendinfo.index          = next->sendinfo.index;
            curr->sendinfo.phy_6d_addr    = next->sendinfo.phy_6d_addr;
            curr->nextptr = next->nextptr;
        }
        curr = next;
    }

    return;
}

/*
 * Create information of send/receive processes in the butterfly network
 *
 * srinfo         (OUT)  Pointer to source or destination
 * nodeinfo_index (IN)   Index of own process in nodeinfo for each axis
 *  (IN)    axisptr      Pointer to the array of each axis information.
 */
static void initialize_butterfly_proc_info(
                                      utf_bf_alloc_butterfly_proc_info_t *srinfo,
                                      size_t nodeinfo_index,
                                      utf_bg_alloc_axis_detail_info_t *axisptr)
{
    int my_zone = get_my_zone(axisptr,axisptr->nodeinfo_index);

    srinfo->nodeinfo_index = nodeinfo_index;
    if (UTF_BG_ALLOC_INVALID_INDEX != srinfo->nodeinfo_index) {
        srinfo->vbg_id = UTF_BG_ALLOC_VBG_ID_UNDECIDED;
        srinfo->phy_6d_addr =
            &(axisptr->nodeinfo + srinfo->nodeinfo_index)->phy_6d_addr;
        srinfo->index =
            (axisptr->nodeinfo + srinfo->nodeinfo_index)->index;
    }
    else {
        if (UTF_BG_ALLOC_POWEROFTWO_EAR == my_zone) {
            srinfo->vbg_id = UTF_BG_ALLOC_VBG_ID_UNNECESSARY;
        }
        else {
            srinfo->vbg_id = UTF_BG_ALLOC_VBG_ID_NOOPARATION;
        }
        srinfo->phy_6d_addr = NULL;
        srinfo->index       = UTF_BG_ALLOC_INVALID_INDEX;
    }
    return;
}

/*
 * Initializes the send/receive sequence.
 *
 * seqptr  (OUT) Pointer to the send/receive sequence
 *  (IN)    axisptr   Pointer to the array of each axis information.
 * sendtable (IN) Array of destination processes
 * recvtable (IN) Array of source processes
 *
 */
static int initialize_sequence(utf_bf_alloc_butterfly_sequence_info_t **seqptr,
                               utf_bg_alloc_axis_detail_info_t *axisptr,
                               size_t *sendtable,
                               size_t *recvtable)
{
    size_t i;

    /*
     *
     *   recvinfo   sendinfo
     *  0   A          B
     *  1   B          C
     *  2   C          D
     *  3   D          E
     *
     *  number_gates_max = 4
     *
     *  1. Initializes A.
     *  2. for(i=0;i<axisptr->number_gates;i++) initializes B, C, D in a loop.
     *  3. for(i=0;i<axisptr->number_gates;i++) After the loop is completed, nitializes E.
     *
     */

    (*seqptr)->recvinfo.vbg_id         = UTF_BG_ALLOC_VBG_ID_NOOPARATION;
    (*seqptr)->recvinfo.nodeinfo_index = UTF_BG_ALLOC_INVALID_INDEX;
    (*seqptr)->recvinfo.index          = UTF_BG_ALLOC_INVALID_INDEX;
    (*seqptr)->recvinfo.phy_6d_addr    = NULL;
    (*seqptr)->nextptr                 = NULL;

    for (i=0;i<axisptr->number_gates;i++) {
        /* Initialize send infomation. */
        initialize_butterfly_proc_info(&((*seqptr)->sendinfo),*sendtable,axisptr);
        /* Initialize recvice infomation. */
        initialize_butterfly_proc_info(&((*seqptr + 1)->recvinfo),*recvtable,axisptr); 
        (*seqptr)->vbg_id = UTF_BG_ALLOC_VBG_ID_UNDECIDED;
        sendtable++;
        recvtable++;
        (*seqptr)++;
    }

    (*seqptr)->sendinfo.vbg_id         = UTOFU_VBG_ID_NULL;
    (*seqptr)->sendinfo.nodeinfo_index = UTF_BG_ALLOC_INVALID_INDEX;
    (*seqptr)->sendinfo.index          = UTF_BG_ALLOC_INVALID_INDEX;
    (*seqptr)->sendinfo.phy_6d_addr    = NULL;
    (*seqptr)->vbg_id = UTF_BG_ALLOC_VBG_ID_UNDECIDED;

    (*seqptr)++;
    return UTF_SUCCESS;
}

/*
 * Creating a sequence of send/receive processes for a butterfly network
 *
 * axisptr   (IN)  Pointer to the array of each axis information.
 * sendtable (OUT) Array of destination processes
 * recvtable (OUT) Array of source processes
 *
 */
static int make_sequence(utf_bg_alloc_axis_detail_info_t *axisptr,
                         size_t *sendtable,
                         size_t *recvtable)
{
#if defined(DEBUGLOG)
    size_t *sendtable_temp = sendtable;
    size_t *recvtable_temp = recvtable;
    char *tempa,*tempb;
#endif
    size_t i;

    if (!(axisptr->flag_poweroftwo)) {
        /* The number of nodes on the axis is not a power of 2 */
        if (axisptr->nodeinfo_index < axisptr->on_poweroftwo) {
            /* EAR(left) */
            *sendtable = axisptr->nodeinfo_index + axisptr->on_poweroftwo;
            sendtable++;
            SET_NOOP_FOR_EAR();
            *recvtable = axisptr->nodeinfo_index + axisptr->on_poweroftwo;
        }
        else if (axisptr->nodeinfo_index < (axisptr->on_poweroftwo * 2)) {
            /* CHHEK(left) */
            *sendtable = UTF_BG_ALLOC_INVALID_INDEX;
            *recvtable = axisptr->nodeinfo_index - axisptr->on_poweroftwo;
            sendtable++;
            recvtable++;
            SET_PAIR_POWEROFTWO(axisptr->on_poweroftwo);
            *sendtable = axisptr->nodeinfo_index - axisptr->on_poweroftwo;
            *recvtable = UTF_BG_ALLOC_INVALID_INDEX;
        }
        else if (axisptr->nodeinfo_index <
                 ((axisptr->off_poweroftwo * 2) - axisptr->size)) {
            /* FACE */
            *sendtable = UTF_BG_ALLOC_INVALID_INDEX;
            *recvtable = UTF_BG_ALLOC_INVALID_INDEX;
            sendtable++;
            recvtable++;
            SET_PAIR_POWEROFTWO(axisptr->on_poweroftwo);
            *sendtable = UTF_BG_ALLOC_INVALID_INDEX;
            *recvtable = UTF_BG_ALLOC_INVALID_INDEX;
        }
        else if (axisptr->nodeinfo_index < axisptr->off_poweroftwo) {
            /* CHEEK(right) */
            *sendtable = UTF_BG_ALLOC_INVALID_INDEX;
            *recvtable = axisptr->nodeinfo_index +
                (axisptr->size - axisptr->off_poweroftwo);
            sendtable++;
            recvtable++;
            SET_PAIR_POWEROFTWO(axisptr->on_poweroftwo);
            *sendtable = axisptr->nodeinfo_index +
                (axisptr->size - axisptr->off_poweroftwo);
            *recvtable = UTF_BG_ALLOC_INVALID_INDEX;
        }
        else if (axisptr->nodeinfo_index < axisptr->size) {
            /* EAR(right) */
            *sendtable = axisptr->nodeinfo_index -
                (axisptr->size - axisptr->off_poweroftwo);
            sendtable++;
            SET_NOOP_FOR_EAR();
            *recvtable = axisptr->nodeinfo_index -
                (axisptr->size - axisptr->off_poweroftwo);
        }
        else {
            assert(0);
            return UTF_ERR_INTERNAL;
        }
    }
    else {
        /* The number of nodes on the axis is a power of 2. */
        assert(axisptr->number_gates_of_poweroftwo == axisptr->number_gates);
        SET_PAIR_POWEROFTWO(0);
    }

#if defined(DEBUGLOG)
    tempa = (char *)malloc(
        (size_t)8 * axisptr->number_gates + 1);
    if (NULL == tempa) {
        return UTF_ERR_OUT_OF_MEMORY;
    }
    tempb = tempa;

    for (i=0;i<axisptr->number_gates;i++){
        sprintf(tempb," %7d",(int)(*recvtable_temp));
        recvtable_temp++;
        tempb += 8;
    }
    *tempb = '\0';
    fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu: nodeinfo_index=%zu Recvtable=%s \n"
        ,my_hostname,__func__,my_rankset_index
        ,axisptr->nodeinfo_index,tempa
    );

    tempb = tempa;
    for (i=0;i<axisptr->number_gates;i++){
        sprintf(tempb," %7d",(int)(*sendtable_temp));
        sendtable_temp++;
        tempb += 8;
    }
    *tempb = '\0';
    fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu: nodeinfo_index=%zu Sendtable=%s \n"
        ,my_hostname,__func__,my_rankset_index
        ,axisptr->nodeinfo_index,tempa
    );
    free(tempa);
#endif

    return UTF_SUCCESS;
}

/*
 * Creates a butterfly network and returns a pointer to the sequence and
 * the number of relay gates required.
 *
 * axisptr    (IN)         Pointer to the array of each axis information.
 * start_axis (IN)         The first axis of the butterfly network
 * sequence   (OUT)        Pointer to sequence data (for release)
 * optimizedsequence (OUT) Pointer to optimized sequence
 * number_vbgs (OUT)       Number of VBGs required
 *
 */
static int make_butterfly_network(utf_bg_alloc_axis_detail_info_t *axisptr,
                                  int start_axis,
                                  utf_bf_alloc_butterfly_sequence_info_t **sequence,
                                  utf_bf_alloc_butterfly_sequence_info_t **optimizedsequence,
                                  size_t *number_vbgs
)
{
    int rc,i,tail_axis=start_axis,curr_zone,prev_zone;
#if defined(DEBUGLOG)
    int head_axis=start_axis;
#endif
    size_t s;
    size_t number_gates_max=0UL;
    size_t number_relay_gates_allaxis=0UL; /* Number of gates using just half */
    utf_bf_alloc_butterfly_sequence_info_t *sequenceptr,*sequence_head;
    size_t *sender,*reciver;

    /*
     * Creates information to build a butterfly network
     */

    for (i=start_axis;i<UTF_BG_ALLOC_AXIS_NUM_AXES;i++) {
        if ((axisptr + i)->size <= 1) {
            /*
             * There is just own node on the axis.
             */
            continue;
        }

        /* Add one number_gates at the start point of the axis and
         * the end point of the axis, and one number_gates at the relay */
        /* 4node |) - (||) - (| → 2 VBGs! */
        for ((axisptr + i)->number_gates=0UL,s=(axisptr + i)->size;
             1 < s;
             s >>= 1,(axisptr + i)->number_gates++);
        (axisptr + i)->number_gates_of_poweroftwo =
            (axisptr + i)->number_gates; /* For building a butterfly network */

        /*
         * Number of relay gates
         * The number in number_relay_gates is twice the required number
         * -> Number of gates using just half
         */
        (axisptr + i)->number_relay_gates[UTF_BG_ALLOC_POWEROFTWO_EAR] =
            (size_t)1 * 2;
        (axisptr + i)->number_relay_gates[UTF_BG_ALLOC_POWEROFTWO_FACE] =
            (axisptr + i)->number_gates * 2;
        (axisptr + i)->number_relay_gates[UTF_BG_ALLOC_POWEROFTWO_CHEEK] =
            ((axisptr + i)->number_gates + 1) * 2;

        /* If size & size-1 is true and the first bit is set, it is not a power of 2. */
        if ( (axisptr + i)->size & ((axisptr + i)->size - 1) ) {
            size_t w;
            /* int k; */
            /*
             * The number of nodes in the axis is not a power of 2
             * (ex.) size=12
             *   1100(12)      (axisptr + i)->size
             * & 1011(12-1=11) (axisptr + i)->size - 1
             * ------
             *   1000          It is not a power of 2 because the first bit is set.
             */
            /*
             * Since the number of nodes is not a power of 2, gets the range to perform butterfly.
             * (ex.) size=12
             *
             * A. Gets a power of 2 that fits in the number of nodes.
             *     1 << number_gates_of_poweroftwo
             *     → 1 << 3 = 8 (1000)
             *
             * B. Gets the start position of butterfly communication.
             *    (size - value of A) / 2
             *    → (12 - 8) / 2 = 2
             *       (12-8) is the number of processes that do not join butterfly.
             *       Divides by 2 because the nodes that do not participate are
             *       at both ends of the butterfly
             *
             * C. Gets the end position of butterfly communication.
             *    B(the start position of butterfly) + A
             *    2 + 8 = 10
             *    10 is the end position of butterfly. Up to (10-1=9) join butterfly.
             *
             *   0 1 2 3 4 5 6 7 8 9 10 11
             *       B               C
             *       <2^n-butterfly>
             */
            w = (1 << (axisptr + i)->number_gates_of_poweroftwo);
            /* for (k=0,w=1;k<=(axisptr + i)->number_gates;k++,w*=2); */
            (axisptr + i)->on_poweroftwo = ((axisptr + i)->size - w) / 2;
            (axisptr + i)->off_poweroftwo = (axisptr + i)->on_poweroftwo + w;
            /*
             * Gets number_gates.
             * Because number_gates is the number in the zone (CHEEK)
             * where the sequences are maximized, number_gates is increased
             * by the number of send/receive sequences (+2)
             * to and from the zone (EAR) before and after the butterfly.
             *
             *    (ex.) Butterfly sequences when size = 12
             *       From a power of 2 -> number_gates(number_gates_of_poweroftwo) == 3
             *       +2 to 3.
*              <---------- (axisptr+x)->number_gates ---------------------->
*                         0           1           2          3            4
*              +-----------+-----------+-----------+-----------+-----------+
*  EAR  -send  |send       |Nooparation|Nooparation|Nooparation|Nooparation|
*              +-----------------------------------------------------------+
*  EAR  -recv  |Nooparation|Nooparation|Nooparation|Nooparation|recv       |
*              +-----------------------------------------------------------+
*  CHEEK-send  |Nooparation|butterfly  |butterfly  |butterfly  |send       |
*              +-----------------------------------------------------------+
*  CHEEK-recv  |recv       |butterfly  |butterfly  |butterfly  |Nooparation|
*              +-----------------------------------------------------------+
*  FACE -send  |Nooparation|butterfly  |butterfly  |butterfly  |Nooparation|
*              +-----------------------------------------------------------+
*  FACE -recv  |Nooparation|butterfly  |butterfly  |butterfly  |Nooparation|
*              +-----------------------------------------------------------+
             */
            (axisptr + i)->number_gates += 2;
            (axisptr + i)->flag_poweroftwo = false;
        }
        else {
            /*
             * The number of nodes in the axis is a power of 2.
             * (ex.) size=8
             *   1000(8)     (axisptr + i)->size
             * & 0111(8-1=7) (axisptr + i)->size - 1
             * ------
             *   0000     Since all bits are OFF, it is a power of 2.
             */
            (axisptr + i)->on_poweroftwo = 0;
            (axisptr + i)->off_poweroftwo = (axisptr + i)->size;
            (axisptr + i)->flag_poweroftwo = true;
        }

        if (!number_gates_max) {
            /*
             * Since the axis has the first multiple nodes,
             * reduces the number for the start gate.
             * Only when the zone is FACE or EAR. |)So it can be reduced.
             */
            (axisptr + i)->number_relay_gates[UTF_BG_ALLOC_POWEROFTWO_EAR]--;
            (axisptr + i)->number_relay_gates[UTF_BG_ALLOC_POWEROFTWO_FACE]--;
#if defined(DEBUGLOG)
            head_axis = i;
#endif
        }
        number_gates_max += ((axisptr + i)->number_gates + 1);
        tail_axis = i;
    }

    /*
     * Since the axis has the last multiple nodes,
     * reduces the number for the end gate.
     * Only when the zone is FACE or EAR. |)So it can be reduced.
     */
    (axisptr + tail_axis)->number_relay_gates[UTF_BG_ALLOC_POWEROFTWO_EAR]--;
    (axisptr + tail_axis)->number_relay_gates[UTF_BG_ALLOC_POWEROFTWO_FACE]--;

#if defined(DEBUGLOG)
    fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu:AXISINFO number_gates_max=%zu head_axis=%d tail_axis=%d \n"
        ,my_hostname,__func__,my_rankset_index
        ,number_gates_max,head_axis,tail_axis
    );
#endif

    prev_zone=(-1);
    for (i=start_axis;i<UTF_BG_ALLOC_AXIS_NUM_AXES;i++) {
        if ((axisptr + i)->size <= 1)  { continue; }

        curr_zone = get_my_zone((axisptr + i),(axisptr + i)->nodeinfo_index);
#if defined(DEBUGLOG)
    fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu:AXISINFO i=%d zone=%d \n"
        ,my_hostname,__func__,my_rankset_index
        ,i,curr_zone
    );
#endif

        switch (prev_zone) {
            case UTF_BG_ALLOC_POWEROFTWO_EAR:
            case UTF_BG_ALLOC_POWEROFTWO_FACE:
                if (UTF_BG_ALLOC_POWEROFTWO_CHEEK == curr_zone) {
                    /*
                     * prev   (|
                     *   NEED  |)
                     * curr      (|
                     */
                    number_relay_gates_allaxis++;
                }
                else {
                    /*
                     * prev   (|
                     * curr      |)
                     */
                }
                break;
            case UTF_BG_ALLOC_POWEROFTWO_CHEEK:
                if ( (UTF_BG_ALLOC_POWEROFTWO_EAR == curr_zone) ||
                     (UTF_BG_ALLOC_POWEROFTWO_FACE  == curr_zone) ) {
                    /*
                     * prev   |)
                     *   NEED   (|
                     * curr      |)
                     */
                    number_relay_gates_allaxis++;
                }
                else {
                    /*
                     * prev   |)
                     * curr      (|
                     */
                }
                break;
            default:
                /* this is the first axis */
                break;
        }

        number_relay_gates_allaxis +=
            (axisptr + i)->number_relay_gates[curr_zone];
        prev_zone = curr_zone;
    }

#if defined(DEBUGLOG)
    fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu:AXISINFO number_relay_gates_allaxis=%zu \n"
        ,my_hostname,__func__,my_rankset_index
        ,number_relay_gates_allaxis
    );
    for (i=UTF_BG_ALLOC_AXIS_IN_NODE;i<UTF_BG_ALLOC_AXIS_NUM_AXES;i++) {
        fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu:AXISINFO %d size=%zu nodeinfo=%p number_gates=%zu relay_gates=%zu/%zu/%zu on_2^n=%zu off_2^n=%zu number_gates2^=%zu flag2^=%d \n"
            ,my_hostname,__func__,my_rankset_index
            ,i,(axisptr + i)->size,(axisptr + i)->nodeinfo
            ,(axisptr + i)->number_gates
            ,(axisptr + i)->number_relay_gates[UTF_BG_ALLOC_POWEROFTWO_EAR]
            ,(axisptr + i)->number_relay_gates[UTF_BG_ALLOC_POWEROFTWO_CHEEK]
            ,(axisptr + i)->number_relay_gates[UTF_BG_ALLOC_POWEROFTWO_FACE]
            ,(axisptr + i)->on_poweroftwo,(axisptr + i)->off_poweroftwo
            ,(axisptr + i)->number_gates_of_poweroftwo
            ,(axisptr + i)->flag_poweroftwo
        );
    }
#endif

    /* Builds a butterfly network */
    sequence_head =
    sequenceptr = (utf_bf_alloc_butterfly_sequence_info_t *)malloc(
        sizeof(utf_bf_alloc_butterfly_sequence_info_t) * number_gates_max);
    if (NULL == sequenceptr) {
        *sequence = NULL;
        *optimizedsequence = NULL;
        *number_vbgs = 0UL;
        return UTF_ERR_OUT_OF_MEMORY;
    }
    memset((void *)sequenceptr,0,
           sizeof(utf_bf_alloc_butterfly_sequence_info_t) * number_gates_max);

    for (i=start_axis;i<UTF_BG_ALLOC_AXIS_NUM_AXES;i++) {
        if ((axisptr + i)->size <= 1)  { continue; }

        sender  = (size_t *)malloc(
            sizeof(size_t) * (axisptr + i)->number_gates);
        reciver = (size_t *)malloc(
            sizeof(size_t) * (axisptr + i)->number_gates);
        if ( (NULL == sender) || (NULL == reciver) ) {
            FREEAREA_BUTTERFLY();
            free(sequence_head);
            *sequence = NULL;
            *optimizedsequence = NULL;
            *number_vbgs = 0UL;
            return UTF_ERR_OUT_OF_MEMORY;
        }

#if defined(DEBUGLOG)
    fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu:AXISINFO %d make_sequence \n"
        ,my_hostname,__func__,my_rankset_index
        ,i
    );
#endif
        rc = make_sequence((axisptr + i),sender,reciver);
        if (rc != UTF_SUCCESS) {
            FREEAREA_BUTTERFLY();
            free(sequence_head);
            *sequence = NULL;
            *optimizedsequence = NULL;
            *number_vbgs = 0UL;
            return rc;
        }

#if defined(DEBUGLOG)
    fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu:AXISINFO %d initialize_sequence ptr=%p \n"
        ,my_hostname,__func__,my_rankset_index
        ,i,sequenceptr
    );
#endif
        initialize_sequence(&sequenceptr,
                            (axisptr + i),sender,reciver);

        FREEAREA_BUTTERFLY();
    }

#if defined(DEBUGLOG)
    print_sequence(sequence_head,number_gates_max,false);
#endif

    sequenceptr=sequence_head;

    optimized_sequence(&sequenceptr,&number_gates_max);

#if defined(DEBUGLOG)
    print_sequence(sequenceptr,number_gates_max,true);
#endif

    *sequence = sequence_head;        /* Pointer to freeing memory */
    *optimizedsequence = sequenceptr; /* Pointer to the optimized part */

    /*
     * Gets the number of VBGs used by the utofu_alloc_vbg function
     */
    for (*number_vbgs=0UL;
         sequenceptr->nextptr!=NULL;
         sequenceptr=sequenceptr->nextptr,(*number_vbgs)++);


    return UTF_SUCCESS;
}

/*
 * Gets a shared memory
 *
 * intra_node_info (IN) Process information in the node
 */
int make_mmap_file(
                   uint32_t vpid,
                   utf_bg_intra_node_detail_info_t *intra_node_info)
{
    char filename_temp[64];
    char *dirname=".";
    size_t namelen=0UL;

    /*
     * Maximum number of processes in the node == MPI_COMM_WORLD(utf_bg_first_call == true)
     */
    utf_bg_mmap_file_info.width = intra_node_info->size;

    /* Gets the size of the shared memory file */
    if (utf_bg_intra_node_barrier_is_tofu) {
        utf_bg_mmap_file_info.size = UTF_BG_MMAP_HEADER_SIZE;
    }
    else {
        utf_bg_mmap_file_info.size =
            /* TNI and BGID information at the beginning */
            UTF_BG_MMAP_HEADER_SIZE
            +
            /* BUFFER + SEQ */
            UTF_BG_MMAP_TNI_INF_SIZE * UTF_BG_NUM_TNI;
    }

    if (!utf_bg_mmap_file_info.disable_utf_shm) {
#if defined(TOFUGIJI)
        if (UTF_BG_INTRA_NODE_MANAGER == intra_node_info->state) {
#endif
#if defined(DEBUGLOG)
    fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu call utf_shm_init \n"
        ,my_hostname,__func__,my_rankset_index
    );
#endif
            if (0 != utf_shm_init(utf_bg_mmap_file_info.size,
                                  (void **)(&utf_bg_mmap_file_info.mmaparea))) {
                return UTF_ERR_INTERNAL;
            }
#if defined(TOFUGIJI)
        }
#endif
        return UTF_SUCCESS;
    }

    /* Gets the directory name to create the shared memory file */
    namelen += strlen(dirname);

    /* Generates shared memory file name */
    sprintf(filename_temp,"/utf_bg_mmap_file-%08x-%08x-%08x-%08x"
        ,my_jobid
        ,utf_info.vname[vpid].xyz.s.x
        ,utf_info.vname[vpid].xyz.s.y
        ,utf_info.vname[vpid].xyz.s.z
    );
    namelen += strlen(&filename_temp[0]);

    utf_bg_mmap_file_info.file_name = (char *)malloc(namelen);
    if (NULL == utf_bg_mmap_file_info.file_name) {
        return UTF_ERR_OUT_OF_MEMORY;
    }
    memset((void *)utf_bg_mmap_file_info.file_name,0,namelen);

    strcat(utf_bg_mmap_file_info.file_name,dirname);
    strcat(utf_bg_mmap_file_info.file_name,filename_temp);

    if (UTF_BG_INTRA_NODE_MANAGER == intra_node_info->state) {
        void *mmaparea=NULL;
        ssize_t wrc;
        /* Creates a file */
        utf_bg_mmap_file_info.fd = open(utf_bg_mmap_file_info.file_name,
                                        O_RDWR|O_CREAT, 0666);
        if (-1 == utf_bg_mmap_file_info.fd) {
#if defined(DEBUGLOG)
            fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu: open=%d errno=%d \n"
                ,my_hostname,__func__,my_rankset_index
                ,utf_bg_mmap_file_info.fd,errno
            );
#else
            utf_printf("[OPEN-ERR] fd=%d errno=%d \n",utf_bg_mmap_file_info.fd,errno);
#endif
            return UTF_ERR_INTERNAL;
        }
        mmaparea = (void *)malloc(utf_bg_mmap_file_info.size);
        if (NULL == mmaparea) {
            return UTF_ERR_OUT_OF_MEMORY;
        }
        memset(mmaparea,0,utf_bg_mmap_file_info.size);
        wrc = write(utf_bg_mmap_file_info.fd,
                    (const void *)mmaparea,
                    utf_bg_mmap_file_info.size);
        free(mmaparea);
        if ( ((ssize_t)-1 == wrc) ||
             (wrc < (ssize_t)utf_bg_mmap_file_info.size) ) {
#if defined(DEBUGLOG)
            fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu: write=%ld errno=%d \n"
                ,my_hostname,__func__,my_rankset_index
                ,wrc,errno
            );
#else
            utf_printf("[WRITE-ERR] wrc=%ld errno=%d \n",wrc,errno);
#endif
            return UTF_ERR_INTERNAL;
        }

        /* mmap */
        mmaparea=NULL;
        mmaparea = mmap(0,
                        utf_bg_mmap_file_info.size,
                        PROT_READ | PROT_WRITE, MAP_SHARED,
                        utf_bg_mmap_file_info.fd,
                        0);
        if (MAP_FAILED == mmaparea) {
#if defined(DEBUGLOG)
            fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu: MAP_FAILED errno=%d \n"
                ,my_hostname,__func__,my_rankset_index
                ,errno
            );
#else
            utf_printf("[MMAP-ERR] errno=%d \n",errno);
#endif
            return UTF_ERR_OUT_OF_MEMORY;
        }
        utf_bg_mmap_file_info.mmaparea = (utf_bg_mmap_header_info_t *)mmaparea;
    }

    return UTF_SUCCESS;
}

/*
 * Allocates gates
 */

/*
 * Allocates VBGs using the utofu function.
 *
 * start_tni   (IN)  The first TNI to try to allocate VBGs
 * num_vbg     (IN)  Number of VBGs (start gates + relay gates)
 * vbg_ids     (OUT) Pointer to an array of VBG
 * my_tni      (OUT) TNI succeeded in allocating VBGs
 *
 */
static int alloc_vbg(utofu_tni_id_t my_tni,
                     size_t num_vbg,
                     utofu_vbg_id_t *vbg_ids)
{
    int rc;
    UTOFU_CALL_RC(rc,utofu_alloc_vbg,my_tni,num_vbg,utf_bg_alloc_flags,vbg_ids)
}

static int allocate_vbg_core(utofu_tni_id_t start_tni,
                             size_t num_vbg,
                             utofu_vbg_id_t *vbg_ids,
                             utofu_tni_id_t *my_tni)
{
    int rc = UTF_SUCCESS;
#if defined(DEBUGLOG)
    size_t i;
#endif

    *my_tni = start_tni;

    do {
        rc = alloc_vbg(*my_tni,num_vbg,vbg_ids);
        UTOFU_ERRCHECK_EXIT_IF(0,rc)
        if (UTOFU_ERR_FULL != rc) {
            /* UTOFU_SUCCESS or a fatal error */
            break;
        }

        /*
         * If UTOFU_ERR_FULL is returned,
         * then no VBG is actually allocated in the utofu library,
         * so there is no need to free them.
         */

        *my_tni = (*my_tni + 1) % UTF_BG_NUM_TNI; 
    } while(*my_tni != start_tni);

#if defined(DEBUGLOG)
    for (i=0;i<num_vbg;i++) {
        fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu: utofu_alloc_vbg=%d my_tni=%d/%d num_vbg=%zu/%zu vbg_ids=%016lx \n"
            ,my_hostname,__func__,my_rankset_index
            ,rc,*my_tni,start_tni,i,num_vbg,vbg_ids[i]
        );
    }
#endif
    return rc;
}

/*
 * Creates the information for dst remote
 * Sets dst(dst == send) in rmt_src_dst_seqs
 */
#define SET_DST_REMOTE(tempseq,i) \
{ \
    if (UTF_BG_ALLOC_INVALID_INDEX != tempseq->sendinfo.nodeinfo_index) { \
        utf_bg_detail_info->rmt_src_dst_seqs[i].dst_rmt_index = \
            tempseq->sendinfo.index; \
    } \
    else { \
        utf_bg_detail_info->rmt_src_dst_seqs[i].dst_rmt_index = \
            UTF_BG_BGINFO_INVALID_INDEX; \
    } \
}

/*
 * Creates the information for src remote
 * Sets src(src == recv) in rmt_src_dst_seqs
 */
#define SET_SRC_REMOTE(tempseq,i) \
{ \
    if (UTF_BG_ALLOC_INVALID_INDEX != tempseq->recvinfo.nodeinfo_index) { \
        utf_bg_detail_info->rmt_src_dst_seqs[i].src_rmt_index = \
            tempseq->recvinfo.index; \
    } \
    else { \
        utf_bg_detail_info->rmt_src_dst_seqs[i].src_rmt_index = \
            UTF_BG_BGINFO_INVALID_INDEX; \
    } \
}

/* Gets BGID from VBG */
#define GET_BGID_FROM_VBGID(vbgid,id)                                  \
{                                                                      \
    id = (utofu_bg_id_t)(vbgid & UTF_BG_ALLOC_MSK_BGID);               \
}

/*
 * Allocates VBGs
 *
 * number_vbgs     (IN) Number of VBGs required
 * intra_node_info (IN) Process information in the node
 * sequenceptr     (IN) Pointer to sequences
 * my_tni          (OUT)TNI succeeded in allocating VBGs
 */
static int allocate_vbg(size_t number_vbgs,
                        utf_bg_intra_node_detail_info_t *intra_node_info,
                        utf_bf_alloc_butterfly_sequence_info_t *sequenceptr,
                        utofu_tni_id_t *my_tni)
{
    int rc = UTF_SUCCESS;
    size_t i;
    utf_bf_alloc_butterfly_sequence_info_t *tempseq;
    utofu_tni_id_t start_tni;

    /* Decides the number of gates to allocate */
    utf_bg_detail_info->num_vbg = number_vbgs;

    /* Allocates memory */
    utf_bg_detail_info->vbg_ids = (utofu_vbg_id_t *)malloc
        (sizeof(utofu_vbg_id_t) * utf_bg_detail_info->num_vbg);
    if (NULL == utf_bg_detail_info->vbg_ids) {
        return UTF_ERR_OUT_OF_MEMORY;
    }
    memset((void *)utf_bg_detail_info->vbg_ids,0,
         sizeof(utofu_vbg_id_t) * utf_bg_detail_info->num_vbg);

    utf_bg_detail_info->vbg_settings = (struct utofu_vbg_setting *)malloc
        (sizeof(struct utofu_vbg_setting) * utf_bg_detail_info->num_vbg);
    if (NULL == utf_bg_detail_info->vbg_settings) {
        free(utf_bg_detail_info->vbg_ids);
        return UTF_ERR_OUT_OF_MEMORY;
    }
    memset((void *)utf_bg_detail_info->vbg_settings,0,
         sizeof(struct utofu_vbg_setting) * utf_bg_detail_info->num_vbg);

    utf_bg_detail_info->rmt_src_dst_seqs = (utf_rmt_src_dst_index_t *)malloc
        (sizeof(utf_rmt_src_dst_index_t) * utf_bg_detail_info->num_vbg);
    if (NULL == utf_bg_detail_info->rmt_src_dst_seqs) {
        free(utf_bg_detail_info->vbg_ids);
        free(utf_bg_detail_info->vbg_settings);
        return UTF_ERR_OUT_OF_MEMORY;
    }
    memset((void *)utf_bg_detail_info->rmt_src_dst_seqs,0,
         sizeof(utf_rmt_src_dst_index_t) * utf_bg_detail_info->num_vbg);

    /* Determines the using TNI */
    if (utf_bg_first_call) {
        start_tni = 0;
    }
    else {
        /* Reads from the beginning of shared memory */
        assert(utf_bg_mmap_file_info.mmaparea);
        start_tni = utf_bg_mmap_file_info.mmaparea->next_tni_id;
#if defined(DEBUGLOG)
        fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu: mmap: read next_tni_id=%u \n"
            ,my_hostname,__func__,my_rankset_index
            ,utf_bg_mmap_file_info.mmaparea->next_tni_id
        );
#endif
    }
    /* Gets the next TNI number */
    utf_bg_next_tni_id = ( start_tni + ((utf_bg_intra_node_barrier_is_tofu) ? intra_node_info->size : 1) ) % UTF_BG_NUM_TNI;
    start_tni = (start_tni + intra_node_info->intra_index) % UTF_BG_NUM_TNI;

    rc = allocate_vbg_core(start_tni,
                           utf_bg_detail_info->num_vbg,
                           utf_bg_detail_info->vbg_ids,
                           my_tni);
    if (UTF_SUCCESS != rc) {
        /* Returns except UTF_SUCCESS. */
        free(utf_bg_detail_info->rmt_src_dst_seqs);
        utf_bg_detail_info->rmt_src_dst_seqs = NULL;
        free(utf_bg_detail_info->vbg_settings);
        utf_bg_detail_info->vbg_settings = NULL;
        free(utf_bg_detail_info->vbg_ids);
        utf_bg_detail_info->vbg_ids=NULL;
        utf_bg_detail_info->num_vbg = 0;
        return rc;
    }

    /*
     * If the gate can be allocated, creates rmt_src_dst_seqs and returns.
     */

    tempseq = sequenceptr;
    i = 0UL;
    while (tempseq) {
        if (tempseq == sequenceptr) {
            /* Start gate */
            tempseq->vbg_id = utf_bg_detail_info->vbg_ids[0];
            SET_DST_REMOTE(tempseq,0);
        }
        else if (NULL == tempseq->nextptr) {
            /* Start/end gate */
            tempseq->vbg_id = utf_bg_detail_info->vbg_ids[0];
            SET_SRC_REMOTE(tempseq,0);
        }
        else {
            /* Relay gate */
            tempseq->vbg_id = utf_bg_detail_info->vbg_ids[i];
            SET_SRC_REMOTE(tempseq,i);
            SET_DST_REMOTE(tempseq,i);
        }
        tempseq = tempseq->nextptr;
        i++;
    }
    assert((i - 1)==utf_bg_detail_info->num_vbg);

    if (UTF_BG_INTRA_NODE_MANAGER == intra_node_info->state) {
        utofu_bg_id_t temp_start_bg;
        int j=0;
        utf_bg_manager_tnibg_info volatile *info =
            &utf_bg_mmap_file_info.mmaparea->manager_info[0];

        assert(utf_bg_mmap_file_info.mmaparea);
        GET_BGID_FROM_VBGID(utf_bg_detail_info->vbg_ids[0],temp_start_bg); 
        for (i=0UL;i<intra_node_info->size;i++) {
            for (j=0;j<ppn_job;j++) {
                if (intra_node_info->vpids[i] ==
                    utf_bg_alloc_intra_world_indexes[j]) {
                    info[j].tni       = *my_tni;
                    info[j].start_bg  = temp_start_bg;
                }
            }
        }
        msync((void *)utf_bg_mmap_file_info.mmaparea,
              (size_t)UTF_BG_MMAP_HEADER_SIZE,MS_SYNC);
#if defined(DEBUGLOG)
        for (i=0UL;i<intra_node_info->size;i++) {
            for (j=0;j<ppn_job;j++) {
                if (intra_node_info->vpids[i] ==
                    utf_bg_alloc_intra_world_indexes[j]) {
                    fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu: mmap: intranode=%zu/%zu vpid=%zu world_indexes=%d/%d vpid=%zu j=%d -> manager_tni=%u/%u manager_start_bg=%u/%u(%016lx) next_tni_id=%u \n"
                        ,my_hostname,__func__,my_rankset_index
                        ,i,intra_node_info->size,intra_node_info->vpids[i]
                        ,j,ppn_job,utf_bg_alloc_intra_world_indexes[j]
                        ,j
                        ,info[j].tni,*my_tni
                        ,info[j].start_bg,temp_start_bg
                        ,utf_bg_detail_info->vbg_ids[0]
                        ,utf_bg_mmap_file_info.mmaparea->next_tni_id
                    );
                }
            }
        }
#endif
    }

#if defined(DEBUGLOG)
    tempseq = sequenceptr;
    i = 0UL;
    fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu: my_tni=%d utf_bg_next_tni_id=%d rc=%d \n"
        ,my_hostname,__func__,my_rankset_index
        ,*my_tni,utf_bg_next_tni_id,rc
    );
    while ((tempseq) && (tempseq->nextptr)) {
        fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu: num_vbg=%zu/%zu vbg_ids=%016lx :src_rmt=%ld dst_rmt=%ld :seq=%p/%p vbg=%016lx recv=%ld/%ld send=%ld/%ld \n"
            ,my_hostname,__func__,my_rankset_index
            ,i,utf_bg_detail_info->num_vbg,utf_bg_detail_info->vbg_ids[i]
            ,utf_bg_detail_info->rmt_src_dst_seqs[i].src_rmt_index
            ,utf_bg_detail_info->rmt_src_dst_seqs[i].dst_rmt_index
            ,tempseq,sequenceptr,tempseq->vbg_id
            ,tempseq->recvinfo.index,tempseq->recvinfo.nodeinfo_index
            ,tempseq->sendinfo.index,tempseq->sendinfo.nodeinfo_index
        );
        tempseq = tempseq->nextptr;
        i++;
    }
    fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu: seq=%p/%p vbg=%016lx recv=%ld/%ld send=%ld/%ld \n"
        ,my_hostname,__func__,my_rankset_index
        ,tempseq,sequenceptr,tempseq->vbg_id
        ,tempseq->recvinfo.index,tempseq->recvinfo.nodeinfo_index
        ,tempseq->sendinfo.index,tempseq->sendinfo.nodeinfo_index
    );
#endif

    return rc;
}

/*
 * In the case of zone (EAR),
 * the sequences are filled with UTF_BG_ALLOC_VBG_ID_UNNECESSARY
 * from the end of sendinfo.nodeinfo_index in the first row
 * to the front of recvinfo.nodeinfo_index in the last row.
 *
 *              recv           send             nextptr
 *    ---------------------------------------------------
 *    0          -1             1                 4
 *    1          UNNECESSARY    UNNECESSARY
 *    2          UNNECESSARY    UNNECESSARY
 *    3          UNNECESSARY @1 UNNECESSARY @2
 *    4          1              -1                NULL
 *
 *  Does not connect dst_lcl_vbg and src_lcl_vbg in the zone (EAR).->Sets UTOFU_VBG_ID_NULL
 *
 *  For both of the following conditions,
 *  ckecks the information in the row immediately before the last row.
 *  (3rd row in the above example)
 *   - Set dst_lcl_vbg_id to UTOFU_VBG_ID_NULL.
 *   - Set src_lcl_vbg_id to UTOFU_VBG_ID_NULL.
 *
 *  Conditions to set dst_lcl_vbg_id to UTOFU_VBG_ID_NULL
 *    0->sendinfo.nodeinfo_index != -1 &&
 *    3->sendinfo.nodeinfo_index == UNNECESSARY @2
 *    3 is traced by(0->nextptr - 1).
 *
 *  Conditions to set src_lcl_vbg_id to UTOFU_VBG_ID_NULL
 *    4->recvinfo.nodeinfo_index != -1 &&
 *    3->recvinfo.nodeinfo_inde == UNNECESSARY @1
 *    3 is traced by(4 - 1).
 *
 */
/* Sets a sequence of a local VBG for the destination of the output signal */
#define SET_DST_LOCAL(tempseq,i)                                          \
{                                                                         \
    if ( (UTF_BG_ALLOC_VBG_ID_NOOPARATION != tempseq->sendinfo.vbg_id) && \
         (UTF_BG_ALLOC_VBG_ID_UNNECESSARY ==                              \
          (tempseq->nextptr - 1)->sendinfo.vbg_id) ) {                    \
        /* Since it is in the zone EAR, dst-local is not connected */     \
        utf_bg_detail_info->vbg_settings[i].dst_lcl_vbg_id =              \
            UTOFU_VBG_ID_NULL;                                            \
    }                                                                     \
    else {                                                                \
        utf_bg_detail_info->vbg_settings[i].dst_lcl_vbg_id =              \
            (tempseq->nextptr)->vbg_id;                                   \
    }                                                                     \
}

/* Sets a sequence of a local VBG for the source of the input signal */
#define SET_SRC_LOCAL(tempseq,i)                                          \
{                                                                         \
    if ( (UTF_BG_ALLOC_VBG_ID_NOOPARATION != tempseq->recvinfo.vbg_id) && \
         (UTF_BG_ALLOC_VBG_ID_UNNECESSARY ==                              \
          (tempseq - 1)->recvinfo.vbg_id) ) {                             \
        /* Since it is in the zone EAR, src-local is not connected */     \
        utf_bg_detail_info->vbg_settings[i].src_lcl_vbg_id =              \
            UTOFU_VBG_ID_NULL;                                            \
    }                                                                     \
    else {                                                                \
        utf_bg_detail_info->vbg_settings[i].src_lcl_vbg_id =              \
            prevseq->vbg_id;                                              \
    }                                                                     \
}

/*
 * Sets sequences of local VBGs
 *
 * sequenceptr     (IN) Pointer to sequences
 */
static void set_local_vbg(utf_bf_alloc_butterfly_sequence_info_t *sequenceptr)
{
    utf_bf_alloc_butterfly_sequence_info_t *tempseq,*prevseq=NULL;
    size_t i=0UL;

    tempseq = sequenceptr;

    while (tempseq) {
        if (tempseq == sequenceptr) {
            /* start gate */
            utf_bg_detail_info->vbg_settings[0].vbg_id = tempseq->vbg_id;
            SET_DST_LOCAL(tempseq,0);
        }
        else if (NULL == tempseq->nextptr) {
            /* start/end gate */
            SET_SRC_LOCAL(tempseq,0);
        }
        else {
            /* relay gate */
            utf_bg_detail_info->vbg_settings[i].vbg_id = tempseq->vbg_id;
            SET_DST_LOCAL(tempseq,i);
            SET_SRC_LOCAL(tempseq,i);
        }
        prevseq = tempseq;
        tempseq = tempseq->nextptr;
        i++;
    }
    assert((i-1) == utf_bg_detail_info->num_vbg);

#if defined(DEBUGLOG)
    for (i=0;i<utf_bg_detail_info->num_vbg;i++) {
        fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu: num_vbg=%zu/%zu src_lcl_vbg_id=%016lx vbg_id=%016lx dst_lcl_vbg_id=%016lx src_rmt_index=%ld dst_rmt_index=%ld \n"
            ,my_hostname,__func__,my_rankset_index
            ,i,utf_bg_detail_info->num_vbg
            ,utf_bg_detail_info->vbg_settings[i].src_lcl_vbg_id
            ,utf_bg_detail_info->vbg_settings[i].vbg_id
            ,utf_bg_detail_info->vbg_settings[i].dst_lcl_vbg_id
            ,utf_bg_detail_info->rmt_src_dst_seqs[i].src_rmt_index
            ,utf_bg_detail_info->rmt_src_dst_seqs[i].dst_rmt_index
        );
    }
#endif

    return;
}

/*
 * Creates bginfo
 */

/* Gets TNI from VBG */
#define GET_TNI_FROM_VBGID(vbgid,tni)                                  \
{                                                                      \
    tni = (utofu_tni_id_t)                                             \
        ((vbgid & UTF_BG_ALLOC_MSK_TNI) >> UTF_BG_BITSFT_BGINFO_TNI);  \
}

/* Compares the physical 6D address in the sequence
 * with the 6D address in the rankset */
#define CHECK_PHY_6D_ADDR_SEQ(found,seq,temp) \
{                                             \
    found =                                   \
    ( (seq.phy_6d_addr->s.x == temp.s.x)  &&  \
      (seq.phy_6d_addr->s.y == temp.s.y)  &&  \
      (seq.phy_6d_addr->s.z == temp.s.z)  &&  \
      (seq.phy_6d_addr->s.a == temp.s.a)  &&  \
      (seq.phy_6d_addr->s.b == temp.s.b)  &&  \
      (seq.phy_6d_addr->s.c == temp.s.c) );   \
}

/* If gates cannot be allocated,
 * sets a bit in bginfo to indicate that barrier communication is not available. */
#define SET_DISABLE_BGINFO(bginfoposition)                           \
{                                                                    \
    bginfoposition = ((uint64_t)1 << UTF_BG_BITSFT_BGINFO_DISABLE);  \
}

/* If gates are allocated, sets information in bginfo. */
#define SET_ENABLE_BGINFO(bginfoposition,                                   \
                         mytni,                                             \
                         coordc,                                            \
                         coordb,                                            \
                         coorda,                                            \
                         coordz,                                            \
                         coordy,                                            \
                         coordx,                                            \
                         src_bg_id,                                         \
                         dst_bg_id)                                         \
{                                                                           \
    bginfoposition = ((uint64_t)mytni     << UTF_BG_BITSFT_BGINFO_TNI)    | \
                    ((uint64_t)coordc    << UTF_BG_BITSFT_BGINFO_COORD_C) | \
                    ((uint64_t)coordb    << UTF_BG_BITSFT_BGINFO_COORD_B) | \
                    ((uint64_t)coorda    << UTF_BG_BITSFT_BGINFO_COORD_A) | \
                    ((uint64_t)coordz    << UTF_BG_BITSFT_BGINFO_COORD_Z) | \
                    ((uint64_t)coordy    << UTF_BG_BITSFT_BGINFO_COORD_Y) | \
                    ((uint64_t)coordx    << UTF_BG_BITSFT_BGINFO_COORD_X) | \
                    ((uint64_t)src_bg_id << UTF_BG_BITSFT_BGINFO_SRCBGID) | \
                    ((uint64_t)dst_bg_id);                                  \
}

/*
 * Creates bginfo
 *
 * my_tni      (IN)     TNI of allocated VBGs
 * sequenceptr (IN)     Pointer to sequences
 * rankset     (IN)     An array of VCQs used by all processes in the barrier network
 *   (IN)     my_index  Own rank.
 *   (IN/OUT) bginfo    Pointer to the array of the VBG information of
 *                      the processes.
 *
 */
int make_bginfo(utofu_tni_id_t my_tni,
                utf_bf_alloc_butterfly_sequence_info_t *sequenceptr,
                uint32_t       *rankset,
                size_t my_index,
                utf_bg_info_t *bginfo)
{
    utf_bf_alloc_butterfly_sequence_info_t *sendseq,*recvseq;
    uint32_t temp_nodeid,my_nodeid;
    utofu_tni_id_t temp_tni;

    /* Connects local VBGs */
    set_local_vbg(sequenceptr);

    /* Gets the node ID andthe 6D address of own process */
    GET_NODEID_FROM_RANKSET(my_nodeid,rankset[my_index]);

    sendseq = sequenceptr;
    while (sendseq) {

        if (UTF_BG_ALLOC_INVALID_INDEX != sendseq->sendinfo.nodeinfo_index) {
            bool found;
            utofu_bg_id_t src_id,dst_id;

            src_id =(utofu_bg_id_t)0;
            GET_BGID_FROM_VBGID(sendseq->vbg_id,src_id);
            found = false;
            recvseq = sequenceptr;
            while (recvseq) {
                /*
                 * Searchs for the same index as sendseq-> sendinfo.index
                 * from the receive information of the sequences.
                 */
                /*
                 *  sequence table
                 *
                 *  VBG    src_rmt    dst_rmt
                 *  ----------------------------
                 *  6        7          1
                 *  18       1          3
                 *  19       3          7
                 *
                 *  1.
                 *     dst_rmt == 1
                 *       - src-VBG to 1 is 6.
                 *       - Searchs for src_rmt is 1.
                 *       - If src_rmt is 1, VBG is 18.
                 *         - dst-VBG to 1 is 18.
                 *  2.
                 *     dst_rmt == 3
                 *       - src-VBG to 3 is 18.
                 *       - Searchs for src_rmt is 3.
                 *       - If src_rmt is 3, VBG is 19.
                 *         - dst-VBG to 3 is 19.
                 *  3.
                 *     dst_rmt == 7
                 *       - src-VBG to 7 is 19.
                 *       - Searchs for src_rmt is 7.
                 *       - If src_rmt is 7, VBG is 6.
                 *         - dst-VBG to 7 is 6.
                 */
                if (sendseq->sendinfo.index == recvseq->recvinfo.index) {
                    found = true;
                    dst_id =(utofu_bg_id_t)0;
                    GET_BGID_FROM_VBGID(recvseq->vbg_id,dst_id);
                    break;
                }
                recvseq = recvseq->nextptr;
            }
            if (!found) {
                /* internal error */
                return UTF_ERR_INTERNAL;
            }

            /* sanity check */
            /*
             * Whether the physical 6D addresses are the same
             * in sendseq-> sendinfo.phy_6d_addr and
             * rankset [sendseq-> sendinfo.index]?
             */
            CHECK_PHY_6D_ADDR_SEQ(found,sendseq->sendinfo,
                utf_info.vname[rankset[sendseq->sendinfo.index]].xyzabc);
            if (!found) {
                /* internal error */
                return UTF_ERR_INTERNAL;
            }
            /*
             * Whether the physical 6D addresses are the same
             * in recvseq->recvinfo.phy_6d_addr and
             *rankset[sendseq->sendinfo.index]?
             */
            CHECK_PHY_6D_ADDR_SEQ(found,recvseq->recvinfo,
                utf_info.vname[rankset[recvseq->recvinfo.index]].xyzabc);
            if (!found) {
                /* internal error */
                return UTF_ERR_INTERNAL;
            }

            bginfo[sendseq->sendinfo.index] = (utf_bg_info_t)0;
                /* sanity check */
                /* Whether the physical 6D addresses are the same
                 * in sendseq->vbg_id and own?
                 */
                GET_NODEID_FROM_VCQ(temp_nodeid,sendseq->vbg_id);
                if (temp_nodeid != my_nodeid) {
                    /* internal error */
                    return UTF_ERR_INTERNAL;
                }
                /* Whether the physical 6D addresses are the same
                 * in recvseq->vbg_id and own?
                 */
                GET_NODEID_FROM_VCQ(temp_nodeid,recvseq->vbg_id);
                if (temp_nodeid != my_nodeid) {
                    /* internal error */
                    return UTF_ERR_INTERNAL;
                }

                /* Whether tni in sendseq->vbg_id is the same as
                 * the argument tni? */
                GET_TNI_FROM_VBGID(sendseq->vbg_id,temp_tni);
                if (temp_tni != my_tni) {
                    /* internal error */
                    return UTF_ERR_INTERNAL;
                }
                /* Whether tni in recvseq->vbg_id is the same as
                 * the argument tni? */
                GET_TNI_FROM_VBGID(recvseq->vbg_id,temp_tni);
                if (temp_tni != my_tni) {
                    /* internal error */
                    return UTF_ERR_INTERNAL;
                }

                SET_ENABLE_BGINFO(bginfo[sendseq->sendinfo.index],
                    my_tni,
                    utf_info.vname[rankset[my_index]].xyzabc.s.c,
                    utf_info.vname[rankset[my_index]].xyzabc.s.b,
                    utf_info.vname[rankset[my_index]].xyzabc.s.a,
                    utf_info.vname[rankset[my_index]].xyzabc.s.z,
                    utf_info.vname[rankset[my_index]].xyzabc.s.y,
                    utf_info.vname[rankset[my_index]].xyzabc.s.x,
                    src_id,
                    dst_id);

#if defined(DEBUGLOG)
           fprintf(stderr,"DEBUGLOG %s:%s:my_rankset_index=%zu: sendseq=%p vbg=%016lx :recvseq=%p vbg=%016lx : bginfo index=%zu bginfo=%016lx [src=%x dst=%x 6daddr=%02u-%02u-%02u-%1u-%1u-%1u tni=%d] \n"
               ,my_hostname,__func__,my_rankset_index
               ,sendseq,sendseq->vbg_id,recvseq,recvseq->vbg_id
               ,sendseq->sendinfo.index,bginfo[sendseq->sendinfo.index]
               ,src_id,dst_id
               ,utf_info.vname[rankset[my_index]].xyzabc.s.x
               ,utf_info.vname[rankset[my_index]].xyzabc.s.y
               ,utf_info.vname[rankset[my_index]].xyzabc.s.z
               ,utf_info.vname[rankset[my_index]].xyzabc.s.a
               ,utf_info.vname[rankset[my_index]].xyzabc.s.b
               ,utf_info.vname[rankset[my_index]].xyzabc.s.c
               ,my_tni
           );
#endif
        }

        sendseq = sendseq->nextptr;
    }

    return UTF_SUCCESS;
}

/*
 * utf_bg_alloc:  Allocates VBGs for a barrier network
 * @param
 *   (IN)     rankset[] An array of ranks in MPI_COMM_WORLD for the processes
 *                      belonging to the barrier network to be created
 *   (IN)     len       Total number of the processes.
 *   (IN)     my_index  Own rank.
 *   (IN)     max_ppn   Maximum number of processes in the node.
 *   (IN)     comm_type Communication method used for intra-node communication.
 *   (IN/OUT) bginfo    Pointer to the array of the VBG information of
 *                      the processes.
 * @return
 *   UTF_SUCCESS
 *   UTF_ERR_????
 */

/* If the shape of the nodes does not fit the barrier communication,
 * sets the barrier communication disabled bit and
 * shape defect type in bginfo.
 *   errcase 1: The number of processes in the node does not meet the required conditions.
 *   errcase 2: The number of processes in the node of all nodes does not match.
 *   errcase 3: Both 1 and 2 above
 *   errcase 4: Not enough nodes
 */
#define SET_DISABLE_ALL(errcase)                                           \
{                                                                          \
    memset((void *)bginfo,0,sizeof(utf_bg_info_t) * len);                  \
    for (i=0UL;i<len;i++) {                                                \
        bginfo[i] = ((uint64_t)1       << UTF_BG_BITSFT_BGINFO_DISABLE) |  \
                     ((uint64_t)errcase << UTF_BG_BITSFT_BGINFO_ERRCASE);  \
    }                                                                      \
}

/* Initializes bginfo */
#define INITIALIZE_BGINFO()                                                \
{                                                                          \
    for (i=0UL;i<len;i++) {                                                \
       bginfo[i] = (utf_bg_info_t)UTF_BG_ALLOC_INIT_BGINFO;                \
    }                                                                      \
}

/* Frees data buffers */
#define FREE_ALLOC()                                       \
{                                                          \
    if (utf_bg_detail_info) { free(utf_bg_detail_info); }  \
    if (intra_node_info)    { free(intra_node_info); }     \
    if (sequenceptr)        { free(sequenceptr); }         \
}

#define SETINFO_CASE_SUCCESS()                                \
{                                                             \
    utf_bg_alloc_count++;                                     \
    if (utf_bg_first_call) {                                  \
        utf_bg_detail_info_first_alloc = utf_bg_detail_info;  \
    }                                                         \
    utf_bg_first_call = false;                                \
}

#if (!defined(TOFUGIJI))
/*
 * Function to get the job ID
 */
static inline uint32_t hash_str(const char *str)
{
    register uint32_t hash = 0;

    while( *str ) {
        hash += *str++;
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }

    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash = (hash + (hash << 15));

    hash &= ~(0x8000);

    return hash;
}
#endif

int utf_bg_alloc(uint32_t rankset[],
                 size_t len,
                 size_t my_index,
                 int max_ppn,
                 int comm_type,
                 utf_bg_info_t *bginfo)
{
    int rc = UTF_SUCCESS;
    int start_axis;
    char *tempval;
    uint8_t errcase=(uint8_t)0;
    utf_bg_alloc_axis_detail_info_t axes_info[UTF_BG_ALLOC_AXIS_NUM_AXES];
    utf_bg_intra_node_detail_info_t *intra_node_info=NULL;
    utf_bf_alloc_butterfly_sequence_info_t *sequenceptr=NULL;
    utf_bf_alloc_butterfly_sequence_info_t *optimizedsequence=NULL;
    size_t i,number_vbgs=0UL;
    utofu_tni_id_t my_tni=UTF_BG_MIN_TNI_NUMBER;

    /*
     * Checks the arguments
     */
    if ( (NULL == (void *)rankset) ||
         (NULL == (void *)bginfo)
       ) {
        return UTF_ERR_INVALID_POINTER;
    }
    if ( (0UL == len)                              ||
         (len <= my_index)                         ||
         (UTF_BG_MAX_PROC_IN_NODE < max_ppn) ||
         (max_ppn <= 0)                            ||
         ((UTF_BG_TOFU != comm_type)&&(UTF_BG_SM != comm_type))
       ) {
        return UTF_ERR_INVALID_NUMBER;
    }
    switch (comm_type) {
        case UTF_BG_TOFU:
        case UTF_BG_SM:
            if (!utf_bg_first_call) {
                if (
                    ((UTF_BG_TOFU == comm_type) &&
                     (!utf_bg_intra_node_barrier_is_tofu))
                    ||
                    ((UTF_BG_SM == comm_type) &&
                     (utf_bg_intra_node_barrier_is_tofu))
                   ) {
                    rc = UTF_ERR_INVALID_NUMBER;
                    return rc;
                }
            }
            break;
    }

#if defined(DEBUGLOG)
    my_rankset_index = my_index;
#endif
    /*
     * Checks environment variables.
     * Only on the first function call.
     */
    if (utf_bg_first_call) {
        utf_bg_intra_node_barrier_is_tofu = (UTF_BG_TOFU == comm_type);
        memset((void *)&utf_bg_mmap_file_info,
               0,sizeof(utf_bg_mmap_file_info_t));
        if (NULL != (tempval=getenv("UTF_BG_DISABLE_UTF_SHM"))) {
            utf_bg_mmap_file_info.disable_utf_shm = true;
            /* Get the jobid for making filename. */
#if (!defined(TOFUGIJI))
            if ( NULL != getenv("PJM_JOBID") ) {
                pmix_proc_t myproc;

                memset((void *)&myproc,0,sizeof(pmix_proc_t));
                if (PMIX_SUCCESS !=  PMIx_Init(&myproc,NULL,0)) {
                    return UTF_ERR_INTERNAL;
                }
                my_jobid = (int)hash_str((const char *)myproc.nspace);
                if (PMIX_SUCCESS != PMIx_Finalize(NULL, 0)) {
                    return UTF_ERR_INTERNAL;
                }
            }
            else {
                /* internal error */
                return UTF_ERR_INTERNAL;
            }
#else
            if (NULL != (tempval=getenv("UTOFU_WORLD_ID"))) {
                my_jobid = atoi(tempval);
            }
            else if (NULL != (tempval=getenv("OMPI_MCA_ess_base_jobid"))) {
                my_jobid = atoi(tempval);
            }
            else if (NULL != (tempval=getenv("OMPI_MCA_orte_ess_jobid"))) {
                my_jobid = atoi(tempval);
            }
            else {
                /* internal error */
                return UTF_ERR_INTERNAL;
            }
#endif
        }
        if (NULL != (tempval=getenv("UTF_BG_ENABLE_THREAD_SAFE"))) {
            utf_bg_alloc_flags = UTOFU_VBG_FLAG_THREAD_SAFE;
        }
        if (NULL != (tempval=getenv("UTF_BG_MAX_INTRA_NODE_PROCS_FOR_HB"))) {
            max_intra_node_procs_for_hb = atoi(tempval);
            if ( (UTF_BG_MAX_PROC_IN_NODE < max_intra_node_procs_for_hb)
                 ||
                 (max_intra_node_procs_for_hb <= 0) ) {
                max_intra_node_procs_for_hb = 
                    UTF_BG_ALLOC_MAX_PROC_IN_NODE_FOR_HB;
            }
        }
#if defined(DEBUGLOG)
        if (NULL != (tempval=getenv("XONLY"))) {
            xonlyflag = 1;
        }
#endif
#if defined(UTF_BG_UNIT_TEST)
        /*
         * Temporary macro for debugging.
         * For testing until merged with other UTF functions.
         */
        if (NULL != (tempval=getenv("UTF_DEBUG"))) {
            if (!strncmp(tempval, "0x", 2)) {
                sscanf(tempval, "%x", &utf_dflag);
            } else {
                utf_dflag = atoi(tempval);
            }
        }
        mypid = my_index;
#endif
        gethostname(my_hostname,64);
    }

    start_axis=UTF_BG_ALLOC_AXIS_IN_NODE;
    if (!utf_bg_intra_node_barrier_is_tofu) { start_axis=UTF_BG_ALLOC_AXIS_ON_X; }

    utf_bg_detail_info =
       (utf_coll_group_detail_t *)malloc(sizeof(utf_coll_group_detail_t));
    if (NULL == utf_bg_detail_info) {
        FREE_ALLOC();
        rc = UTF_ERR_OUT_OF_MEMORY;
        return rc;
    }
    memset((void *)utf_bg_detail_info,0,sizeof(utf_coll_group_detail_t));

    utf_bg_detail_info->arg_info.rankset  = rankset;
    utf_bg_detail_info->arg_info.len      = len;
    utf_bg_detail_info->arg_info.my_index = my_index;

#if defined(DEBUGLOG)
#if defined(UTF_BG_UNIT_TEST)
/*
 * Temporary macro for debugging.
 * For testing until merged with other UTF functions.
 */
    fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu: mypid=%d utf_dflag=%x \n"
        ,my_hostname,__func__,my_index
        ,mypid,utf_dflag
    );
#endif
    fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu: len=%zu my_jobid=%d utf_bg_first_call=%d utf_bg_alloc_count=%lu utf_bg_intra_node_barrier_is_tofu=%d : utf_bg_alloc_flags=%lu max_intra_node_procs_for_hb=%d xonlyflag=%d disable_utf_shm=%d \n"
        ,my_hostname,__func__,my_index
        ,len,my_jobid
        ,utf_bg_first_call,utf_bg_alloc_count
        ,utf_bg_intra_node_barrier_is_tofu
        ,utf_bg_alloc_flags,max_intra_node_procs_for_hb,xonlyflag
        ,utf_bg_mmap_file_info.disable_utf_shm
    );
    fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu: max_ppn=%d comm_type=%d \n"
        ,my_hostname,__func__,my_index
        ,max_ppn,comm_type
    );
#endif

    /*
     * Information for each axis
     */
    memset((void *)(&axes_info[0]),0,
        sizeof(utf_bg_alloc_axis_detail_info_t) * UTF_BG_ALLOC_AXIS_NUM_AXES);
    rc = make_axes_and_intra_node_infos(rankset,len,my_index,
                                        max_ppn,
                                        &axes_info[0],&intra_node_info,
                                        &errcase);
#if defined(DEBUGLOG)
    fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu: make_axes_and_intra_node_infos=%d intra_node_info=%p errcase=%x \n"
        ,my_hostname,__func__,my_index
        ,rc,intra_node_info,errcase
    );
#endif
    switch (rc) {
        case UTF_SUCCESS:
            utf_bg_detail_info->intra_node_info = intra_node_info;
            break;
        case UTF_BG_ALLOC_ERR_UNMATCH_PROC_PER_NODE:
        case UTF_BG_ALLOC_ERR_TOO_MANY_PROC_IN_NODE:
        case UTF_BG_ALLOC_ERR_TOO_FEW_NODE:
            utf_bg_detail_info->intra_node_info = intra_node_info;
            if (!errcase) {
                /* internal error */
                FREE_ALLOC();
                return UTF_ERR_INTERNAL;
            }
            break;
        default:
            return rc;
    }

    /*
     * Gets shared memory
     */
    if (utf_bg_first_call) {
        int rcmmap;
        errno = 0;
        rcmmap = make_mmap_file(
                                rankset[my_index],
                                intra_node_info);
#if defined(DEBUGLOG)
        fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu: make_mmap_file=%d disable_utf_shm=%d file_name=%s size=%zu width=%zu fd=%d mmaparea=%p \n"
            ,my_hostname,__func__,my_index
            ,rcmmap
            ,utf_bg_mmap_file_info.disable_utf_shm
            ,utf_bg_mmap_file_info.file_name
            ,utf_bg_mmap_file_info.size
            ,utf_bg_mmap_file_info.width
            ,utf_bg_mmap_file_info.fd
            ,utf_bg_mmap_file_info.mmaparea
        );
#endif
        if (UTF_SUCCESS != rcmmap) {
            FREE_ALLOC();
            return rcmmap;
        }
    }

    /*
     * If the first call to the utf_bg_alloc function fails
     * to allocate VBGs for MPI_COMM_WORLD,
     * the utf_bg_init function will always return with the return value
     * UTF_ERR_RESOURCE_BUSY thereafter.
     */
    if ( (!utf_bg_first_call) &&
         (utf_bg_mmap_file_info.first_alloc_is_failed) ) {
#if defined(DEBUGLOG)
        fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu: utf barrier communication is invalid \n"
            ,my_hostname,__func__,my_index
        );
#endif
        SET_DISABLE_ALL(errcase);
        utf_bg_alloc_count++;
        return UTF_SUCCESS;
    }

    /*
     * Checks the number of processes
     */
    if (len < 4UL) {
        utf_bg_detail_info->intra_node_info = intra_node_info;
        errcase |= UTF_BG_ALLOC_TOO_FEW_NODE;
        SET_DISABLE_ALL(errcase);
        SETINFO_CASE_SUCCESS();
        return UTF_SUCCESS;
    }

    if ( (max_intra_node_procs_for_hb < max_ppn) &&
         (utf_bg_intra_node_barrier_is_tofu) ) {
        errcase |= UTF_BG_ALLOC_TOO_MANY_PROC;
        SET_DISABLE_ALL(errcase);
        SETINFO_CASE_SUCCESS();
        return UTF_SUCCESS;
    }

    /*
     * Whether the information for each axis was created successfully?
     */
    if (UTF_SUCCESS != rc) {
        /*
         * case UTF_BG_ALLOC_ERR_UNMATCH_PROC_PER_NODE:
         * case UTF_BG_ALLOC_ERR_TOO_MANY_PROC_IN_NODE:
         * case UTF_BG_ALLOC_ERR_TOO_FEW_NODE:
         */
        SET_DISABLE_ALL(errcase);
        SETINFO_CASE_SUCCESS();
        return UTF_SUCCESS;
    }

    if ( (UTF_BG_INTRA_NODE_WORKER == intra_node_info->state) &&
         (!utf_bg_intra_node_barrier_is_tofu) ) {
        /*
         * If shared memory communication is specified in the node,
         * UTF_BG_INTRA_NODE_WORKER does not need to set a barrier gate.
         * Sets just the necessary information and returns.
         */
        INITIALIZE_BGINFO();
        SETINFO_CASE_SUCCESS();
        return rc;
    }

    /*
     * Builds a butterfly network
     */
    rc = make_butterfly_network(&axes_info[0],start_axis,
                                &sequenceptr, /* Pointer to freeing memory */
                                &optimizedsequence,
                                        /*  Pointer to the optimized part */
                                &number_vbgs
                           );
#if defined(DEBUGLOG)
    fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu: make_butterfly_network=%d sequenceptr=%p optimizedsequence=%p number_vbgs=%zu \n"
        ,my_hostname,__func__,my_index
        ,rc,sequenceptr,optimizedsequence,number_vbgs
    );
#endif
    if (UTF_SUCCESS != rc) {
        FREE_ALLOC();
        return rc;
    }

    /*
     * Allocates gates
     */
    rc = allocate_vbg(number_vbgs,intra_node_info,optimizedsequence,&my_tni);
#if defined(DEBUGLOG)
    fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu: allocate_vbg=%d my_tni=%d \n"
        ,my_hostname,__func__,my_index
        ,rc,my_tni
    );
#endif
    if ( (UTF_SUCCESS != rc) && (UTF_ERR_FULL != rc) ) {
        FREE_ALLOC();
        return rc;
    }
#if defined(DEBUGLOG)
    fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu:INTRAINFO ptr=%p state=%ld size=%zu intra_index=%zu curr_seq=%lu mmap_buf=%p \n"
        ,my_hostname,__func__,my_index
        ,utf_bg_detail_info->intra_node_info
        ,utf_bg_detail_info->intra_node_info->state
        ,utf_bg_detail_info->intra_node_info->size
        ,utf_bg_detail_info->intra_node_info->intra_index
        ,utf_bg_detail_info->intra_node_info->curr_seq
        ,utf_bg_detail_info->intra_node_info->mmap_buf
    );
#endif

    /*
     * Creates bginfo
     */
    INITIALIZE_BGINFO();
    if (UTF_SUCCESS ==rc) {
        rc = make_bginfo(my_tni,optimizedsequence,rankset,my_index,bginfo);
#if defined(DEBUGLOG)
        fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu: make_bginfo=%d \n"
            ,my_hostname,__func__,my_index
            ,rc
    );
#endif
        if (UTF_SUCCESS != rc) {
            FREE_ALLOC();
            return rc;
        }
    }
    else {
        errcase=0;
        SET_DISABLE_ALL(errcase);
        rc = UTF_SUCCESS; 
#if defined(DEBUGLOG)
        fprintf(stderr,"DEBUGLOG %s:%s:my_index=%zu: UTOFU_ERR_FULL \n"
            ,my_hostname,__func__,my_index
        );
#endif
    } 
    DEBUG(DLEVEL_PROTO_VBG) {
        char msg[256];
        for (i=0UL;i<len;i++) {
            snprintf(msg, 256,"%s: %s: count=%lu my_index=%zu len=%zu/%zu rankset=%016lx bginfo=%016lx \n"
                ,__func__,my_hostname,utf_bg_alloc_count
                ,my_index,i,len
                ,(long unsigned int)rankset[i]
                ,bginfo[i]);
            utf_printf("%s",msg);
        }
    }

    SETINFO_CASE_SUCCESS();
    return rc;
}
