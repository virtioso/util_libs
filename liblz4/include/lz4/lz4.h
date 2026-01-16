/*
 * Copyright 2024, Unikie
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Minimal LZ4 frame format decoder for seL4 VMM.
 * Supports LZ4 frame format (magic 0x184D2204) with content size.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/* LZ4 frame format magic number */
#define LZ4_FRAME_MAGIC 0x184D2204

/* Error codes */
#define LZ4_OK              0
#define LZ4_ERROR_MAGIC    -1   /* Invalid magic number */
#define LZ4_ERROR_VERSION  -2   /* Unsupported version */
#define LZ4_ERROR_HEADER   -3   /* Invalid header */
#define LZ4_ERROR_CHECKSUM -4   /* Checksum mismatch */
#define LZ4_ERROR_SIZE     -5   /* Output buffer too small */
#define LZ4_ERROR_CORRUPT  -6   /* Corrupted data */
#define LZ4_ERROR_NO_SIZE  -7   /* Content size not in header */

/**
 * Check if data is LZ4 frame format.
 *
 * @param src       Pointer to compressed data
 * @param src_size  Size of compressed data
 * @return          1 if LZ4 frame format, 0 otherwise
 */
int lz4_is_frame(const void *src, size_t src_size);

/**
 * Get decompressed content size from LZ4 frame header.
 *
 * The frame must have been compressed with --content-size flag.
 *
 * @param src       Pointer to compressed data
 * @param src_size  Size of compressed data
 * @return          Decompressed size, or negative error code
 */
int64_t lz4_frame_get_content_size(const void *src, size_t src_size);

/**
 * Decompress LZ4 frame to output buffer.
 *
 * @param dst       Output buffer for decompressed data
 * @param dst_size  Size of output buffer (must be >= content size)
 * @param src       Pointer to compressed data
 * @param src_size  Size of compressed data
 * @return          Number of bytes written to dst, or negative error code
 */
int64_t lz4_frame_decode(void *dst, size_t dst_size,
                         const void *src, size_t src_size);
