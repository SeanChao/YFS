#ifndef ydb_server_h
#define ydb_server_h

#include <map>
#include <set>
#include <string>
#include <vector>

#include "extent_client.h"
#include "lock_client.h"
#include "lock_client_cache.h"
#include "lock_protocol.h"
#include "ydb_protocol.h"
    typedef extent_protocol::extentid_t eid_t;
class ydb_server {
   protected:

    static const eid_t superNodeId = 1;
    extent_client *ec;
    lock_client *lc;

    ydb_protocol::status create(eid_t key, eid_t &id);
    eid_t getInum(eid_t key);

   public:
    ydb_server(std::string, std::string);
    virtual ~ydb_server();
    virtual ydb_protocol::status transaction_begin(
        int, ydb_protocol::transaction_id &);
    virtual ydb_protocol::status transaction_commit(
        ydb_protocol::transaction_id, int &);
    virtual ydb_protocol::status transaction_abort(ydb_protocol::transaction_id,
                                                   int &);
    virtual ydb_protocol::status get(ydb_protocol::transaction_id,
                                     const std::string, std::string &);
    virtual ydb_protocol::status set(ydb_protocol::transaction_id,
                                     const std::string, const std::string,
                                     int &);
    virtual ydb_protocol::status del(ydb_protocol::transaction_id,
                                     const std::string, int &);

    static extent_protocol::extentid_t hashKey(const std::string key);

   private:
    std::map<extent_protocol::extentid_t, extent_protocol::extentid_t>
        key2fileMap;
};

#endif
