// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"

#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <sstream>

#include "extent_client.h"

#define USE_EXTENT_CLIENT_CACHE 1
#define USE_LOCK_CACHE          1

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst) {
#ifdef USE_EXTENT_CLIENT_CACHE
    ec = new extent_client_cache(extent_dst);
#else
    ec = new extent_client(extent_dst);
#endif
#ifdef USE_LOCK_CACHE
    lc = new lock_client_cache(lock_dst);
#else
    lc = new lock_client(lock_dst);
#endif
    lc->acquire(rootId);
    std::string rootContent;
    ec->get(rootId, rootContent);
    // Concurrency: init only if root is not initialized
    if (rootContent == "" && ec->put(rootId, "") != extent_protocol::OK)
        printf("error init root dir\n");  // XYB: init root dir
    releaseLock(rootId);
}

yfs_client::inum_t yfs_client::n2i(std::string n) {
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string yfs_client::filename(inum_t inum) {
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

uint32_t yfs_client::get_type(inum_t inum) const {
    extent_protocol::attr a;
    ec->getattr(inum, a);
    return a.type;
}

bool yfs_client::is_type(inum_t inum, extent_protocol::types type) const {
    return get_type(inum) == type;
}

bool yfs_client::isfile(inum_t inum) {
    return is_type(inum, extent_protocol::T_FILE);
}

bool yfs_client::isdir(inum_t inum) {
    return is_type(inum, extent_protocol::T_DIR);
}

bool yfs_client::is_symlink(inum_t inum) {
    return is_type(inum, extent_protocol::T_SYMLINK);
}

int yfs_client::getfile(inum_t inum, fileinfo &fin) {
    int r = OK;

    // printf("[YC] getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    // printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    return r;
}

int yfs_client::getdir(inum_t inum, dirinfo &din) {
    int r = OK;

    // printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    return r;
}

#define EXT_RPC(xx)                                                \
    do {                                                           \
        if ((xx) != extent_protocol::OK) {                         \
            printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
            r = IOERR;                                             \
            goto release;                                          \
        }                                                          \
    } while (0)

// Only support set size of attr
int yfs_client::setattr(inum_t ino, size_t size) {
    // std::cout << "[YC] [SETATTR] " << ino << " " << size << "\n";
    int r = OK;

    /*
     * get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */
    extent_protocol::attr a;
    if ((r = ec->getattr(ino, a)) != OK) {
        return r;
    }
    size_t oldsize = a.size;
    if (oldsize == size) return OK;
    std::string buf;
    ec->get(ino, buf);
    if (size < oldsize) {
        r = ec->put(ino, buf.substr(0, size));
    } else {
        r = ec->put(ino, buf.append(std::string(size - oldsize, '\0')));
    }

    return r;
}

int yfs_client::create(inum_t parent, const char *name, mode_t mode,
                       inum_t &ino_out) {
    lc->acquire(parent);
    // std::cout << "[YC] [CREATE] " << name << " at " << parent << std::endl;
    int r = OK;
    bool found = false;
    if (unlockedLookup(parent, name, found, ino_out) == OK && found) {
        releaseLock(parent);
        std::cerr << "!ERR file exists" << std::endl;
        return EXIST;
    }
    uint32_t type = get_type(parent);
    if (type != extent_protocol::T_DIR && type != extent_protocol::T_SYMLINK)
        return IOERR;
    if (type == extent_protocol::T_SYMLINK) {
        std::string path;
        readlink(parent, path);
        path_to_inum(path, parent);
        // std::cout << "\tSymlink: set target parent: " << parent << "\n";
    }
    std::string buf;
    r = ec->get(parent, buf);
    if (r != extent_protocol::OK) {
        std::cerr << "read parent failed" << std::endl;
        releaseLock(parent);
        return r;
    }
    // create inode
    // FIXME: ec is not thread-safe, it may give the same inode to two different
    // yfs_client(whose parent is not the same) to the new file
    if ((r = ec->create(extent_protocol::T_FILE, ino_out)) != OK) {
        std::cerr << "!ERR ec returns error " << r << std::endl;
        releaseLock(parent);
        return r;
    }
    // std::cout << "[yc] [CREATE] inode: " << ino_out << "\n";
    // Add an entry to parent
    buf.append(to_str(std::string(name), ino_out));
    if ((r = ec->put(parent, buf)) != extent_protocol::OK) {
        std::cerr << "!ERR ec put" << std::endl;
        releaseLock(parent);
        return r;
    }
    releaseLock(parent);
    return r;
}

int yfs_client::mkdir(inum_t parent, const char *name, mode_t mode,
                      inum_t &ino_out) {
    // std::cout << "[YC] [MKDIR] " << name << " at " << parent << "\n";
    int r = OK;
    bool found = false;
    lc->acquire(parent);
    if (unlockedLookup(parent, name, found, ino_out) == OK && found) {
        releaseLock(parent);
        return EXIST;
    }
    if (!isdir(parent)) {
        releaseLock(parent);
        return IOERR;
    }
    std::string buf;
    ec->get(parent, buf);
    // create inode
    if (ec->create(extent_protocol::T_DIR, ino_out) != OK) {
        releaseLock(parent);
        return IOERR;
    }
    // Add an entry to parent
    buf.append(to_str(std::string(name), ino_out));
    ec->put(parent, buf);
    releaseLock(parent);
    return r;
}

int yfs_client::lookup(inum_t parent, const char *name, bool &found,
                       inum_t &ino_out) {
    lc->acquire(parent);
    int ret = unlockedLookup(parent, name, found, ino_out);
    releaseLock(parent);
    return ret;
}

int yfs_client::unlockedLookup(inum_t parent, const char *name, bool &found,
                               inum_t &ino_out) {
    // std::cout << "[YC] [LOOKUP] " << name << " in " << parent << '\n';
    int r = NOENT;

    if (!isdir(parent)) return NOENT;

    status s;
    std::string buf;
    if ((s = (ec->get(parent, buf))) != OK) {
        // std::cout << "[YC] [READDIR] !ERR " << s << "\n";
        return s;
    }

    while (!buf.empty()) {
        size_t pos = buf.find('/');
        std::string fname = buf.substr(0, pos);
        buf.erase(0, pos + 1);
        pos = buf.find('/');
        std::string ino = (buf.substr(0, pos));
        buf.erase(0, pos + 1);
        if (fname == name) {
            r = OK;
            found = true;
            ino_out = atoi(ino.c_str());
            break;
        }
    }

    return r;
}

int yfs_client::readdir(inum_t dir, std::list<dirent> &list) {
    lc->acquire(dir);
    int ret = unlockedReaddir(dir, list);
    releaseLock(dir);
    return ret;
}

int yfs_client::unlockedReaddir(inum_t dir, std::list<dirent> &list) {
    // std::cout << "[YC] [READDIR] " << dir << "\n";
    int r = OK;

    /*
     * your code goes here.
     * note: you should parse the directory content using your defined format,
     * and push the dirents to the list.
     */
    std::string buf;
    status s;
    if ((s = (ec->get(dir, buf))) != OK) {
        // std::cout << "[YC] [READDIR] !ERR " << s << "\n";
        return s;
    }
    if (isdir(dir)) {
        while (!buf.empty()) {
            size_t pos = buf.find('/');
            std::string fname = buf.substr(0, pos);
            buf.erase(0, pos + 1);
            pos = buf.find('/');
            std::string ino = (buf.substr(0, pos));
            buf.erase(0, pos + 1);
            dirent e;
            e.name = fname;
            e.inum = atoi(ino.c_str());
            list.push_back(e);
            // std::cout << "\tdir ent: " << e.name << "\t" << e.inum << "\n";
        }
    } else {
        std::cout << "!!!!!!!!!!!!!!!readdir call to a file or symlink!\n";
    }
    return r;
}

int yfs_client::read(inum_t ino, size_t size, off_t off, std::string &data) {
    // std::cout << "[YC] [READ] " << ino << " size=" << size << " off=" << off
    //           << "\n";
    int r = OK;
    std::string buf;
    lc->acquire(ino);
    r = ec->get(ino, buf);
    releaseLock(ino);
    // std::cout << "\tget OK\n";
    if (off >= (long)buf.size())
        data = "";
    else
        data = buf.substr(off, size);
    // std::cout << "data read " << data.size() << " bytes:\n" << data << "<\n";
    return r;
}

int yfs_client::write(inum_t ino, size_t size, off_t off, const char *data,
                      size_t &bytes_written) {
    // std::cout << "[yc] [write] " << ino << " size=" << size << " off=" << off
    //           << "\n";
    int r = OK;
    lc->acquire(ino);

    /*
     * write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */
    std::string buf;
    r = ec->get(ino, buf);
    // std::cout << "origin size " << buf.size() << " original content:\n";
    //   << buf << "|||\n";
    if (off > (long)buf.size()) {
        std::string tmp(data, size);
        std::string new_data = std::string(off - buf.size(), '\0') + tmp;
        buf += new_data;
        bytes_written = new_data.length();
        // std::cout << "write beyond buf, add 0's newsize=" << buf.size()
        //           << " and buf is:\n"
        //           << buf << std::endl;
    } else {
        std::string new_data(data, size);
        // std::cout << "using replace policy size=" << size
        //           << " data.size()=" << new_data.size() << std::endl;
        buf.replace(off, size, new_data.substr(0, size));
        bytes_written = size;
    }
    // setattr(ino, buf.size());
    r = ec->put(ino, buf);
    // std::cout << bytes_written << " bytes written, now size " << buf.size()
    //           << " r=" << r << "\n";
    //   << " updated:\n";
    //   << buf << "|||\n";
    releaseLock(ino);
    return r;
}

int yfs_client::unlink(inum_t parent, const char *name) {
    lc->acquire(parent);
    // std::cout << "[YC] [UNLINK] parent " << parent << " " << name << std::endl;
    int r = OK;

    /*
     * remove the file using ec->remove,
     * and update the parent directory content.
     */
    uint32_t type = get_type(parent);
    if (type == extent_protocol::T_SYMLINK) {
        std::string path = "";
        readlink(parent, path);
        path_to_inum(path, parent);
        type = get_type(parent);
    }
    if (type != extent_protocol::T_DIR) {
        std::cerr << "!!!PARENT is not a directory" << std::endl;
        releaseLock(parent);
        return IOERR;
    }
    std::list<dirent> flist;
    // std::cout << "[YC] [UNLINK]->readdir\n";
    unlockedReaddir(parent, flist);
    for (std::list<dirent>::iterator i = flist.begin(); i != flist.end(); i++) {
        if (i->name.compare(name) == 0) {
            inum_t lid = i->inum;
            lc->acquire(lid);
            flist.erase(i);
            ec->remove(i->inum);
            std::string buf;
            for (std::list<dirent>::iterator i = flist.begin();
                 i != flist.end(); i++) {
                buf.append(to_str(i->name, i->inum));
            }
            ec->put(parent, buf);
            releaseLock(lid);
            releaseLock(parent);
            // std::cout << "[YC] [UNLINK] OK\n";
            return OK;
        }
    }
    r = NOENT;
    releaseLock(parent);
    return r;
}

int yfs_client::symlink(const char *link, inum_t parent, const char *name,
                        inum_t &ino_out) {
    lc->acquire(parent);
    // std::cout << "[YC] [SYMLINK] " << parent << " " << name << " " << link <<
    // "\n"; create a new file, write path(link) into it
    int r = OK;
    if (!isdir(parent)) return IOERR;
    std::string buf;
    ec->get(parent, buf);
    // create inode
    if (ec->create(extent_protocol::T_SYMLINK, ino_out) != OK) {
        return IOERR;
    }
    // No need to lock since write itself would lock
    // lc->acquire(ino_out);
    // Add an entry to parent
    buf.append(to_str(std::string(name), ino_out));
    ec->put(parent, buf);

    // std::cout << "\t Create symlink file in parent ok\n";
    size_t written = 0;
    r = write(ino_out, strlen(link), 0, link, written);
    releaseLock(parent);
    return r;
}

std::string yfs_client::to_str(std::string fname, inum_t ino) {
    std::string package(fname);
    char buf[16];
    sprintf(buf, "/%llu/", ino);
    package.append(buf);
    return package;
}

int yfs_client::path_to_inum(std::string path, inum_t &ino_out) {
    std::string target = path;
    inum_t p = 0;

    std::list<dirent> dirlist;
    while (!target.empty()) {
        size_t pos = target.find('/');
        std::string ent = target.substr(0, pos);
        if (pos == std::string::npos) pos = target.size() - 1;
        target.erase(0, pos + 1);
        unlockedReaddir(p, dirlist);
        std::list<dirent>::iterator i;
        for (i = dirlist.begin(); i != dirlist.end(); i++) {
            if (i->name == ent) {
                p = i->inum;
                break;
            }
        }
        if (i == dirlist.end()) return NOENT;
    }
    ino_out = p;
    return OK;
}

int yfs_client::readlink(inum_t ino, std::string &path) {
    return ec->get(ino, path);
}

void yfs_client::releaseLock(inum_t lockId) {
    // Consistency insurance: writeback data when lock is released
    ec->flush(lockId);
    lc->release(lockId);
}
