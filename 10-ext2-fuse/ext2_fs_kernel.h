#pragma once

#include <print_lib.h>
#include <ext2_wrapper.h>

// path: /hello/file
// path: /hello/dir
// path: /hello/dir/
int path2inode(const char* path,
			    const int img,
			    const struct ext2_super_block* sblk,
			    const struct ext2_group_desc* bg_decs_table,
			    uint8_t* blk_buf);  // Size must be not less then get_blk_size(sblk)
