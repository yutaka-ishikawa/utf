/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "pmix.h"
#include "pmix_fjext.h"
#include "jtofu.h"
#include "utofu.h"

#define COND_VERBOSE	1

extern int  main_0001(unsigned int j_id, int rank, int size);
extern int  main_0002(unsigned int j_id, int rank, int size);

/* derived from pmix/src/include/hash_string.h */
static inline uint32_t main_pmix_hash_str(const char *str)
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

    return hash;
}

static inline uint32_t main_pmix_hash_str2(const char *str)
{
    register uint32_t hash = 0; /* XXX */
    const char *kp = "UTOFU_WORLD_ID";
    const char *cp = getenv(kp); char *p = 0; long lv;

    if (cp != 0) { /* XXX if tofu simulater */
	lv = strtol(cp, &p, 0);
	if ((p != 0) && (p > cp) && (lv >= 0)) {
	    hash = (uint32_t)lv; 
	}
    }
    else {
	hash = main_pmix_hash_str(str);
    }

    return hash;
}

int main(int argc, char *argv[], char *environ[])
{
    pmix_status_t rc = PMIX_SUCCESS; int el = 0;
    pmix_proc_t *myproc = 0, myproc_save[1];
    unsigned int pmii_j_id = -1U;
    int pmii_rank = -1;
    int pmii_size = 0;
    void *pmii_rmap = 0, *pmii_rmap_free = 0;
    int jtofu_inited = 0;

    /* init */
    {
	pmix_info_t *info = 0;
	size_t ninfo = 0;
	
	rc = PMIx_Init(myproc_save, info, ninfo);
	if (rc != PMIX_SUCCESS) { el = __LINE__; goto bad; }
	myproc = myproc_save;
	if (COND_VERBOSE) {
	    printf("%03d\tnamespace \"%s\"\n",
		myproc->rank, myproc->nspace);
	    fflush(stdout);
	}
	/* j_id */
	pmii_j_id = main_pmix_hash_str2(myproc->nspace);
	if (COND_VERBOSE) {
	    printf("%03d\thash 0x%x\n", myproc->rank, pmii_j_id);
	    fflush(stdout);
	}
    }

    /* rank */
    {
#ifdef	PMIX_RANK
	pmix_value_t *v = 0;
	pmix_info_t *info = 0;
	size_t ninfo = 0;

	rc = PMIx_Get(myproc, PMIX_RANK, info, ninfo, &v);
	if (rc == PMIX_ERR_NOT_FOUND) {
	    /* Fujitsu libpmix does not support PMIX_RANK */
	    pmii_rank = myproc->rank;
	}
	else if (rc == PMIX_SUCCESS) {
	    assert(v->type == PMIX_PROC_RANK);
	    pmii_rank = v->data.rank;
	    PMIX_VALUE_RELEASE(v);
	    assert(pmii_rank == myproc->rank);
	}
	else {
	    el = __LINE__; goto bad;
	}
#else
	pmii_rank = myproc->rank;
#endif
    }
    /* size */
    {
	pmix_proc_t proc[1];
	pmix_value_t *v = 0;
	pmix_info_t *info = 0;
	size_t ninfo = 0;

	/* PMIX_PROC_LOAD(proc, myproc->nspace, PMIX_RANK_WILDCARD);  */
	strncpy(proc->nspace, myproc->nspace, PMIX_MAX_NSLEN);
	proc->rank = PMIX_RANK_WILDCARD;

	rc = PMIx_Get(proc, PMIX_JOB_SIZE, info, ninfo, &v);
	if (rc != PMIX_SUCCESS) { el = __LINE__; goto bad; }
	assert(v->type == PMIX_UINT32);
	pmii_size = v->data.uint32;
	PMIX_VALUE_RELEASE(v);
	assert(pmii_size > 0);
    }

    /* rank-map */
    {
	pmix_proc_t proc[1];
	pmix_value_t *v = 0;
	pmix_info_t *info = 0;
	size_t ninfo = 0;

	/* PMIX_PROC_LOAD(proc, myproc->nspace, PMIX_RANK_WILDCARD);  */
	strncpy(proc->nspace, myproc->nspace, PMIX_MAX_NSLEN);
	proc->rank = PMIX_RANK_WILDCARD;

	rc = PMIx_Get(proc, FJPMIX_RANKMAP, info, ninfo, &v);
	if (rc != PMIX_SUCCESS) { el = __LINE__; goto bad; }
	if (v->type == PMIX_UNDEF) {
	    PMIX_VALUE_RELEASE(v);
	    rc = PMIX_ERROR;
	    el = __LINE__; goto bad;
	}
	else if (v->type == PMIX_BYTE_OBJECT) {
	    pmii_rmap = v->data.bo.bytes;
	    pmii_rmap_free = v->data.bo.bytes;
	    if (COND_VERBOSE) {
		printf("FJPMIX_RANKMAP bytes %p size %ld\n",
		    v->data.bo.bytes, v->data.bo.size);
		fflush(stdout);
	    }
	    PMIX_BYTE_OBJECT_CONSTRUCT(&v->data.bo);
	}
	else {
	    PMIX_VALUE_RELEASE(v);
	    rc = PMIX_ERROR;
	    el = __LINE__; goto bad;
	}
	PMIX_VALUE_RELEASE(v);
	assert(pmii_rmap != 0);
    }
    if (COND_VERBOSE) {
	printf("%03d\tPMIX  rmap %p\n", pmii_rank, pmii_rmap);
	fflush(stdout);
    }

    /* initialize */
    {
        jtofu_job_id_t j_id = pmii_j_id;
        jtofu_rank_t rank = pmii_rank;
        void *mapinfo = pmii_rmap;

        rc = jtofu_initialize(j_id, rank, mapinfo);
        if (rc != 0) { el = __LINE__; goto bad; }
	jtofu_inited = 1;
    }

    rc = main_0001(pmii_j_id, pmii_rank, pmii_size);
    if (rc != 0) { el = __LINE__; goto bad; }

    rc = main_0002(pmii_j_id, pmii_rank, pmii_size);
    if (rc != 0) { el = __LINE__; goto bad; }

bad:
    if (el != 0) {
	fprintf(stderr, "ERROR %d\t%s:%s:%d\n", rc, __func__, __FILE__, el);
	fflush(stderr);
    }
    if (jtofu_inited != 0) {
        int rrc = jtofu_finalize();
        if (rrc != 0) {
            fprintf(stderr, "WARNING %d\t%s:%d\n", rrc, __FILE__, __LINE__);
            fflush(stderr);
        }
    }
    if (pmii_rmap_free != 0) {
	free(pmii_rmap_free);
    }
    if (myproc != 0) {
	pmix_info_t *info = 0;
	size_t ninfo = 0;
	pmix_status_t rrc = PMIx_Finalize(info, ninfo);
	if (rrc != PMIX_SUCCESS) {
	    /* Warning */
	    /* el = __LINE__; goto bad; */
	}
    }
    return (rc != 0)? 1: 0;
}

int main_0001(unsigned int j_id, int rank, int size)
{
    int uc = UTOFU_SUCCESS, el = 0;

    /* version */
    {
	int vmaj = -1, vmin = -1;
	utofu_query_utofu_version(&vmaj, &vmin);
	printf("%03d\t%s: version utofu inc %d.%d lib %d.%d\n", rank, __func__,
	    UTOFU_VERSION_MAJOR, UTOFU_VERSION_MINOR, vmaj, vmin);

	vmaj = -1; vmin = -1;
	utofu_query_tofu_version(&vmaj, &vmin);
	printf("%03d\t%s: version tofhw %d.%d\n", rank, __func__, vmaj, vmin);
    }

/* bad: */
    if (el != 0) {
	fprintf(stderr, "ERROR %d\t%s:%s:%d\n", uc, __func__, __FILE__, el);
	fflush(stderr);
    }
    return uc;
}

int main_0002(unsigned int j_id, int rank, int size)
{
    int uc = UTOFU_SUCCESS, el = 0;
    utofu_tni_id_t *tnis = 0;
    size_t ntni = 0;
    utofu_vcq_hdl_t vcqh = -1UL;

    /* tnis */
    {
	size_t itni;

	uc = utofu_get_onesided_tnis(&tnis, &ntni);
	if (uc != UTOFU_SUCCESS) { el = __LINE__; goto bad; }

	printf("%03d\t%s: ntni %ld\n", rank, __func__, ntni);
	printf("%03d\ttni ", rank);
	for (itni = 0; itni < ntni; itni++) {
	    printf("%02ld %d ", itni, tnis[itni]);
	}
	printf("\n");
    }
    /*  create vcqh */
    {
	utofu_tni_id_t tni;
	unsigned long int flg = 0;

	if (ntni <= 0) { uc = UTOFU_ERR_FATAL; el = __LINE__; goto bad; }
	tni = tnis[0];

	uc = utofu_create_vcq(tni, flg, &vcqh);
	if (uc != UTOFU_SUCCESS) { el = __LINE__; goto bad; }
    }
    {
	utofu_vcq_id_t vcqi = 0;
	uint8_t xyz[8];
	utofu_tni_id_t tni[1];
	utofu_cq_id_t tcq[1];
	uint16_t cid[1];

	uc = utofu_query_vcq_id(vcqh, &vcqi);
	if (uc != UTOFU_SUCCESS) { el = __LINE__; goto bad; }

	uc = utofu_query_vcq_info(vcqi, xyz, tni, tcq, cid);
	if (uc != UTOFU_SUCCESS) { el = __LINE__; goto bad; }

	printf("%03d\t%s: vcq addr %d.%d.%d.%d - %d %d %d\n",
	    rank, __func__,
	    xyz[0], xyz[1], xyz[2], xyz[3] * 100 +  xyz[4] * 10 + xyz[5],
	    tni[0], tcq[0], cid[0]);
    }
    {
	uint8_t xyz[8];

	uc = utofu_query_my_coords(xyz);
	if (uc != UTOFU_SUCCESS) { el = __LINE__; goto bad; }

	printf("%03d\t%s: my  addr %d.%d.%d.%d\n",
	    rank, __func__,
	    xyz[0], xyz[1], xyz[2], xyz[3] * 100 + xyz[4] * 10 + xyz[5]);
    }

bad:
    if (el != 0) {
	fprintf(stderr, "ERROR %d\t%s:%s:%d\n", uc, __func__, __FILE__, el);
	fflush(stderr);
    }
    if (vcqh != -1UL) {
	int uc_i = utofu_free_vcq(vcqh);
	if (uc_i != UTOFU_SUCCESS) {
	    fprintf(stderr, "ERROR %d\t%s:%s:%d\n", uc_i,
		__func__, __FILE__, __LINE__);
	    fflush(stderr);
	}
    }
    if (tnis != 0) {
	free(tnis); tnis = 0;
    }
    return uc;
}
