#ifndef ydb_server_occ_h
#define ydb_server_occ_h

#include <map>
#include <string>

#include "extent_client.h"
#include "lock_client.h"
#include "lock_client_cache.h"
#include "lock_protocol.h"
#include "ydb_protocol.h"
#include "ydb_server.h"

class ydb_server_occ : public ydb_server {
   public:
    ydb_server_occ(std::string, std::string);
    ~ydb_server_occ();
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
    ydb_protocol::transaction_id maxTxId;
    // Stores the snapshot of transactions, changes yet to be committed
    std::map<ydb_protocol::transaction_id, std::map<std::string, std::string>>
        txRSet;
    std::map<ydb_protocol::transaction_id, std::map<std::string, std::string>>
        txWSet;
};

#endif
