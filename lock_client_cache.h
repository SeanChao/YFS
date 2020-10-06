// lock client interface.

#ifndef lock_client_cache_h

#define lock_client_cache_h

#include <string>

#include "lang/verify.h"
#include "lock_client.h"
#include "lock_protocol.h"
#include "rpc.h"

// Classes that inherit lock_release_user can override dorelease so that
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 6.
class lock_release_user {
   public:
    virtual void dorelease(lock_protocol::lockid_t) = 0;
    virtual ~lock_release_user(){};
};

typedef lock_protocol::lockid_t lid_t;

class lock_client_cache : public lock_client {
   public:
    class lock_release_user *lu;
    int rlock_port;
    std::string hostname;
    std::string id;
    pthread_mutex_t mutex;
    enum LOCK_STATE { NONE, FREE, ACQUIRING, LOCKED, RELEASING };
    std::map<lid_t, LOCK_STATE> lock_state;
    std::map<lid_t, pthread_cond_t> retry_cv;  // cv: retry RPC
    std::map<lid_t, bool> retry_recv;
    std::map<lid_t, pthread_cond_t> lock_free;  // cv: the cached lock is free
    std::map<lid_t, bool> revoke_recv;  // if the lock had received revoke
    std::string last_locked;

   public:
    static int last_port;
    lock_client_cache(std::string xdst, class lock_release_user *l = 0);
    virtual ~lock_client_cache(){};
    lock_protocol::status acquire(lock_protocol::lockid_t);
    lock_protocol::status release(lock_protocol::lockid_t);
    rlock_protocol::status revoke_handler(lock_protocol::lockid_t, int &);
    rlock_protocol::status retry_handler(lock_protocol::lockid_t, int &);
    bool check() ;
};

#endif
