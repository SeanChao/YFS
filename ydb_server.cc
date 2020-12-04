#include "ydb_server.h"

#include <unordered_map>

#include "extent_client.h"

// #define DEBUG 1

// #define USE_CACHE

std::mutex PrintThread::_mutexPrint;

static long timestamp(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

ydb_server::ydb_server(std::string extent_dst, std::string lock_dst) {
    ec = new extent_client(extent_dst);
#ifdef USE_CACHE
    lc = new lock_client_cache(lock_dst);
#else
    lc = new lock_client(lock_dst);
#endif

    long starttime = timestamp();

    for (int i = 2; i < 1024; i++) {
        // for simplicity, just pre alloc all the needed inodes
        extent_protocol::extentid_t id;
        ec->create(extent_protocol::T_FILE, id);
    }

    long endtime = timestamp();
    printf("time %ld ms\n", endtime - starttime);
}

ydb_server::~ydb_server() {
    delete lc;
    delete ec;
}

// the first arg is not used, it is just a hack to the rpc lib
// no imply, just return OK
ydb_protocol::status ydb_server::transaction_begin(
    int, ydb_protocol::transaction_id &out_id) {
    txState[out_id] = STARTED;
    return ydb_protocol::OK;
}

ydb_protocol::status ydb_server::transaction_commit(
    ydb_protocol::transaction_id id, int &) {
    // no imply, just return OK
    txState[id] = COMMITTED;
    return ydb_protocol::OK;
}

ydb_protocol::status ydb_server::transaction_abort(
    ydb_protocol::transaction_id id, int &) {
    // no imply, just return OK
    txState[id] = ABORTED;
    return ydb_protocol::OK;
}

ydb_protocol::status ydb_server::get(ydb_protocol::transaction_id id,
                                     const std::string key,
                                     std::string &out_value) {
    // lab3: your code here
    extent_protocol::status stat = extent_protocol::OK;
    extent_protocol::extentid_t keyId = ydb_server::hashKey(key);
    extent_protocol::extentid_t inum = getInum(keyId);
    stat = ec->get(inum, out_value);
    LOG("? " << key << " [" << inum << "] -> " << out_value << std::endl);
    return stat;
}

ydb_protocol::status ydb_server::set(ydb_protocol::transaction_id id,
                                     const std::string key,
                                     const std::string value, int &) {
    extent_protocol::status ret = extent_protocol::OK;

    extent_protocol::extentid_t keyId = ydb_server::hashKey(key);
    eid_t inum = getInum(keyId);
    ret = ec->put(inum, value);
    LOG("+ " << key << " [" << inum << "] <- " << value << std::endl);
    return ret;
}

ydb_protocol::status ydb_server::del(ydb_protocol::transaction_id id,
                                     const std::string key, int &) {
    LOG("- " << key << std::endl);
    extent_protocol::status stat = extent_protocol::OK;
    extent_protocol::extentid_t keyId = ydb_server::hashKey(key);
    ec->put(keyId, "");
    return stat;
}
/**
 * hash a byte sequence into extentid_t key
 */
extent_protocol::extentid_t ydb_server::hashKey(const std::string key) {
    size_t h = std::hash<std::string>{}(key);
    extent_protocol::extentid_t hashed =  2 + h % 1023;
    // LOG("hash " << key << " -> " << hashed << std::endl);
    return hashed;
}

bool ydb_server::txUpdateable(ydb_protocol::transaction_id id) const {
    return txState.find(id)->second == STARTED;
}

eid_t ydb_server::getInum(eid_t key) {
    return key;
}
