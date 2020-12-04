#ifndef ydb_server_2pl_h
#define ydb_server_2pl_h

#include <map>
#include <string>

#include "extent_client.h"
#include "lock_client.h"
#include "lock_client_cache.h"
#include "lock_protocol.h"
#include "ydb_protocol.h"
#include "ydb_server.h"

class ydb_server_2pl : public ydb_server {
   public:
    ydb_server_2pl(std::string, std::string);
    ~ydb_server_2pl();
    ydb_protocol::status transaction_begin(int, ydb_protocol::transaction_id &);
    ydb_protocol::status transaction_commit(ydb_protocol::transaction_id,
                                            int &);
    ydb_protocol::status transaction_abort(ydb_protocol::transaction_id, int &);
    ydb_protocol::status get(ydb_protocol::transaction_id, const std::string,
                             std::string &);
    ydb_protocol::status set(ydb_protocol::transaction_id, const std::string,
                             const std::string, int &);
    ydb_protocol::status del(ydb_protocol::transaction_id, const std::string,
                             int &);

   private:
    ydb_protocol::transaction_id maxTxid;
    // Stores locks that a transaction acquired, transaction_id -> { lock_id }
    std::map<ydb_protocol::transaction_id,
             std::set<extent_protocol::extentid_t>>
        keySetMap;
    // Stores the snapshot of transactions, changes yet to be committed
    std::map<ydb_protocol::transaction_id, std::map<std::string, std::string>>
        txTmp;
    // transaction id -> { a set of locks in the process of acquiring }
    std::map<ydb_protocol::transaction_id,
             std::set<extent_protocol::extentid_t>>
        waitForLocks;
    // wait-for-graph
    std::vector<std::vector<ydb_protocol::transaction_id>> wfg;

    ydb_protocol::status tryAcquire(ydb_protocol::transaction_id txId,
                                    const std::string &key);
    void releaseLocks(ydb_protocol::transaction_id txId);
    void updateWfg(ydb_protocol::transaction_id id,
                   extent_protocol::extentid_t lockId);
    bool detectDeadLock() const;
};

#endif
