#pragma once

#include <ext2fs/ext2fs.h>

#define min(a,b) 				\
   ({ __typeof__ (a) _a = (a); 	\
       __typeof__ (b) _b = (b); \
     _a > _b ? _b : _a; })

#define max(a,b) 				\
   ({ __typeof__ (a) _a = (a); 	\
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

typedef int (*viewer_t)(void* data, off_t pos, uint32_t size_read);

int view_inode(int in, const struct ext2_super_block* sblk, const struct ext2_inode* inode,
               viewer_t viewer, void* user_data);