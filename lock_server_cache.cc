// the caching lock server implementation

#include "lock_server_cache.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>

#include <sstream>

#include "handle.h"
#include "lang/verify.h"
#include "tprintf.h"

lock_server_cache::lock_server_cache() {
    rpc = new handle("");
    pthread_mutex_init(&lock, NULL);
    tprintf("* lock server started (cached)\n");
}

lock_server_cache::~lock_server_cache() {
    delete rpc;
}

int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id,
                               int &) {
    lock_protocol::status ret = lock_protocol::OK;
    // Your lab2 part3 code goes here
    pthread_mutex_lock(&lock);
    tprintf("[%llu] [%s] ACQ\n", lid, id.c_str());
    std::map<lock_protocol::lockid_t, lock_info>::iterator it = info.find(lid);
    if (it == info.end()) info[lid] = lock_info(lid);  // properly init
    lock_info &li = info[lid];
    // tprintf("%s\n", li.summary().c_str());

    if (li.free) {
        // the lock is free
        li.holder_id = id;
        li.free = false;
        li.revoke_sent = false;
        li.remove(id);
        ret = lock_protocol::OK;
        if (li.in_need() > 0) {
            // send revoke along
            handle h(li.holder_id);
            rpcc *cl = h.safebind();
            ret = lock_protocol::RPCERR;
            tprintf("directly send revoke RPC\n");
            if (cl) {
                li.revoke_sent = true;
                pthread_mutex_unlock(&lock);
                // unblock RPC
                int r;
                ret = cl->call(rlock_protocol::revoke, lid, r);
                pthread_mutex_lock(&lock);
            } else {
                tprintf("revoke RPC failed");
            }
        }
        // tprintf("GRANTED %llu to %s\n", lid, id.c_str());
    } else {
        // the lock is not free
        // add to wait list
        li.add(id);
        // revoke RPC to holder if we have not sent yet
        if (!li.revoke_sent) {
            handle h(li.holder_id);
            rpcc *cl = h.safebind();
            ret = lock_protocol::RPCERR;
            tprintf("[%llu] send revoke to %s\n", lid, li.holder_id.c_str());
            if (cl) {
                int r;
                li.revoke_sent = true;
                pthread_mutex_unlock(&lock);
                ret = cl->call(rlock_protocol::revoke, lid, r);
                pthread_mutex_lock(&lock);
            } else {
                tprintf("revoke RPC failed");
            }
        }
        ret = lock_protocol::RETRY;
    }

    // tprintf("[%llu] [%s] ACQ returns %d\n", lid, id.c_str(), ret);
    pthread_mutex_unlock(&lock);
    return ret;
}

int lock_server_cache::release(lock_protocol::lockid_t lid, std::string id,
                               int &r) {
    lock_protocol::status ret = lock_protocol::OK;
    pthread_mutex_lock(&lock);
    tprintf("[%llu] [%s] release lock\n", lid, id.c_str());
    lock_info &li = info[lid];
    if (li.free || li.holder_id != id) {
        tprintf("ERR! lock is free or held by another %d %s\n", li.free,
                li.holder_id.c_str());
        return 1;
    }
    li.free = true;
    li.revoke_sent = false;
    li.remove(id);
    // sends retry to next
    if (li.in_need() && !li.revoke_sent) {
        std::string next = li.next();
        handle h(next);
        rpcc *cl = h.safebind();
        ret = lock_protocol::RPCERR;
        tprintf("[%llu] send retry to %s\n", lid, next.c_str());
        if (cl) {
            int r;
            pthread_mutex_unlock(&lock);
            ret = cl->call(rlock_protocol::retry, lid, r);
            // tprintf("[%llu] [%s] retry RPC to %s returns %d\n", lid, id.c_str(),
            //         next.c_str(), ret);
            pthread_mutex_lock(&lock);
        } else {
            tprintf("revoke RPC failed");
        }
    }
    pthread_cond_signal(&(lock_cv[lid]));
    pthread_mutex_unlock(&lock);
    return ret;
}

lock_protocol::status lock_server_cache::stat(lock_protocol::lockid_t lid,
                                              int &r) {
    tprintf("stat request\n");
    // r = nacquire;
    pthread_mutex_lock(&lock);
    r = this->lock_state[lid];
    pthread_mutex_unlock(&lock);
    return lock_protocol::OK;
}

bool lock_server_cache::check() const {
    // info check
    for (std::map<lock_protocol::lockid_t, lock_info>::const_iterator i =
             info.begin();
         i != info.end(); i++) {
        lock_info in = i->second;
        if (!in.free) std::cerr << "lock " << in.lid << " is not free\n";
        if (in.revoke_sent) std::cerr << "lock " << in.lid << " revoke should be false\n";
        if (in.pending.size() > 0) std::cerr << "lock " << in.lid << " waiting list is not empty\n";
    }
    return true;
}
