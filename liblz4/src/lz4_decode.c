/*
 * Copyright 2024, Unikie
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Minimal LZ4 frame format decoder.
 * Based on LZ4 frame format specification:
 * https://github.com/lz4/lz4/blob/dev/doc/lz4_Frame_format.md
 */

#include <lz4/lz4.h>
#include <string.h>

/* Frame descriptor flags (FLG byte) */
#define FLG_VERSION_MASK     0xC0
#define FLG_VERSION_01       0x40  /* Version 01 (current) */
#define FLG_BLOCK_INDEP      0x20  /* Block independence flag */
#define FLG_BLOCK_CHECKSUM   0x10  /* Block checksum flag */
#define FLG_CONTENT_SIZE     0x08  /* Content size present */
#define FLG_CONTENT_CHECKSUM 0x04  /* Content checksum present */
#define FLG_DICT_ID          0x01  /* Dictionary ID present */

/* Block descriptor (BD byte) */
#define BD_BLOCK_SIZE_MASK   0x70

/* Block header flags */
#define BLOCK_UNCOMPRESSED   0x80000000

/* Minimum match length in LZ4 */
#define MIN_MATCH 4

static inline uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static inline uint64_t read_le64(const uint8_t *p)
{
    return (uint64_t)read_le32(p) |
           ((uint64_t)read_le32(p + 4) << 32);
}

static inline uint16_t read_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

int lz4_is_frame(const void *src, size_t src_size)
{
    if (src_size < 4) {
        return 0;
    }
    return read_le32(src) == LZ4_FRAME_MAGIC;
}

/*
 * Parse LZ4 frame header.
 * Returns header size on success, negative error on failure.
 * If content_size is non-NULL, stores content size there.
 */
static int parse_frame_header(const uint8_t *src, size_t src_size,
                              uint64_t *content_size, uint8_t *flags_out)
{
    if (src_size < 7) {  /* magic(4) + FLG(1) + BD(1) + HC(1) minimum */
        return LZ4_ERROR_HEADER;
    }

    /* Check magic */
    if (read_le32(src) != LZ4_FRAME_MAGIC) {
        return LZ4_ERROR_MAGIC;
    }

    uint8_t flg = src[4];
    uint8_t bd = src[5];
    (void)bd;  /* Block descriptor - not used for decompression */

    /* Check version */
    if ((flg & FLG_VERSION_MASK) != FLG_VERSION_01) {
        return LZ4_ERROR_VERSION;
    }

    if (flags_out) {
        *flags_out = flg;
    }

    int header_size = 7;  /* magic + FLG + BD + HC */

    /* Content size (8 bytes, little endian) */
    if (flg & FLG_CONTENT_SIZE) {
        if (src_size < 15) {
            return LZ4_ERROR_HEADER;
        }
        if (content_size) {
            *content_size = read_le64(src + 6);
        }
        header_size += 8;
    } else if (content_size) {
        *content_size = 0;  /* Not present */
    }

    /* Dictionary ID (4 bytes) - skip if present */
    if (flg & FLG_DICT_ID) {
        header_size += 4;
    }

    if ((size_t)header_size > src_size) {
        return LZ4_ERROR_HEADER;
    }

    return header_size;
}

int64_t lz4_frame_get_content_size(const void *src, size_t src_size)
{
    uint64_t content_size;
    uint8_t flags;

    int ret = parse_frame_header(src, src_size, &content_size, &flags);
    if (ret < 0) {
        return ret;
    }

    if (!(flags & FLG_CONTENT_SIZE)) {
        return LZ4_ERROR_NO_SIZE;
    }

    return (int64_t)content_size;
}

/*
 * Decompress a single LZ4 block.
 * Returns number of bytes written to dst, or negative error.
 */
static int64_t decompress_block(uint8_t *dst, size_t dst_size,
                                const uint8_t *src, size_t src_size)
{
    const uint8_t *src_end = src + src_size;
    const uint8_t *dst_start = dst;
    uint8_t *dst_end = dst + dst_size;

    while (src < src_end) {
        /* Read token */
        uint8_t token = *src++;

        /* Literal length */
        size_t literal_len = token >> 4;
        if (literal_len == 15) {
            uint8_t s;
            do {
                if (src >= src_end) {
                    return LZ4_ERROR_CORRUPT;
                }
                s = *src++;
                literal_len += s;
            } while (s == 255);
        }

        /* Copy literals */
        if (literal_len > 0) {
            if (src + literal_len > src_end ||
                dst + literal_len > dst_end) {
                return LZ4_ERROR_CORRUPT;
            }
            memcpy(dst, src, literal_len);
            src += literal_len;
            dst += literal_len;
        }

        /* Check for end of block (last sequence has no match) */
        if (src >= src_end) {
            break;
        }

        /* Read offset (2 bytes, little endian) */
        if (src + 2 > src_end) {
            return LZ4_ERROR_CORRUPT;
        }
        size_t offset = read_le16(src);
        src += 2;

        if (offset == 0 || offset > (size_t)(dst - dst_start)) {
            return LZ4_ERROR_CORRUPT;
        }

        /* Match length */
        size_t match_len = (token & 0x0F) + MIN_MATCH;
        if (match_len == 15 + MIN_MATCH) {
            uint8_t s;
            do {
                if (src >= src_end) {
                    return LZ4_ERROR_CORRUPT;
                }
                s = *src++;
                match_len += s;
            } while (s == 255);
        }

        /* Copy match */
        if (dst + match_len > dst_end) {
            return LZ4_ERROR_SIZE;
        }

        const uint8_t *match = dst - offset;

        /* Handle overlapping copy */
        if (offset < match_len) {
            /* Byte-by-byte copy for overlapping regions */
            for (size_t i = 0; i < match_len; i++) {
                dst[i] = match[i];
            }
            dst += match_len;
        } else {
            memcpy(dst, match, match_len);
            dst += match_len;
        }
    }

    return dst - dst_start;
}

int64_t lz4_frame_decode(void *dst, size_t dst_size,
                         const void *src, size_t src_size)
{
    const uint8_t *in = src;
    uint8_t *out = dst;
    uint8_t flags;

    /* Parse header */
    int header_size = parse_frame_header(in, src_size, NULL, &flags);
    if (header_size < 0) {
        return header_size;
    }

    in += header_size;
    size_t remaining = src_size - header_size;

    size_t total_out = 0;

    /* Process blocks */
    while (remaining >= 4) {
        /* Read block size */
        uint32_t block_header = read_le32(in);
        in += 4;
        remaining -= 4;

        /* End mark */
        if (block_header == 0) {
            break;
        }

        int is_uncompressed = (block_header & BLOCK_UNCOMPRESSED) != 0;
        size_t block_size = block_header & ~BLOCK_UNCOMPRESSED;

        if (block_size > remaining) {
            return LZ4_ERROR_CORRUPT;
        }

        if (is_uncompressed) {
            /* Uncompressed block - just copy */
            if (total_out + block_size > dst_size) {
                return LZ4_ERROR_SIZE;
            }
            memcpy(out + total_out, in, block_size);
            total_out += block_size;
        } else {
            /* Compressed block */
            int64_t decoded = decompress_block(
                out + total_out, dst_size - total_out,
                in, block_size);
            if (decoded < 0) {
                return decoded;
            }
            total_out += decoded;
        }

        in += block_size;
        remaining -= block_size;

        /* Skip block checksum if present */
        if (flags & FLG_BLOCK_CHECKSUM) {
            if (remaining < 4) {
                return LZ4_ERROR_CORRUPT;
            }
            in += 4;
            remaining -= 4;
        }
    }

    return total_out;
}
