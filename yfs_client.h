#ifndef yfs_client_h
#define yfs_client_h

#include <string>
#include <vector>

#include "extent_client.h"
#include "lock_client.h"
#include "lock_client_cache.h"
#include "lock_protocol.h"

#define FNAME_SIZE     248
#define INUM_SIZE      (sizeof(inum))
#define DIR_ENTRY_SIZE (FNAME_SIZE + INUM_SIZE)

class yfs_client {
   public:
    typedef unsigned long long inum_t;
    enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
    typedef int status;

    struct fileinfo {
        unsigned long long size;
        unsigned long atime;
        unsigned long mtime;
        unsigned long ctime;
    };
    struct dirinfo {
        unsigned long atime;
        unsigned long mtime;
        unsigned long ctime;
    };
    struct dirent {
        std::string name;
        yfs_client::inum_t inum;
    };

   private:
    extent_client *ec;
    lock_client *lc;
    const extent_protocol::extentid_t rootId = 1;

    static std::string filename(inum_t);
    static inum_t n2i(std::string);

    std::string to_str(std::string filename, inum_t ino);

    int path_to_inum(std::string path, inum_t &ino_out);
    void releaseLock(inum_t lockId);

   public:
    yfs_client();
    yfs_client(std::string, std::string);

    bool isfile(inum_t);
    bool isdir(inum_t);
    bool is_symlink(inum_t);
    bool is_type(inum_t, extent_protocol::types) const;
    uint32_t get_type(inum_t inum) ;
    uint32_t unlocked_get_type(inum_t inum) const;

    int getfile(inum_t, fileinfo &);
    int unlocked_getfile(inum_t, fileinfo &);
    int getdir(inum_t, dirinfo &);
    int unlocked_getdir(inum_t, dirinfo &);

    int setattr(inum_t, size_t);
    int unlockedLookup(inum_t, const char *, bool &, inum_t &);
    int lookup(inum_t, const char *, bool &, inum_t &);
    int create(inum_t, const char *, mode_t, inum_t &);
    int readdir(inum_t, std::list<dirent> &);
    int unlockedReaddir(inum_t, std::list<dirent> &);
    int write(inum_t, size_t, off_t, const char *, size_t &);
    int read(inum_t, size_t, off_t, std::string &);
    int unlink(inum_t, const char *);
    int mkdir(inum_t, const char *, mode_t, inum_t &);

    /** you may need to add symbolic link related methods here.*/
    int symlink(const char *link, inum_t ino, const char *name,
                inum_t &ino_out);
    // readlink returns the path of symlink whose inode number is ino
    int readlink(inum_t ino, std::string &path);

    /**
     * Communication Link
     */
    int onLockRevoke(unsigned long long lid);
};

#endif
