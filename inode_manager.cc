#include "inode_manager.h"

#include <math.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <ctime>

// disk layer -----------------------------------------

disk::disk() {
    bzero(blocks, sizeof(blocks));
}

inline void disk::read_block(blockid_t id, char *buf) {
    // if (id < 0 || id >= BLOCK_NUM || buf == NULL) return;

    memcpy(buf, blocks[id], BLOCK_SIZE);
}

inline void disk::write_block(blockid_t id, const char *buf) {
    // if (id < 0 || id >= BLOCK_NUM || buf == NULL) return;

    memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t block_manager::alloc_block() {
    /*
     * you should mark the corresponding bit in block bitmap when alloc.
     * you need to think about which block you can start to be allocated.
     */
    // iterate throught the bit map to find a free block
    // uint32_t bitmap_blocks = BLOCK_NUM / BPB;
    // uint32_t inode_table_blocks = INODE_NUM / IPB;
    // uint32_t reserved = 2 + bitmap_blocks + inode_table_blocks;
    const uint32_t reserved = 2 + BLOCK_NUM / BPB + INODE_NUM / IPB;
    blockid_t free_block_id = BLOCK_NUM;
    pthread_mutex_lock(&lock);
    std::vector<int>::iterator end = using_blocks.end();
    std::vector<int>::iterator it = using_blocks.begin();
    std::advance(it, reserved);
    for (; it != end; it++) {
        if (*it == 0) {
            *it = 1;
            pthread_mutex_unlock(&lock);
            return it - using_blocks.begin();
        }
    }
    pthread_mutex_unlock(&lock);
    return free_block_id;
}

void block_manager::free_block(uint32_t id) {
    /*
     * your code goes here.
     * note: you should unmark the corresponding bit in the block bitmap when
     * free.
     */
    pthread_mutex_lock(&lock);
    using_blocks[id] = 0;
    pthread_mutex_unlock(&lock);
    return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager() {
    d = new disk();

    // format the disk
    sb.size = BLOCK_SIZE * BLOCK_NUM;
    sb.nblocks = BLOCK_NUM;
    sb.ninodes = INODE_NUM;

    // init free block map
    using_blocks = std::vector<int>(BLOCK_NUM, 0);

    uint32_t bitmap_blocks = BLOCK_NUM / BPB;
    uint32_t inode_table_blocks = INODE_NUM / IPB;
    uint32_t reserved = 2 + bitmap_blocks + inode_table_blocks;
    for (uint32_t i = 0; i < reserved; i++) using_blocks[i] = 1;
    pthread_mutex_init(&lock, NULL);
}

inline void block_manager::read_block(uint32_t id, char *buf) {
    d->read_block(id, buf);
}

void block_manager::write_block(uint32_t id, const char *buf) {
    pthread_mutex_lock(&lock);
    d->write_block(id, buf);
    pthread_mutex_unlock(&lock);
}

// inode layer -----------------------------------------

inode_manager::inode_manager() {
    bm = new block_manager();
    srand(getpid());
    pthread_mutex_init(&lock, NULL);
    uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
    if (root_dir != 1) {
        printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
        exit(0);
    }
}

/* Create a new file.
 * Return its inum. */
uint32_t inode_manager::alloc_inode(uint32_t type) {
    /*
     * your code goes here.
     * note: the normal inode block should begin from the 2nd inode block.
     * the 1st is used for root_dir, see inode_manager::inode_manager().
     */

    inode_t *inode_buf;
    pthread_mutex_lock(&lock);
    // Allocate root dir (inode=1)
    if (get_inode(1) == NULL) {
        // printf("> im: alloc_inode root 1\n");
        inode_t ino;
        ino.type = type;
        ino.size = 0;
        std::time_t time = std::time(NULL);
        ino.ctime = time;
        ino.atime = time;
        ino.mtime = time;
        put_inode(1, &ino);
        pthread_mutex_unlock(&lock);
        return 1;
    }
    // random choose:
    // Find a free inode in inode table
    uint32_t upbound = bm->sb.ninodes - 1;
    size_t iter = 0;
    while (++iter <= bm->sb.ninodes) {
        uint32_t i = 1 + rand() % upbound;
        inode_buf = get_inode(i);
        if (inode_buf == NULL) {
            // printf("> im: alloc_inode %d\n", i);
            inode_t ino;
            ino.type = type;
            ino.size = 0;
            std::time_t time = std::time(NULL);
            ino.ctime = time;
            ino.atime = time;
            ino.mtime = time;
            put_inode(i, &ino);
            pthread_mutex_unlock(&lock);
            return i;
        }
    }
    printf("!!! Failed to allocate an inode\n");
    pthread_mutex_unlock(&lock);
    return 1;
}

std::vector<extent_protocol::extentid_t> inode_manager::alloc_ninode(
    uint32_t type, int n) {
    inode_t *inode_buf;
    pthread_mutex_lock(&lock);
    // Allocate root dir (inode=1)
    if (get_inode(1) == NULL) {
        // printf("> im: alloc_inode root 1\n");
        inode_t ino;
        ino.type = type;
        ino.size = 0;
        std::time_t time = std::time(NULL);
        ino.ctime = time;
        ino.atime = time;
        ino.mtime = time;
        put_inode(1, &ino);
    }
    // random choose: Find a free inode in inode table
    uint32_t upbound = bm->sb.ninodes - 1;
    size_t iter = 0;
    std::vector<extent_protocol::extentid_t> inumArray;
    while (++iter <= bm->sb.ninodes && n > 0) {
        uint32_t i = 1 + rand() % upbound;
        inode_buf = get_inode(i);
        if (inode_buf == NULL) {
            n--;
            inode_t ino;
            ino.type = type;
            ino.size = 0;
            std::time_t time = std::time(NULL);
            ino.ctime = time;
            ino.atime = time;
            ino.mtime = time;
            put_inode(i, &ino);
            inumArray.push_back(i);
        }
    }
    // printf("!!! Failed to allocate an inode\n");
    pthread_mutex_unlock(&lock);
    return inumArray;
}

void inode_manager::free_inode(uint32_t inum) {
    /*
     * your code goes here.
     * note: you need to check if the inode is already a freed one;
     * if not, clear it, and remember to write back to disk.
     */
    blockid_t inode_block_id = IBLOCK(inum, bm->sb.nblocks);
    char buf[BLOCK_SIZE];
    bm->read_block(inode_block_id, buf);
    struct inode *ino = (struct inode *)buf;
    if (ino->type == 0) return;
    ino->type = 0;
    // Free all its blocks
    for (int i = 0; i < round(ino->size / (double)BLOCK_SIZE); i++) {
        bm->free_block(ino->blocks[i]);
    }
    return;
}

/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode *inode_manager::get_inode(uint32_t inum) {
    struct inode *ino, *ino_disk;
    char buf[BLOCK_SIZE];

    // printf("\tim: get_inode %d\n", inum);

    // if (inum < 0 || inum >= INODE_NUM) {
    //     printf("\tim: inum out of range\n");
    //     return NULL;
    // }

    bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);

    ino_disk = (struct inode *)buf;
    if (ino_disk->type == 0) {
        // printf("\tim: inode not exist\n");
        return NULL;
    }

    ino = (struct inode *)malloc(sizeof(struct inode));
    *ino = *ino_disk;

    return ino;
}

void inode_manager::put_inode(uint32_t inum, struct inode *ino) {
    char buf[BLOCK_SIZE];
    struct inode *ino_disk;

    // printf("\tim: put_inode %d\n", inum);
    if (ino == NULL) return;

    bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
    ino_disk = (struct inode *)buf + inum % IPB;
    *ino_disk = *ino;
    bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* Get all the data of a file by inum.
 * Return alloced data, should be freed by caller. */
void inode_manager::read_file(uint32_t inum, char **buf_out, int *size) {
    /*
     * your code goes here.
     * note: read blocks related to inode number inum,
     * and copy them to buf_out
     */
    pthread_mutex_lock(&lock);
    inode_t *ino = get_inode(inum);
    if (ino == NULL) {
        printf("ERR! inode %d not found\n", inum);
        pthread_mutex_unlock(&lock);
        return;
    }
    *size = ino->size;
    int nblk = NBLK(ino->size);
    char *tmp = (char *)malloc((nblk * BLOCK_SIZE));
    int block_idx;
    for (block_idx = 0; block_idx < nblk; block_idx++) {
        blockid_t bid = get_inode_block(ino, block_idx);
        bm->read_block(bid, tmp + block_idx * BLOCK_SIZE);
    }
    *buf_out = tmp;
    // udpate metadata
    std::time_t time = std::time(NULL);
    ino->atime = (unsigned int)time;
    put_inode(inum, ino);
    pthread_mutex_unlock(&lock);
    free(ino);
    return;
}

/* alloc/free blocks if needed */
void inode_manager::write_file(uint32_t inum, const char *buf, int size) {
    /*
     * your code goes here.
     * note: write buf to blocks of inode inum.
     * you need to consider the situation when the size of buf
     * is larger or smaller than the size of original inode
     */
    pthread_mutex_lock(&lock);
    inode_t *ino = get_inode(inum);
    if (ino == NULL) {
        printf("ERR! inode %d not found\n", inum);
        pthread_mutex_unlock(&lock);
        return;
    }
    if ((unsigned)size > MAXFILE * BLOCK_SIZE) {
        printf("ERR! File size is too large to support!");
        pthread_mutex_unlock(&lock);
        return;
    }
    int o_blk_num = NBLK(ino->size);
    int new_blk_num = NBLK(size);
    // Make a copy of data, when accessing this data as multiple BLOCK_SIZE
    // blocks, it's possible to access past the boundary of memory, leading to
    // segmentation fault
    char *copy = (char *)calloc(new_blk_num, BLOCK_SIZE);
    memcpy(copy, buf, size);
    buf = copy;
    if (new_blk_num > o_blk_num) {
        int i;
        int di_addition = new_blk_num < NDIRECT ? new_blk_num : NDIRECT;
        for (i = o_blk_num; i < di_addition; i++) {
            blockid_t bid = bm->alloc_block();
            ino->blocks[i] = bid;
        }
        if (i < new_blk_num) {
            ino->blocks[NDIRECT] = bm->alloc_block();
        }
        if (i < new_blk_num) {
            blockid_t indirect_id = ino->blocks[NDIRECT];
            char buf[BLOCK_SIZE];
            bm->read_block(indirect_id, buf);
            // Unrolling
            while (i + 1 < new_blk_num) {
                blockid_t id = bm->alloc_block();
                blockid_t id2 = bm->alloc_block();
                ((blockid_t *)buf)[i - NDIRECT] = id;
                ((blockid_t *)buf)[i + 1 - NDIRECT] = id2;
                i += 2;
            }
            if (i < new_blk_num) {
                blockid_t id = bm->alloc_block();
                ((blockid_t *)buf)[i - NDIRECT] = id;
            }
            bm->write_block(indirect_id, buf);
        }
    } else if (new_blk_num < o_blk_num) {
        // free blocks
        int diff = o_blk_num - new_blk_num;
        for (int i = 0; i < diff; i++) {
            bm->free_block(get_inode_block(ino, new_blk_num + i));
        }
    }
    // Write new file data
    int block_idx;
    for (block_idx = 0; block_idx < new_blk_num; block_idx++) {
        blockid_t bid = get_inode_block(ino, block_idx);
        bm->write_block(bid, buf + block_idx * BLOCK_SIZE);
    }
    // update metadata
    ino->size = size;
    std::time_t time = std::time(NULL);
    ino->atime = time;
    ino->mtime = time;
    ino->ctime = time;

    // write back inode
    put_inode(inum, ino);
    // std::cout << " im: write_file return" << std::endl;
    pthread_mutex_unlock(&lock);
    // std::cout << " im: write_file return" << std::endl;
    free(ino);
    return;
}

void inode_manager::getattr(uint32_t inum, extent_protocol::attr &a) {
    /*
     * your code goes here.
     * note: get the attributes of inode inum.
     * you can refer to "struct attr" in extent_protocol.h
     */
    inode_t *ino = get_inode(inum);
    if (ino == NULL) {
        a.type = 0;
        return;
    }
    a.type = ino->type;
    a.atime = ino->atime;
    a.mtime = ino->mtime;
    a.ctime = ino->ctime;
    a.size = ino->size;
    free(ino);
}

void inode_manager::remove_file(uint32_t inum) {
    /*
     * your code goes here
     * note: you need to consider about both the data block and inode of the
     * file
     */
    pthread_mutex_lock(&lock);
    inode_t *ino = get_inode(inum);
    if (ino == NULL) {
        printf("ERR! remove_file: inum not exist\n");
        pthread_mutex_unlock(&lock);
        // printf("ERR! remove_file: inum not exist\n");
        return;
    }
    // freedom to blocks
    uint32_t size = ino->size;
    int nblk = NBLK(size);
    int block_idx;
    int bound = nblk < NDIRECT ? nblk : NDIRECT;
    for (block_idx = 0; block_idx < bound; block_idx++) {
        bm->free_block(ino->blocks[block_idx]);
    }
    // indirect read
    if (block_idx < nblk) {
        blockid_t indirect_blk_id = (blockid_t)ino->blocks[NDIRECT];
        char blockid_block[BLOCK_SIZE];
        bm->read_block(indirect_blk_id, blockid_block);
        for (; block_idx < nblk; block_idx++) {
            bm->free_block(((blockid_t *)blockid_block)[block_idx - NDIRECT]);
        }
    }
    // reset metadata
    ino->type = 0;  // mark as deleted
    ino->size = 0;
    std::time_t time = std::time(NULL);
    ino->mtime = time;
    put_inode(inum, ino);
    pthread_mutex_unlock(&lock);
    free(ino);
    return;
}

blockid_t inode_manager::get_inode_block(inode_t *ino, unsigned int idx) const {
    if (idx < NDIRECT) {
        return ino->blocks[idx];
    } else {
        blockid_t indirect_blk_id = ino->blocks[NDIRECT];
        char buf[BLOCK_SIZE];
        bm->read_block(indirect_blk_id, buf);
        return ((blockid_t *)buf)[idx - NDIRECT];
    }
}
