#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include <vector>

#include "extent_client.h"

#define FNAME_SIZE     248
#define INUM_SIZE      (sizeof(inum))
#define DIR_ENTRY_SIZE (FNAME_SIZE + INUM_SIZE)

class yfs_client {
    extent_client *ec;

   public:
    typedef unsigned long long inum;
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
        yfs_client::inum inum;
    };

   private:
    static std::string filename(inum);
    static inum n2i(std::string);

    std::string to_str(std::string filename, inum ino);
    std::string get_filename(std::string buf);
    static inum get_ino(std::string buf);

   public:
    yfs_client();
    yfs_client(std::string, std::string);

    bool isfile(inum);
    bool isdir(inum);

    int getfile(inum, fileinfo &);
    int getdir(inum, dirinfo &);

    int setattr(inum, size_t);
    int lookup(inum, const char *, bool &, inum &);
    int create(inum, const char *, mode_t, inum &);
    int readdir(inum, std::list<dirent> &);
    int write(inum, size_t, off_t, const char *, size_t &);
    int read(inum, size_t, off_t, std::string &);
    int unlink(inum, const char *);
    int mkdir(inum, const char *, mode_t, inum &);

    /** you may need to add symbolic link related methods here.*/
};

#endif