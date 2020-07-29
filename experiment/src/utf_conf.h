#include <assert.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef UTF_DEBUG
#define DEBUG(mask) if (dflag&(mask))
#else
#define DEBUG(mask) if (0)
#endif
#define DLEVEL_PMIX		0x1
#define DLEVEL_UTOFU		0x2
#define DLEVEL_PROTOCOL		0x4
#define DLEVEL_PROTO_EAGER	0x8
#define DLEVEL_PROTO_RENDEZOUS	0x10
#define DLEVEL_ALL		0xffff

/* 
 * MTU 1920 Byte, Piggyback size 32 Byte, Assume payload(1888 Byte)
 * 6.8 GB/s, 0.147 nsec/B
 * Rendezvous
 *	Time(B) = 2 + 0.147*B nsec, 
 *	e.g., T(1MiB) = 154.143 usec, 1MiB/154.143usec = 6.487 GiB/s
 *	    actual: 6.35 GiB/s/MB
 * Eager
 *	T(1888 B) = 1 + 0.147*1920 = 184.2 nsec, 1888B/184.2nsec = 10.2 MB/s
 */
/*
 *   165,888 node = 24 x 24 x 24 x 12
 *   663,552 process = 648 Ki, 20 bit
 * 7,962,624 process = 7.6 Mi, 23 bit
 */
/*
 *  Cache Line Size (https://www.7-cpu.com/)
 *	Skylake&KNL: 64 Byte
 *	A64FX: 512 Byte
 */

#define MSG_SIZE	1920
#define MSG_PEERS	12	/* must be smaller than 2^8 (edata) */
#define TOFU_ALIGN	256
#define TAG_SNDCTR	12
#define TAG_ERBUF	13
#define TAG_EGRMGT	14
#define SND_EGR_BUFENT	128
#define SND_CNTR_SIZE	256
#define REQ_SIZE	512

