#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <map>
#include <sstream>
#include <string>

#include "handle.h"
#include "lock_protocol.h"
#include "lock_server.h"
#include "rpc.h"

class lock_info {
   public:
    lock_protocol::lockid_t lid;
    bool free;
    std::string holder_id;
    bool revoke_sent;
    std::list<std::string> pending;  // clients waiting for lock<lid>

    lock_info() : free(true), revoke_sent(false) {}
    lock_info(lock_protocol::lockid_t lid)
        : lid(lid), free(true), revoke_sent(false) {}

    void add(std::string id) {
        for (std::list<std::string>::iterator i = pending.begin();
             i != pending.end(); i++) {
            if (*i == id) return;
        }
        pending.push_back(id);
    }

    void remove(std::string id) {
        for (std::list<std::string>::iterator i = pending.begin();
             i != pending.end();) {
            if (*i == id)
                i = pending.erase(i);
            else
                i++;
        }
    }

    std::string next() const {
        return in_need() ? *pending.begin() : "";
    }

    size_t in_need() const {
        return pending.size();
    }

    std::string summary() const {
        std::string waiting = "[ ";
        for (std::list<std::string>::const_iterator i = pending.begin();
             i != pending.end(); i++) {
            waiting += *i + " ";
        }
        waiting += "]";
        std::stringstream ss;
        ss << lid
           << " " + (free ? " FREE" : "LOCKED by " + holder_id) + " " + waiting
           << (revoke_sent ? " R" : "");
        return ss.str();
    }
};

class lock_server_cache {
   private:
    int nacquire;
    handle *rpc;
    std::map<lock_protocol::lockid_t, int> lock_state;
    std::map<lock_protocol::lockid_t, pthread_cond_t> lock_cv;
    std::map<lock_protocol::lockid_t, lock_info> info;
    pthread_mutex_t lock;

   public:
    lock_server_cache();
    ~lock_server_cache();
    lock_protocol::status stat(lock_protocol::lockid_t, int &);
    int acquire(lock_protocol::lockid_t, std::string id, int &);
    int release(lock_protocol::lockid_t, std::string id, int &);
    bool check() const;
};

#endif
