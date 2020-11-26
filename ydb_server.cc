#include "ydb_server.h"

#include <unordered_map>

#include "extent_client.h"

#define DEBUG 1

#ifdef DEBUG
#define LOG(x) std::cout << x;
#else
#define LOG(x) \
    do {       \
    } while (0)
#endif

static long timestamp(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

ydb_server::ydb_server(std::string extent_dst, std::string lock_dst) {
    ec = new extent_client(extent_dst);
    lc = new lock_client(lock_dst);
    // lc = new lock_client_cache(lock_dst);

    long starttime = timestamp();

    // for (int i = 2; i < 1024; i++) {
    //     // for simplicity, just pre alloc all the needed inodes
    //     extent_protocol::extentid_t id;
    //     ec->create(extent_protocol::T_FILE, id);
    // }

    long endtime = timestamp();
    printf("time %ld ms\n", endtime - starttime);
}

ydb_server::~ydb_server() {
    delete lc;
    delete ec;
}

ydb_protocol::status ydb_server::transaction_begin(
    int,
    ydb_protocol::transaction_id &out_id) {  // the first arg is not used, it is
                                             // just a hack to the rpc lib
                                             // no imply, just return OK
    LOG("transaction begin\n")
    return ydb_protocol::OK;
}

ydb_protocol::status ydb_server::transaction_commit(
    ydb_protocol::transaction_id id, int &) {
    // no imply, just return OK
    return ydb_protocol::OK;
}

ydb_protocol::status ydb_server::transaction_abort(
    ydb_protocol::transaction_id id, int &) {
    // no imply, just return OK
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
    LOG("? " << key << " [" << inum << "] -> " << out_value << std::endl)
    return stat;
}

ydb_protocol::status ydb_server::set(ydb_protocol::transaction_id id,
                                     const std::string key,
                                     const std::string value, int &) {
    // lab3: your code here
    extent_protocol::status ret = extent_protocol::OK;

    extent_protocol::extentid_t keyId = ydb_server::hashKey(key);
    // extent_protocol::extentid_t
    eid_t inum = getInum(keyId);
    if (inum == superNodeId) {
        // create an file
        create(keyId, inum);
    }
    ret = ec->put(inum, value);
    LOG("+ " << key << " [" << inum << "] <- " << value << std::endl)
    return ret;
}

ydb_protocol::status ydb_server::del(ydb_protocol::transaction_id id,
                                     const std::string key, int &) {
    // lab3: your code here
    LOG("- " << key << std::endl)
    extent_protocol::status stat = extent_protocol::OK;
    extent_protocol::extentid_t keyId = ydb_server::hashKey(key);
    std::map<extent_protocol::extentid_t, extent_protocol::extentid_t>::iterator
        it = key2fileMap.find(keyId);
    if (it == key2fileMap.end()) return ydb_protocol::OK;
    stat = ec->remove(key2fileMap[keyId]);
    return stat;
}
/**
 * hash a byte sequence into extentid_t key
 */
extent_protocol::extentid_t ydb_server::hashKey(const std::string key) {
    size_t h = std::hash<std::string>{}(key);
    extent_protocol::extentid_t hashed = h % 1024;
    LOG("hash " << key << " -> " << hashed << std::endl)
    return hashed;
}

ydb_protocol::status ydb_server::create(eid_t key, eid_t &id) {
    extent_protocol::status st;
    if (getInum(key) != superNodeId) return 0;
    std::string kvArray;
    st = ec->get(superNodeId, kvArray);
    eid_t newId;
    st = ec->create(extent_protocol::T_FILE, newId);
    eid_t buf[2];
    buf[0] = key;
    buf[1] = newId;
    std::string stringBuf;
    stringBuf.assign((char *)buf, 2 * sizeof(eid_t));
    kvArray.replace(kvArray.size(), 2 * sizeof(eid_t), stringBuf);
    ec->put(superNodeId, kvArray);
    id = newId;
    return st;
}

eid_t ydb_server::getInum(eid_t key) {
    extent_protocol::status st;
    std::string kvArray;
    st = ec->get(superNodeId, kvArray);
    size_t size = kvArray.size() / sizeof(eid_t) / 2;
    eid_t *kvRawArray = (eid_t *)(kvArray.data());
    for (size_t i = 0; i < size; i++) {
        eid_t fKey = kvRawArray[2 * i];
        eid_t fInum = kvRawArray[2 * i + 1];
        if (fKey == key) return fInum;
    }
#ifdef LOG
    LOG("+-----------+\n")
    LOG("size = " << size << "\n")
    for (size_t i = 0; i < size; i++) {
        eid_t fKey = kvRawArray[2 * i];
        eid_t fInum = kvRawArray[2 * i + 1];
        LOG(fKey << " -> " << fInum << std::endl)
    }
#endif
    // denote not found
    return superNodeId;
}