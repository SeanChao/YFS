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

yfs_client::yfs_client() {
    ec = new extent_client();
}

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst) {
    ec = new extent_client();
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n");  // XYB: init root dir
}

yfs_client::inum yfs_client::n2i(std::string n) {
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string yfs_client::filename(inum inum) {
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool yfs_client::is_type(inum inum, extent_protocol::types type) const {
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }
    if (a.type == type) {
        return true;
    }
    return false;
}

bool yfs_client::isfile(inum inum) {
    return is_type(inum, extent_protocol::T_FILE);
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 *
 * */

bool yfs_client::isdir(inum inum) {
    return is_type(inum, extent_protocol::T_DIR);
}

bool yfs_client::is_symlink(inum inum) {
    return is_type(inum, extent_protocol::T_SYMLINK);
}

int yfs_client::getfile(inum inum, fileinfo &fin) {
    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    return r;
}

int yfs_client::getdir(inum inum, dirinfo &din) {
    int r = OK;

    printf("getdir %016llx\n", inum);
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
int yfs_client::setattr(inum ino, size_t size) {
    std::cout << "[YC] [SETATTR] " << ino << " " << size << "\n";
    int r = OK;

    /*
     * your code goes here.
     * note: get the content of inode ino, and modify its content
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
    std::cout << "BUF:\n" << buf << std::endl;
    if (size < oldsize) {
        r = ec->put(ino, buf.substr(0, size));
    } else {
        // std::cout << "###\n" << size << "%" << oldsize << "\n";
        r = ec->put(ino, buf.append(std::string(size - oldsize, '\0')));
    }

    return r;
}

int yfs_client::create(inum parent, const char *name, mode_t mode,
                       inum &ino_out) {
    std::cout << "[YC] [CREATE] " << name << " at " << parent << std::endl;
    int r = OK;

    /*
     * your code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent
     * infomation.
     */
    bool found = false;
    if (lookup(parent, name, found, ino_out) == OK && found) {
        return EXIST;
    }
    if (!isdir(parent) && !is_symlink(parent)) return IOERR;
    if (is_symlink(parent)) {
        std::string path;
        readlink(parent, path);
        path_to_inum(path, parent);
        std::cout << "\tSymlink: set target parent: " << parent << "\n";
    }
    std::string buf;
    ec->get(parent, buf);
    // std::cout << "[yc] [CREATE] "
    //   << "get parent ok\n";
    // create inode
    if (ec->create(extent_protocol::T_FILE, ino_out) != OK) {
        return IOERR;
    }
    // TODO: modify the parent info
    // Add an entry to parent
    buf.append(to_str(std::string(name), ino_out));
    ec->put(parent, buf);
    return r;
}

int yfs_client::mkdir(inum parent, const char *name, mode_t mode,
                      inum &ino_out) {
    std::cout << "[YC] [MKDIR] " << name << " at " << parent << "\n";
    int r = OK;

    /*
     * your code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent
     * infomation.
     */
    bool found = false;
    if (lookup(parent, name, found, ino_out) == OK && found) {
        return EXIST;
    }
    if (!isdir(parent)) return IOERR;
    std::string buf;
    ec->get(parent, buf);
    // create inode
    if (ec->create(extent_protocol::T_DIR, ino_out) != OK) {
        return IOERR;
    }
    // Add an entry to parent
    buf.append(to_str(std::string(name), ino_out));
    ec->put(parent, buf);
    return r;
}

int yfs_client::lookup(inum parent, const char *name, bool &found,
                       inum &ino_out) {
    std::cout << "[YC] [LOOKUP] " << name << " in " << parent << '\n';
    int r = NOENT;

    /*
     * your code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
    if (!isdir(parent)) return NOENT;
    std::list<dirent> flist;
    if (readdir(parent, flist) != OK) return IOERR;
    std::cout << "\t[YC] [LOOKUP] "
              << "readdir OK\n";
    for (std::list<dirent>::iterator it = flist.begin(); it != flist.end();
         it++) {
        if (name == it->name) {
            r = OK;
            found = true;
            ino_out = it->inum;
            std::cout << "yc: lookup found " << ino_out << "\n";
            break;
        }
    }
    return r;
}

int yfs_client::readdir(inum dir, std::list<dirent> &list) {
    std::cout << "[YC] [READDIR] " << dir << "\n";
    int r = OK;

    /*
     * your code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */
    std::string buf;
    status s;
    if ((s = (ec->get(dir, buf))) != OK) {
        std::cout << "[YC] [READDIR] !ERR " << s << "\n";
        return s;
    }
    // std::cout << "\t[YC] [READDIR] "
    //           << "get OK\n";
    std::cout << buf << "<<\n";
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
            e.inum = n2i(ino);
            list.push_back(e);
            std::cout << "\tdir ent: " << e.name << "\t" << e.inum << "\n";
        }
    } else if (is_symlink(dir)) {
        std::cout << "\t Read symlink dir!\n";
    } else {
        std::cout << "!!!!!!!!!!!!!!!readdir call to a file!\n";
    }
    std::cout << "\t[YC] [READDIR] "
              << "build list OK\n";
    return r;
}

int yfs_client::read(inum ino, size_t size, off_t off, std::string &data) {
    std::cout << "[YC] [READ] " << ino << " size=" << size << " off=" << off
              << "\n";
    int r = OK;

    /*
     * your code goes here.
     * note: read using ec->get().
     */
    std::string buf;
    r = ec->get(ino, buf);
    // std::cout << "\tget OK\n";
    if (off >= (long)buf.size())
        data = "";
    else
        data = buf.substr(off, size);
    std::cout << "data read " << data.size() << " bytes : \n" << data << "<\n";
    return r;
}

int yfs_client::write(inum ino, size_t size, off_t off, const char *data,
                      size_t &bytes_written) {
    std::cout << "[yc] [write] " << ino << " size=" << size << " off=" << off << "\n";
    int r = OK;

    /*
     * your code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */
    std::string buf;
    r = ec->get(ino, buf);
    std::cout << "origin size " << buf.size() << " original content:\n";
    //   << buf << "|||\n";
    if (off > (long)buf.size()) {
        std::string tmp = "";
        tmp.assign(data, size);
        std::string new_data = std::string(off - buf.size(), '\0') + tmp;
        buf = buf + new_data;
        bytes_written = new_data.length();
        std::cout << "write beyond buf, add 0's newsize=" << buf.size()
                  << " and buf is:\n"
                  << buf << std::endl;
    } else {
        std::string new_data(data);
        std::cout << "using replace policy size=" << size
                  << " data.size()=" << new_data.size() << std::endl;
        buf.replace(off, size, new_data.substr(0, size));
        bytes_written = size;
    }
    // setattr(ino, buf.size());
    r = ec->put(ino, buf);
    std::cout << bytes_written << " bytes written, now size " << buf.size() << " r=" << r << "\n";
    //   << " updated:\n";
    //   << buf << "|||\n";
    return r;
}

int yfs_client::unlink(inum parent, const char *name) {
    int r = OK;

    /*
     * your code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
    // bool dir = isdir(parent);
    bool symlink = is_symlink(parent);
    if (symlink) {
        std::string path = "";
        readlink(parent, path);
        path_to_inum(path, parent);
    }
    if (!isdir(parent)) return IOERR;
    std::list<dirent> flist;
    readdir(parent, flist);
    for (std::list<dirent>::iterator i = flist.begin(); i != flist.end(); i++) {
        if (i->name.compare(name) == 0) {
            flist.erase(i);
            std::string buf;
            ec->remove(i->inum);
            for (std::list<dirent>::iterator i = flist.begin();
                 i != flist.end(); i++) {
                buf.append(to_str(i->name, i->inum));
            }
            ec->put(parent, buf);
            return OK;
        }
    }
    r = NOENT;
    return r;
}

int yfs_client::symlink(const char *link, inum parent, const char *name,
                        inum &ino_out) {
    std::cout << "[YC] [SYMLINK]" << parent << " " << name << " " << link <<  "\n";
    // create a new file, write path(link) into it
    int r = OK;

    // bool found = false;
    if (!isdir(parent)) return IOERR;
    std::string buf;
    ec->get(parent, buf);
    // create inode
    if (ec->create(extent_protocol::T_SYMLINK, ino_out) != OK) {
        return IOERR;
    }
    // Add an entry to parent
    buf.append(to_str(std::string(name), ino_out));
    ec->put(parent, buf);

    std::cout << "\t Create symlink file in parent ok\n";
    size_t written = 0;
    r = write(ino_out, strlen(link), 0, link, written);
    std::cout << "\t symlink returned " << r << "\n";
    return r;
}

std::string yfs_client::to_str(std::string fname, inum ino) {
    std::string package(fname);
    package.push_back('/');
    package.append(this->filename(ino) + "/");
    return package;
}

std::string yfs_client::get_filename(std::string buf) {
    return buf.substr(0, buf.find('/'));
}

yfs_client::inum yfs_client::get_ino(std::string buf) {
    size_t pos = buf.find('/');
    return get_ino(buf.substr(pos + 1));
}

int yfs_client::path_to_inum(std::string path, inum &ino_out) {
    std::string target = path;
    inum p = 0;

    std::list<dirent> dirlist;
    while (!target.empty()) {
        size_t pos = target.find('/');
        std::string ent = target.substr(0, pos);
        // cout << ent << "\n";
        if (pos == std::string::npos) pos = target.size() - 1;
        target.erase(0, pos + 1);
        readdir(p, dirlist);
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

int yfs_client::readlink(inum ino, std::string &path) {
    return ec->get(ino, path);
}
