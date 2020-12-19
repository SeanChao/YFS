// the extent server implementation

#include "extent_server.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <sstream>

extent_server::extent_server() {
    im = new inode_manager();
}

int extent_server::create(uint32_t type, extent_protocol::extentid_t &id) {
    // alloc a new inode and return inum
    id = im->alloc_inode(type);
    // printf("extent_server: create inode %llu\n", id);

    return extent_protocol::OK;
}

int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &) {
    // printf(">extent_server: put %llu\n", id);
    id &= 0x7fffffff;
    const char *cbuf = buf.data();
    int size = static_cast<int>(buf.size());
    im->write_file(id, cbuf, size);
    // printf("<extent_server: put inode=%llu, %u bytes\n", id, size);
    return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf) {
    // printf(">extent_server: get %llu\n", id);
    id &= 0x7fffffff;

    int size = 0;
    char *cbuf = NULL;

  im->read_file(id, &cbuf, &size);
  if (size == 0)
    buf = "";
  else {
    // buf.assign(cbuf, size);
    buf = std::string(cbuf, size);
    free(cbuf);
  }

    // printf("<extent_server: get %llu\n", id);
    return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id,
                           extent_protocol::attr &a) {
    // printf(">extent_server: getattr %lld\n", id);

    id &= 0x7fffffff;

    extent_protocol::attr attr;
    memset(&attr, 0, sizeof(attr));
    im->getattr(id, attr);
    a = attr;

    // printf("<extent_server: getattr %lld\n", id);
    return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &) {
    // printf(">extent_server: remove %lld\n", id);

    id &= 0x7fffffff;
    im->remove_file(id);

    // printf("<extent_server: remove %lld\n", id);
    return extent_protocol::OK;
}

int extent_server::fullget(extent_protocol::extentid_t id, std::string &buf) {
    std::cout << "extent_server: fullget";
    buf = "YES!";
    return extent_protocol::OK;
}
