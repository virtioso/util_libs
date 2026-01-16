#
# Copyright 2024, Unikie
#
# SPDX-License-Identifier: BSD-2-Clause
#

cmake_minimum_required(VERSION 3.16.0)

project(liblz4 C)

add_library(lz4 STATIC EXCLUDE_FROM_ALL src/lz4_decode.c)
target_include_directories(lz4 PUBLIC include)
target_link_libraries(lz4 muslc sel4_autoconf)
