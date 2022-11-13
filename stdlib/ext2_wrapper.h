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

int read_range(int fd, void* buf, uint32_t pos, uint32_t size);
int read_blk(int fd, void* buf, uint32_t blk_pos, uint32_t blk_size);
int read_sblk(int fd, struct ext2_super_block* sblk_buf);
uint32_t get_blk_size(const struct ext2_super_block* sblk);
uint32_t get_inode_group(const struct ext2_super_block* sblk, uint32_t inode_nr);
uint32_t get_inode_local_index(const struct ext2_super_block* sblk, uint32_t inode_nr);
uint32_t get_inode_pos(const struct ext2_super_block* sblk,
							         const struct ext2_group_desc* bg_desc_table, uint32_t inode_nr);
enum BLK_VIEWER
{
	BLK_VIEWER_END = 0,
	BLK_VIEWER_CONT
};

// This callbback must return only values from BLK_VIEWER or -errno
typedef int (*viewer_t)(void* data, off64_t pos, uint32_t blk_size);

// This function execute viewer on all data blocks accotiated with inode
int view_blocks(int fd, const struct ext2_super_block* sblk, const struct ext2_inode* inode,
                viewer_t viewer, void* user_data);


// Viewer for copy file
struct copyer_data_t
{
    uint8_t* buf;				// Buffer for ext2 data block
    int in, out;				// Descriptors of opened src and dest files
	uint64_t unreaded_size;		// Size of file in bytes
};

int copyer(void* data_ptr, off64_t blk_pos, uint32_t blk_size);