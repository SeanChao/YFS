// RPC stubs for clients to talk to extent_server

#include "extent_client.h"

#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <ctime>
#include <iostream>
#include <sstream>

using std::shared_ptr;
using std::unique_ptr;

// #define DEBUG 42
#ifdef DEBUG
#define LOG(f_, ...) printf((f_), __VA_ARGS__)
#else
#define LOG(f_, ...) \
    do {       \
    } while (0)
#endif

extent_client::extent_client(std::string dst) {
    sockaddr_in dstsock;
    make_sockaddr(dst.c_str(), &dstsock);
    cl = new rpcc(dstsock);
    if (cl->bind() != 0) {
        printf("extent_client: bind failed\n");
    }
}

extent_protocol::status extent_client::create(uint32_t type,
                                              extent_protocol::extentid_t &id) {
    extent_protocol::status ret = extent_protocol::OK;
    ret = cl->call(extent_protocol::create, type, id);
    return ret;
}

extent_protocol::status extent_client::get(extent_protocol::extentid_t eid,
                                           std::string &buf) {
    extent_protocol::status ret = extent_protocol::OK;
    ret = cl->call(extent_protocol::get, eid, buf);
    return ret;
}

extent_protocol::status extent_client::getattr(extent_protocol::extentid_t eid,
                                               extent_protocol::attr &attr) {
    extent_protocol::status ret = extent_protocol::OK;
    ret = cl->call(extent_protocol::getattr, eid, attr);
    return ret;
}

extent_protocol::status extent_client::put(extent_protocol::extentid_t eid,
                                           std::string buf) {
    extent_protocol::status ret = extent_protocol::OK;
    int r;
    ret = cl->call(extent_protocol::put, eid, buf, r);
    return ret;
}

extent_protocol::status extent_client::remove(extent_protocol::extentid_t eid) {
    extent_protocol::status ret = extent_protocol::OK;
    int r;
    ret = cl->call(extent_protocol::remove, eid, r);
    return ret;
}

extent_protocol::status extent_client::flush(extent_protocol::extentid_t eid) {
    // No cache, do nothing
    return extent_protocol::OK;
}

/**
 * Cached Version
 */

shared_ptr<cached_file> extent_client_cache::cachedGet(
    extent_protocol::extentid_t id) {
    auto file = cache[id];
    if (file == NULL) {
        // cache miss: fetch data
        std::string buf;
        extent_client::get(id, buf);
        auto filePtr = std::make_shared<cached_file>();
        filePtr->data = buf;
        cache[id] = filePtr;
        return filePtr;
    }
    return file;
}

shared_ptr<cached_file> extent_client_cache::lookup(
    extent_protocol::extentid_t id) const {
    if (cache.find(id) == cache.end()) return NULL;
    auto file = cache.at(id);
    return file;
}

shared_ptr<cached_file> extent_client_cache::setCachedFileData(
    extent_protocol::extentid_t id, std::string &buf) {
    auto &fp = cache[id];
    if (fp == NULL) fp = std::make_shared<cached_file>();
    fp->dataValid = true;
    fp->data = buf;
    fp->attrValid = false;
    return fp;
}

shared_ptr<cached_file> extent_client_cache::setCachedFileAttr(
    extent_protocol::extentid_t id, extent_protocol::attr &a) {
    auto &fp = cache[id];
    if (fp == NULL) fp = std::make_shared<cached_file>();
    fp->attrValid = true;
    fp->attr = a;
    return fp;
}

shared_ptr<cached_file> extent_client_cache::cacheRemove(
    extent_protocol::extentid_t id) {
    auto &fp = cache[id];
    if (fp == NULL) fp = std::make_shared<cached_file>();
    fp->remove = true;
    return fp;
}

extent_client_cache::extent_client_cache(std::string dst)
    : extent_client(dst) {}

extent_protocol::status extent_client_cache::create(
    uint32_t type, extent_protocol::extentid_t &eid) {
    extent_protocol::status st = extent_protocol::OK;
    // RPC: get an id
    st = extent_client::create(type, eid);
    LOG("CREATE type %u id %llu\n", type, eid);
    // create in cache
    VERIFY(st == extent_protocol::OK);
    auto file = std::make_shared<cached_file>();
    file->dataValid = true;
    file->attrValid = true;
    // less consistency
    auto t = std::time(NULL);
    file->attr.ctime = t;
    file->attr.atime = t;
    file->attr.mtime = t;
    file->attr.type = type;
    file->attr.size = 0;
    file->data = "";
    cache[eid] = file;
    return st;
}

extent_protocol::status extent_client_cache::get(
    extent_protocol::extentid_t eid, std::string &buf) {
    extent_protocol::status st = extent_protocol::OK;
    auto file = lookup(eid);
    if (file && file->dataValid) {
        buf = file->data;
        // update atime
        // FIXME: getattr first and set valid bit
        file->attr.atime = std::time(nullptr);
        LOG("GET cached %llu: ^%s$\n", eid, buf.c_str());
    } else {
        st = extent_client::get(eid, buf);
        if (st != extent_protocol::OK) return st;
        setCachedFileData(eid, buf);
        LOG("GET %llu: %s\n", eid, buf.c_str());
    }
    return st;
}

extent_protocol::status extent_client_cache::getattr(
    extent_protocol::extentid_t eid, extent_protocol::attr &a) {
    extent_protocol::status st = extent_protocol::OK;
    auto file = lookup(eid);
    if (file && file->attrValid) {
        a = file->attr;
        LOG("GETATTR cached %llu size=%u\n", eid, a.size);
    } else {
        st = extent_client::getattr(eid, a);
        if (st != extent_protocol::OK) return st;
        setCachedFileAttr(eid, a);
        LOG("GETATTR %llu size=%u\n", eid, a.size);
    }
    return st;
}

extent_protocol::status extent_client_cache::put(
    extent_protocol::extentid_t eid, std::string buf) {
    extent_protocol::status st = extent_protocol::OK;
    auto file = lookup(eid);
    if (file && file->dataValid) {
        file->data = buf;
        if (!file->attrValid) extent_client::getattr(eid, file->attr);
        file->attrValid = true;
        file->attr.size = buf.size();
        time_t now = std::time(nullptr);
        file->attr.mtime = now;  // less consistency
        file->attr.ctime = now;
        file->dataDirty = true;
        LOG("PUT cached %llu %s\n", eid, buf.c_str());
    } else {
        st = extent_client::put(eid, buf);
        if (st != extent_protocol::OK) return st;
        setCachedFileData(eid, buf);
        LOG("PUT %llu %s\n", eid, buf.c_str());
    }
    return st;
}

extent_protocol::status extent_client_cache::remove(
    extent_protocol::extentid_t eid) {
    extent_protocol::status st = extent_protocol::OK;
    auto file = lookup(eid);
    if (file) {
        LOG("REMOVE cached %llu\n", eid);
        file->remove = true;
    } else {
        // cache miss
        LOG("REMOVE %llu\n", eid);
        st = extent_client::remove(eid);
    }
    return st;
}

extent_protocol::status extent_client_cache::fullget(
    extent_protocol::extentid_t eid, std::string &buf) {
    std::cout << "FULL GET";
    cl->call(extent_protocol::fullget, eid, buf);
    std::cout << buf << std::endl;
    return extent_protocol::OK;
}

extent_protocol::status extent_client_cache::flush(
    extent_protocol::extentid_t eid) {
    extent_protocol::status st = extent_protocol::OK;
    auto file = lookup(eid);
    if (file) {
        if (file->dataDirty) {
            extent_client::put(eid, file->data);
            LOG("FLUSH: %llu put\n", eid);
        }
        if (file->remove) {
            extent_client::remove(eid);
            LOG("FLUSH: %llu remove\n", eid);
        }
        cache.erase(eid);
    }
    return st;
}
