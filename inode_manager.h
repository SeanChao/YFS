// inode layer interface.

#ifndef inode_h
#define inode_h

#include <pthread.h>
#include <stdint.h>
#include <pthread.h>
#include <vector>

#include "extent_protocol.h"  // TODO: delete it

#define DISK_SIZE  1024 * 1024 * 16
#define BLOCK_SIZE 512
#define BLOCK_NUM  (DISK_SIZE / BLOCK_SIZE)

typedef uint32_t blockid_t;

// disk layer -----------------------------------------

class disk {
   private:
    unsigned char blocks[BLOCK_NUM][BLOCK_SIZE];

   public:
    disk();
    void read_block(uint32_t id, char *buf);
    void write_block(uint32_t id, const char *buf);
};

// block layer -----------------------------------------

typedef struct superblock {
    uint32_t size;
    uint32_t nblocks;
    uint32_t ninodes;
} superblock_t;

class block_manager {
   private:
    disk *d;
    // for block manager itself, for inode layer it should look for the real
    // block
    std::vector<int> using_blocks;
    pthread_mutex_t lock;

   public:
    block_manager();
    struct superblock sb;

    uint32_t alloc_block();
    void free_block(uint32_t id);
    void read_block(uint32_t id, char *buf);
    void write_block(uint32_t id, const char *buf);
};

// inode layer -----------------------------------------

#define INODE_NUM 8192

// Inodes per block.
#define IPB 1
//(BLOCK_SIZE / sizeof(struct inode))

// Block containing inode i
#define IBLOCK(i, nblocks) ((nblocks) / BPB + (i) / IPB + 3)

// Bitmap bits per block (number of bits in a block, indicating how much the
// number of block state that can be hold in one block)
#define BPB (BLOCK_SIZE * 8)

// Block containing bit for block b
#define BBLOCK(b) ((b) / BPB + 2)

// Number of direct blocks
#define NDIRECT 100
// Number of doubly-indirect blocks
#define NINDIRECT (BLOCK_SIZE / sizeof(uint))
// Max number of blocks that a file can have
#define MAXFILE (NDIRECT + NINDIRECT)

#define NBLK(size) ((int)ceil(size / (double)BLOCK_SIZE))

typedef struct inode {
    short type;
    unsigned int size;
    unsigned int atime;
    unsigned int mtime;
    unsigned int ctime;
    blockid_t blocks[NDIRECT + 1];  // Data block addresses
} inode_t;

class inode_manager {
   private:
    block_manager *bm;
    struct inode *get_inode(uint32_t inum);
    void put_inode(uint32_t inum, struct inode *ino);
    blockid_t get_inode_block(inode_t *ino, unsigned int idx) const;
    pthread_mutex_t lock;

   public:
    inode_manager();
    uint32_t alloc_inode(uint32_t type);
    std::vector<extent_protocol::extentid_t> alloc_ninode(uint32_t type, int n);
    void free_inode(uint32_t inum);
    void read_file(uint32_t inum, char **buf, int *size);
    void write_file(uint32_t inum, const char *buf, int size);
    void remove_file(uint32_t inum);
    void getattr(uint32_t inum, extent_protocol::attr &a);
};

#endif
