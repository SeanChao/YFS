// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"

#include <stdio.h>

#include <iostream>
#include <sstream>
#include <pthread.h>

#include "rpc.h"
#include "tprintf.h"

// #define DEBUG
#ifdef DEBUG
#define LOG(x) printf(x)
#else
#define LOG(x) \
    do {       \
    } while (0)
#endif

// #define TID (pthread_self() % 23)
#define TID (tellme(this->id, pthread_self() % 23))

const char *tellme(std::string id, pthread_t tid) {
    std::stringstream ss;
    ss << "[" << id << " " << tid << "]";
    return ss.str().c_str();
}

int lock_client_cache::last_port = 0;

lock_client_cache::lock_client_cache(std::string xdst,
                                     class lock_release_user *_lu)
    : lock_client(xdst), lu(_lu) {
    srand(time(NULL) ^ last_port);
    rlock_port = ((rand() % 32000) | (0x1 << 10));
    const char *hname;
    // VERIFY(gethostname(hname, 100) == 0);
    hname = "127.0.0.1";
    std::ostringstream host;
    host << hname << ":" << rlock_port;
    id = host.str();
    last_port = rlock_port;
    rpcs *rlsrpc = new rpcs(rlock_port);
    rlsrpc->reg(rlock_protocol::revoke, this,
                &lock_client_cache::revoke_handler);
    rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);
    if (pthread_mutex_init(&mutex, NULL)) {
        std::cerr << "lock client mutex init failed!" << std::endl;
    }
}

lock_protocol::status lock_client_cache::acquire(lock_protocol::lockid_t lid) {
    pthread_mutex_lock(&mutex);
    // last_locked = "acq";
    // tprintf("%s acquire %llu\n", TID, lid);
    int ret = lock_protocol::OK;
    bool done = false;
    while (!done) {
        // tprintf("%s [lock %llu] [state %d]\n", TID, lid, lock_state[lid]);
        switch (lock_state[lid]) {
            case NONE: {
                int r;
                lock_state[lid] = ACQUIRING;
                // tprintf("%s [lock %llu] [state %d] sending AC RPC\n", TID,
                // lid, lock_state[lid]);
                // reset retry_received (this run is invoked by a retry)
                if (retry_recv[lid]) retry_recv[lid] = false;
                pthread_mutex_unlock(&mutex);
                ret = cl->call(lock_protocol::acquire, lid, id, r);
                pthread_mutex_lock(&mutex);
                // last_locked = "acq after rpc";
                // tprintf("%s ACQ RPC: %d\n", TID, ret);
                switch (ret) {
                    case lock_protocol::OK:
                        lock_state[lid] = LOCKED;
                        ret = lock_protocol::OK;
                        done = true;
                        break;
                    case lock_protocol::RETRY:
                        // tprintf("%s waiting for retry...\n", TID);
                        if (!retry_recv[lid]) {
                            // waiting for retry rpc
                            pthread_cond_wait(&(retry_cv[lid]), &mutex);
                            // tprintf("%s awake from retry, lock->FREE\n", TID);
                            lock_state[lid] = FREE;
                        } else {
                            // tprintf("%s [%llu] already got retry\n", TID, lid);
                        }
                        lock_state[lid] = NONE;  // resend acquire RPC
                    default:
                        break;
                }
                break;
            }
            case FREE:
                // tprintf("%s %llu get lock\n", TID, lid);
                ret = lock_protocol::OK;
                lock_state[lid] = LOCKED;
                done = true;
                break;
            case ACQUIRING:
            case LOCKED:
                // should wait until one thread release the lock
                if (lock_free.find(lid) == lock_free.end())
                    pthread_cond_init(&(lock_free[lid]), NULL);
                // tprintf("%s %llu waiting for lock free\n", TID, lid);
                pthread_cond_wait(&(lock_free[lid]), &mutex);
                // tprintf("%s %llu waiting for lock free OK\n", TID, lid);
                ret = lock_protocol::OK;
                break;
            case RELEASING:
                break;
        }
    }
    pthread_mutex_unlock(&mutex);
    return ret;
}

lock_protocol::status lock_client_cache::release(lock_protocol::lockid_t lid) {
    int ret = lock_protocol::OK;
    // Your lab2 part3 code goes here
    pthread_mutex_lock(&mutex);
    if (lock_state[lid] == NONE) {
        // tprintf("%s release %llu (already released [NONE])\n", TID, lid);
        return lock_protocol::OK;
    }
    // tprintf("%s release loc [%llu] (revoked: %d\n", TID, lid, revoke_recv[lid]);
    if (revoke_recv[lid]) {
        // return the lock back to server
        // tprintf("%s %llu return back to server\n", TID, lid);
        lock_state[lid] = NONE;
        revoke_recv[lid] = false;
        pthread_mutex_unlock(&mutex);
        int r;  // no use
        ret = cl->call(lock_protocol::release, lid, id, r);
        pthread_mutex_lock(&mutex);
    } else {
        lock_state[lid] = FREE;
        pthread_cond_signal(&(lock_free[lid]));
    }
    // tprintf("%s release %llu OK\n", TID, lid);
    pthread_mutex_unlock(&mutex);
    return ret;
}

rlock_protocol::status lock_client_cache::revoke_handler(
    lock_protocol::lockid_t lid, int &) {
    int ret = rlock_protocol::OK;
    pthread_mutex_lock(&mutex);
    tprintf("%s <- revoke %llu\n", TID, lid);
    revoke_recv[lid] = true;
    if (lock_state[lid] == FREE) {
        tprintf("%s revoke -> release\n", TID);
        pthread_mutex_unlock(&mutex);
        release(lid);
        pthread_mutex_lock(&mutex);
        pthread_mutex_unlock(&mutex);
        return rlock_protocol::OK;
    }
    pthread_mutex_unlock(&mutex);
    return ret;
}

rlock_protocol::status lock_client_cache::retry_handler(
    lock_protocol::lockid_t lid, int &) {
    int ret = rlock_protocol::OK;
    // Your lab2 part3 code goes here
    pthread_mutex_lock(&mutex);
    // tprintf("%s <- retry %llu\n", TID, lid);
    retry_recv[lid] = true;
    pthread_cond_signal(&(retry_cv[lid]));
    pthread_mutex_unlock(&mutex);
    return ret;
}

bool lock_client_cache::check() {
    for (std::unordered_map<lid_t, LOCK_STATE>::const_iterator i = lock_state.begin();
         i != lock_state.end(); i++) {
        // std::cout << "lock " << i->first << ": " << i->second << std::endl;
    }
    for (std::map<lid_t, bool>::const_iterator i = retry_recv.begin();
         i != retry_recv.end(); i++) {
        // std::cout << "retry recv " << i->first << " " << i->second << std::endl;
    }
    for (std::map<lid_t, bool>::const_iterator i = revoke_recv.begin();
         i != revoke_recv.end(); i++) {
        // std::cout << "revoke recv " << i->first << " " << i->second
        //           << std::endl;
    }
    // std::cout << "last locked: " << last_locked << "\n";
    return true;
}