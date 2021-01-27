#ifndef _SNDMGT_DEF_H
#define _SNDMGT_DEF_H
#include <stdint.h>
#include <string.h>

typedef struct utf_sndmgt { /* 3 byte */
    uint8_t	examed:1,
		sndok:1,
		mode:6; /* 1B */
    uint8_t	index;	/* 1B: idx: index of receiver's buf array */
    uint8_t	count;		/* 1 B */
} utf_sndmgt_t;


static inline void utf_sndmgt_clr_examed(int pos, utf_sndmgt_t *bp)
{
    bp[pos].examed = 0;
}

static inline void utf_sndmgt_clr_sndok(int pos, utf_sndmgt_t *bp)
{
    bp[pos].sndok = 0;
}

static inline int utf_sndmgt_isset_examed(int pos, utf_sndmgt_t *bp)
{
    return bp[pos].examed == 1;
}

static inline int utf_sndmgt_isset_sndok(int pos, utf_sndmgt_t *bp)
{
    return bp[pos].sndok == 1;
}

static inline void utf_sndmgt_set_examed(int pos, utf_sndmgt_t *bp)
{
    bp[pos].examed = 1;
}

static inline void utf_sndmgt_set_sndok(int pos, utf_sndmgt_t *bp)
{
    bp[pos].sndok = 1;
}

static inline void utf_sndmgt_set_index(int pos, utf_sndmgt_t *bp, unsigned index)
{
    bp[pos].index = index;
}

static inline unsigned utf_sndmgt_get_index(int pos, utf_sndmgt_t *bp)
{
    return bp[pos].index;
}

static inline int
utf_sndmgt_get_smode(int pos, utf_sndmgt_t *bp)
{
    return bp[pos].mode;
}

static inline int
utf_sndmgt_set_smode(int pos, utf_sndmgt_t *bp, unsigned mode)
{
    return bp[pos].mode = mode;
}

static inline int
sndmg_update_chainmode(int pos, utf_sndmgt_t *bp)
{
    bp[pos].count++;
    if (bp[pos].count >= TRANSMODE_THR) {
	bp[pos].mode = TRANSMODE_AGGR;
    }
    return bp[pos].mode;
}

#endif /* ~_SNDMGT_DEF_H */
