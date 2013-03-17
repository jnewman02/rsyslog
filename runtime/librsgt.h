/* librsgt.h - rsyslog's guardtime support library
 *
 * Copyright 2013 Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef INCLUDED_LIBRSGT_H
#define INCLUDED_LIBRSGT_H
#include <gt_base.h>

/* Max number of roots inside the forest. This permits blocks of up to
 * 2^MAX_ROOTS records. We assume that 64 is sufficient for all use
 * cases ;) [and 64 is not really a waste of memory, so we do not even
 * try to work with reallocs and such...]
 */
#define MAX_ROOTS 64
#define LOGSIGHDR "LOGSIG10"

/* context for gt calls. This primarily serves as a container for the
 * config settings. The actual file-specific data is kept in gtfile.
 */
struct gtctx_s {
	enum GTHashAlgorithm hashAlg;
	uint8_t bKeepRecordHashes;
	uint8_t bKeepTreeHashes;
	uint64_t blockSizeLimit;
	char *timestamper;
};
typedef struct gtctx_s *gtctx;

/* this describes a file, as far as librsgt is concerned */
struct gtfile_s {
	gtctx ctx;
	/* the following data items are mirrored from gtctx to
	 * increase cache hit ratio (they are frequently accesed).
	 */
	enum GTHashAlgorithm hashAlg;
	uint8_t bKeepRecordHashes;
	uint8_t bKeepTreeHashes;
	/* end mirrored properties */
	uint64_t blockSizeLimit;
	uint8_t *IV; /* initial value for blinding masks */
	GTDataHash *x_prev; /* last leaf hash (maybe of previous block) --> preserve on term */
	unsigned char *sigfilename;
	unsigned char *statefilename;
	int fd;
	unsigned char *blkStrtHash; /* last hash from previous block */
	uint16_t lenBlkStrtHash;
	uint64_t nRecords;  /* current number of records in current block */
	uint64_t bInBlk;    /* are we currently inside a blk --> need to finish on close */
	int8_t nRoots;
	/* algo engineering: roots structure is split into two arrays
	 * in order to improve cache hits.
	 */
	int8_t roots_valid[MAX_ROOTS];
	GTDataHash *roots_hash[MAX_ROOTS];
	/* data members for the associated TLV file */
	char	tlvBuf[4096];
	int	tlvIdx; /* current index into tlvBuf */
};
typedef struct gtfile_s *gtfile;

typedef struct imprint_s imprint_t;
typedef struct block_sig_s block_sig_t;

struct imprint_s {
	uint8_t hashID;
	int	len;
	uint8_t *data;
};

#define SIGID_RFC3161 0
struct block_sig_s {
	uint8_t hashID;
	uint8_t sigID; /* what type of *signature*? */
	uint8_t *iv;
	imprint_t lastHash;
	uint64_t recCount;
	struct {
		struct {
			uint8_t *data;
			size_t len; /* must be size_t due to GT API! */
		} der;
	} sig;
};


/* the following defines the gtstate file record. Currently, this record
 * is fixed, we may change that over time.
 */
struct rsgtstatefile {
	char hdr[8];	/* must be "GTSTAT10" */
	uint8_t hashID;
	uint8_t lenHash;
	/* after that, the hash value is contained within the file */
};

/* error states */
#define RSGTE_IO 1 	/* any kind of io error */
#define RSGTE_FMT 2	/* data fromat error */
#define RSGTE_INVLTYP 3	/* invalid TLV type record (unexcpected at this point) */
#define RSGTE_OOM 4	/* ran out of memory */
#define RSGTE_LEN 5	/* error related to length records */
#define RSGTE_NO_BLKSIG 6/* block signature record is missing --> invalid block */
#define RSGTE_INVLD_RECCNT 7/* mismatch between actual records and records
                               given in block-sig record */
#define RSGTE_INVLHDR 8/* invalid file header */
#define RSGTE_EOF 9 	/* specific EOF */
#define RSGTE_MISS_REC_HASH 10 /* record hash missing when expected */
#define RSGTE_MISS_TREE_HASH 11 /* tree hash missing when expected */
#define RSGTE_INVLD_REC_HASH 12 /* invalid record hash (failed verification) */
#define RSGTE_INVLD_TREE_HASH 13 /* invalid tree hash (failed verification) */
#define RSGTE_INVLD_REC_HASHID 14 /* invalid record hash ID (failed verification) */
#define RSGTE_INVLD_TREE_HASHID 15 /* invalid tree hash ID (failed verification) */
#define RSGTE_MISS_BLOCKSIG 16 /* block signature record missing when expected */
#define RSGTE_INVLD_TIMESTAMP 17 /* RFC3161 timestamp is invalid */


static inline uint16_t
hashOutputLengthOctets(uint8_t hashID)
{
	switch(hashID) {
	case GT_HASHALG_SHA1:	/* paper: SHA1 */
		return 20;
	case GT_HASHALG_RIPEMD160: /* paper: RIPEMD-160 */
		return 20;
	case GT_HASHALG_SHA224:	/* paper: SHA2-224 */
		return 28;
	case GT_HASHALG_SHA256: /* paper: SHA2-256 */
		return 32;
	case GT_HASHALG_SHA384: /* paper: SHA2-384 */
		return 48;
	case GT_HASHALG_SHA512:	/* paper: SHA2-512 */
		return 64;
	default:return 32;
	}
}

static inline uint8_t
hashIdentifier(uint8_t hashID)
{
	switch(hashID) {
	case GT_HASHALG_SHA1:	/* paper: SHA1 */
		return 0x00;
	case GT_HASHALG_RIPEMD160: /* paper: RIPEMD-160 */
		return 0x02;
	case GT_HASHALG_SHA224:	/* paper: SHA2-224 */
		return 0x03;
	case GT_HASHALG_SHA256: /* paper: SHA2-256 */
		return 0x01;
	case GT_HASHALG_SHA384: /* paper: SHA2-384 */
		return 0x04;
	case GT_HASHALG_SHA512:	/* paper: SHA2-512 */
		return 0x05;
	default:return 0xff;
	}
}
static inline char *
hashAlgName(uint8_t hashID)
{
	switch(hashID) {
	case GT_HASHALG_SHA1:
		return "SHA1";
	case GT_HASHALG_RIPEMD160:
		return "RIPEMD-160";
	case GT_HASHALG_SHA224:
		return "SHA2-224";
	case GT_HASHALG_SHA256:
		return "SHA2-256";
	case GT_HASHALG_SHA384:
		return "SHA2-384";
	case GT_HASHALG_SHA512:
		return "SHA2-512";
	default:return "[unknown]";
	}
}
static inline enum GTHashAlgorithm
hashID2Alg(uint8_t hashID)
{
	switch(hashID) {
	case 0x00:
		return GT_HASHALG_SHA1;
	case 0x02:
		return GT_HASHALG_RIPEMD160;
	case 0x03:
		return GT_HASHALG_SHA224;
	case 0x01:
		return GT_HASHALG_SHA256;
	case 0x04:
		return GT_HASHALG_SHA384;
	case 0x05:
		return GT_HASHALG_SHA512;
	default:
		return 0xff;
	}
}
static inline char *
sigTypeName(uint8_t sigID)
{
	switch(sigID) {
	case SIGID_RFC3161:
		return "RFC3161";
	default:return "[unknown]";
	}
}
static inline uint16_t
getIVLen(block_sig_t *bs)
{
	return hashOutputLengthOctets(bs->hashID);
}
static inline void
rsgtSetTimestamper(gtctx ctx, char *timestamper)
{
	free(ctx->timestamper);
	ctx->timestamper = strdup(timestamper);
}
static inline void
rsgtSetBlockSizeLimit(gtctx ctx, uint64_t limit)
{
	ctx->blockSizeLimit = limit;
}
static inline void
rsgtSetKeepRecordHashes(gtctx ctx, int val)
{
	ctx->bKeepRecordHashes = val;
}
static inline void
rsgtSetKeepTreeHashes(gtctx ctx, int val)
{
	ctx->bKeepTreeHashes = val;
}

int rsgtSetHashFunction(gtctx ctx, char *algName);
void rsgtInit(char *usragent);
void rsgtExit(void);
gtctx rsgtCtxNew(void);
gtfile rsgtCtxOpenFile(gtctx ctx, unsigned char *logfn);
void rsgtfileDestruct(gtfile gf);
void rsgtCtxDel(gtctx ctx);
void sigblkInit(gtfile gf);
void sigblkAddRecord(gtfile gf, const unsigned char *rec, const size_t len);
void sigblkFinish(gtfile gf);
/* reader functions */
int rsgt_tlvrdHeader(FILE *fp, unsigned char *hdr);
int rsgt_tlvrd(FILE *fp, uint16_t *tlvtype, uint16_t *tlvlen, void *obj);
void rsgt_tlvprint(FILE *fp, uint16_t tlvtype, void *obj, uint8_t verbose);
void rsgt_printBLOCK_SIG(FILE *fp, block_sig_t *bs, uint8_t verbose);
int rsgt_getBlockParams(FILE *fp, uint8_t bRewind, block_sig_t **bs, uint8_t *bHasRecHashes, uint8_t *bHasIntermedHashes);
int rsgt_chkFileHdr(FILE *fp, char *expect);
gtfile rsgt_vrfyConstruct_gf(void);
void rsgt_vrfyBlkInit(gtfile gf, block_sig_t *bs, uint8_t bHasRecHashes, uint8_t bHasIntermedHashes);
int rsgt_vrfy_nextRec(block_sig_t *bs, gtfile gf, FILE *sigfp, unsigned char *rec, size_t lenRec);
int verifyBLOCK_SIG(block_sig_t *bs, gtfile gf, FILE *sigfp, uint64_t nRecs);

/* TODO: replace these? */
void hash_m(gtfile gf, GTDataHash **m);
void hash_r(gtfile gf, GTDataHash **r, const unsigned char *rec, const size_t len);
void hash_node(gtfile gf, GTDataHash **node, GTDataHash *m, GTDataHash *r, uint8_t level);
#endif  /* #ifndef INCLUDED_LIBRSGT_H */
