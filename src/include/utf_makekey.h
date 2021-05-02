/*
 * stadd examples
 *			: 0x0010000000009570LL for 0x000000e99570
 *			: 0x00100200001f65f0LL for 0x000003df65f0
 *			: 0x0010020000153510LL for 0x000004153510
 *			: 0x00103e0000000010LL for 0x40000f400010
 ****
 * stadd address        : 0xfffffff000000000LL
 * virtual address range: 0x0000000fffffffffLL
 */
#define MASK_STADDVAL	0xffffff0000000000LL
#define MASK_VIRTADDR	0x0000000fffffffffLL
#define MASK_ENCTYPE	0x000000f000000000LL
#define MASK_KEY0	0x000000000000ffffLL
#define MASK_KEY1	0x00000000001fffffLL
#define KEY_ENCRULE1	0x0000001000000000LL

static inline uint64_t
make_key(uint64_t stadd, char *virt)
{
    uint64_t	key;

    key = (stadd & MASK_STADDVAL)
	| ((uint64_t)virt & MASK_VIRTADDR);
    /*
     * encoding rules: using lower
     *	16bit (0xffffLL) or 21bit (0x1fffffLL)
     */
    if ((stadd & MASK_KEY1) == ((uint64_t) virt & MASK_KEY1)) {
	/* second rule */
	key |= KEY_ENCRULE1;
    } else if ((stadd & MASK_VIRTADDR) == ((uint64_t) virt & MASK_KEY0)) {
	/* first rule */
	;
    } else {
	fprintf(stderr, "Unknown stadd encapsulation: stadd=0x%lx\n", stadd);
    }
    return key;
}

static inline uint64_t
calc_stadd(uint64_t key, char *virt)
{
    uint64_t	base, stadd, off;
    base = key & MASK_VIRTADDR;
    off = ((uint64_t)virt & MASK_VIRTADDR) -  base;
    if ((key & MASK_ENCTYPE) == KEY_ENCRULE1) {
	stadd = key & (MASK_STADDVAL|MASK_KEY1);
    } else {
	stadd = key & (MASK_STADDVAL|MASK_KEY0);
    }
    stadd += off;
    return stadd;
}

static inline uint64_t
extrct_stadd(uint64_t key)
{
    uint64_t	stadd;
    stadd = ((key & MASK_ENCTYPE) == KEY_ENCRULE1) ?
	(key & (MASK_STADDVAL | MASK_KEY1))
	: (key & (MASK_STADDVAL | MASK_KEY0));
    return stadd;
}
