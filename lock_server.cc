// the lock server implementation

#include "lock_server.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>

#include <sstream>

/*
Lock server: grant a lock to clients, one at a time
Correctness: at any point in time, there is at most one client holding a lock
with a given identifier.

Additional Requirement: acquire() at client does not
return until lock is granted

Each lock is identified by an integer of type lock_protocol::lockid_t. The set
of locks is open-ended: if a client asks for a lock that the server has never
seen before, the server should create the lock and grant it to the client. When
multiple clients request the same lock, the lock server must grant the lock to
one client at a time.
 */

lock_server::lock_server() : nacquire(0) {
    printf("* lock server started");
    pthread_mutex_init(&lock, NULL);
}

lock_protocol::status lock_server::stat(int clt, lock_protocol::lockid_t lid,
                                        int &r) {
    lock_protocol::status ret = lock_protocol::OK;
    // printf("stat request from clt %d\n", clt);
    r = nacquire;
    pthread_mutex_lock(&lock);
    r = this->lock_state[lid];
    pthread_mutex_unlock(&lock);
    return ret;
}

lock_protocol::status lock_server::acquire(int clt, lock_protocol::lockid_t lid,
                                           int &r) {
    lock_protocol::status ret = lock_protocol::OK;
    // std::cout << "ACQ " << lid << std::endl;
    pthread_mutex_lock(&lock);
    // std::cout << "get global lock ok\n";
    std::map<lock_protocol::lockid_t, pthread_cond_t>::iterator cit =
        lock_cv.find(lid);
    if (cit == lock_cv.end()) {
        // The lockid is new
        lock_state[lid]++;
        pthread_cond_init(&(lock_cv[lid]), NULL);
    } else {
        // std::cout << "SERVER waiting for cv " << lid << " lock state "
        //           << lock_state[lid] << " to " << clt << "\n";
        // No race condition since lock_state is locked
        while (lock_state[lid] > 0) pthread_cond_wait(&(cit->second), &lock);
        lock_state[lid]++;
    }
    pthread_mutex_unlock(&lock);
    r = 0;
    // std::cout << "SERVER granted " << lid << " to " << clt << "\n";
    return ret;
}

lock_protocol::status lock_server::release(int clt, lock_protocol::lockid_t lid,
                                           int &r) {
    lock_protocol::status ret = lock_protocol::OK;
    // Your lab2 part2 code goes here
    pthread_mutex_lock(&lock);
    printf("%d release %lld\n", clt, lid);
    std::map<lock_protocol::lockid_t, int>::iterator it = lock_state.find(lid);
    if (it->second == 0) return lock_protocol::OK;
    it->second--;
    pthread_cond_signal(&(lock_cv[lid]));
    r = 0;
    pthread_mutex_unlock(&lock);
    return ret;
}
