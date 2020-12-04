#include "ydb_server_occ.h"

#include "extent_client.h"

// #define DEBUG 1

ydb_server_occ::ydb_server_occ(std::string extent_dst, std::string lock_dst)
    : ydb_server(extent_dst, lock_dst), maxTxId(0) {}

ydb_server_occ::~ydb_server_occ() {}

// the first arg is not used, it is just a hack to the rpc lib
ydb_protocol::status ydb_server_occ::transaction_begin(
    int _, ydb_protocol::transaction_id &out_id) {
    lc->acquire(0);
    out_id = ++maxTxId;
    LOG("* Start new TX " << out_id << "\n");
    ydb_server::transaction_begin(_, out_id);
    lc->release(0);
    return ydb_protocol::OK;
}

ydb_protocol::status ydb_server_occ::transaction_commit(
    ydb_protocol::transaction_id id, int &_) {
    /**
     * Validation in critical section
     * Validates whether serializability is guaranteed: Has any tuple in the
     * read set been modified?
     */
    lc->acquire(0);
    auto &myRead = txRSet[id];
    for (auto &&kv : myRead) {
        std::string read;
        ydb_server::get(id, kv.first, read);
        if (read != kv.second) {
            lc->release(0);
            return ydb_protocol::ABORT;
        }
    }
    /**
     * Install the write set and commit
     */
    auto &updateSet = txWSet[id];
    for (auto &kvPair : updateSet) {
        ydb_server::set(id, kvPair.first, kvPair.second, _);
    }
    ydb_server::transaction_commit(id, _);
    lc->release(0);
    return ydb_protocol::OK;
}

ydb_protocol::status ydb_server_occ::transaction_abort(
    ydb_protocol::transaction_id id, int &_) {
    ydb_server::transaction_abort(id, _);
    return ydb_protocol::OK;
}

ydb_protocol::status ydb_server_occ::get(ydb_protocol::transaction_id id,
                                         const std::string key,
                                         std::string &out_value) {
    if (!txUpdateable(id)) {
        return ydb_protocol::TRANSIDINV;
    }
    lc->acquire(0);
    auto &txRead = txRSet[id];
    lc->release(0);
    auto st = ydb_server::get(id, key, out_value);
    if (st != ydb_protocol::OK) return st;
    if (txRead.find(key) == txRead.end()) txRead[key] = out_value;
    out_value = txRead[key];
    lc->acquire(0);
    auto &txWrite = txWSet[id];
    lc->release(0);
    if (txWrite.find(key) != txWrite.end()) out_value = txWrite[key];
    return ydb_protocol::OK;
}

ydb_protocol::status ydb_server_occ::set(ydb_protocol::transaction_id id,
                                         const std::string key,
                                         const std::string value, int &_) {
    if (!txUpdateable(id)) {
        return ydb_protocol::TRANSIDINV;
    }
    auto &txWrite = txWSet[id];
    txWrite[key] = value;
    return ydb_protocol::OK;
}

ydb_protocol::status ydb_server_occ::del(ydb_protocol::transaction_id id,
                                         const std::string key, int &_) {
    set(id, key, "", _);
    return ydb_protocol::OK;
}
