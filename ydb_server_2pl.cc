#include "ydb_server_2pl.h"

#include <algorithm>

#include "extent_client.h"
#define DEBUG 1

ydb_server_2pl::ydb_server_2pl(std::string extent_dst, std::string lock_dst)
    : ydb_server(extent_dst, lock_dst), maxTxid(0) {}

ydb_server_2pl::~ydb_server_2pl() {}

// the first arg is not used, it is just a hack to the rpc lib
ydb_protocol::status ydb_server_2pl::transaction_begin(
    int _, ydb_protocol::transaction_id &out_id) {
    // lab3: your code here
    lc->acquire(0);
    out_id = ++maxTxid;
    LOG("* Start new TX " << out_id << "\n");
    ydb_server::transaction_begin(_, out_id);
    lc->release(0);
    return ydb_protocol::OK;
}

ydb_protocol::status ydb_server_2pl::transaction_commit(
    ydb_protocol::transaction_id id, int &unused) {
    // lab3: your code here
    ydb_server::transaction_commit(id, unused);
    // write updates
    auto &updateSet = txTmp[id];
    for (auto &kvPair : updateSet) {
        ydb_server::set(id, kvPair.first, kvPair.second, unused);
    }
    releaseLocks(id);
    wfg[id].clear();
    return ydb_protocol::OK;
}

ydb_protocol::status ydb_server_2pl::transaction_abort(
    ydb_protocol::transaction_id id, int &unused) {
    // lab3: your code here
    ydb_server::transaction_abort(id, unused);
    releaseLocks(id);
    wfg[id].clear();
    return ydb_protocol::OK;
}

ydb_protocol::status ydb_server_2pl::get(ydb_protocol::transaction_id id,
                                         const std::string key,
                                         std::string &out_value) {
    // lab3: your code here
    if (!txUpdateable(id)) {
        return ydb_protocol::TRANSIDINV;
    }
    auto st = tryAcquire(id, key);
    if (st == ydb_protocol::ABORT) return st;
    auto &updateSet = txTmp[id];
    if (updateSet.find(key) == updateSet.end()) {
        // Not yet in update set, fetch it
        auto status = ydb_server::get(id, key, out_value);
        VERIFY(status == ydb_protocol::OK);
        updateSet[key] = out_value;
        return ydb_protocol::OK;
    }
    out_value = updateSet[key];
    return ydb_protocol::OK;
}

ydb_protocol::status ydb_server_2pl::set(ydb_protocol::transaction_id id,
                                         const std::string key,
                                         const std::string value, int &) {
    // lab3: your code here
    if (!txUpdateable(id)) {
        return ydb_protocol::TRANSIDINV;
    }
    LOG("set: TX" << id << " is to acquire " << key << "\n");
    auto st = tryAcquire(id, key);
    LOG("set: TX" << id << " acquired " << key << "\n");
    if (st == ydb_protocol::ABORT) return st;

    auto &updateSet = txTmp[id];

    updateSet[key] = value;
    return ydb_protocol::OK;
}

ydb_protocol::status ydb_server_2pl::del(ydb_protocol::transaction_id id,
                                         const std::string key, int &_) {
    set(id, key, "", _);
    return ydb_protocol::OK;
}

ydb_protocol::status ydb_server_2pl::tryAcquire(
    ydb_protocol::transaction_id txId, const std::string &key) {
    auto st = ydb_protocol::OK;
    lc->acquire(0);
    auto &keySet = keySetMap[txId];
    auto hash = hashKey(key);

    if (keySet.find(hash) == keySet.end()) {
        auto &myWaitFor = waitForLocks[txId];
        myWaitFor.insert(hash);
        updateWfg(txId, hash);
        if (detectDeadLock()) {
            LOG("ABORT" << txId << "!\n");
            lc->release(0);
            int _;
            transaction_abort(txId, _);
            return ydb_protocol::ABORT;
        }
        lc->release(0);
        lock_protocol::status lockSt = lc->acquire(hash);
        if (lockSt != lock_protocol::OK) {
            std::cerr << "acquire lock " << key << "[" << hash << "] failed!\n";
        }

        lc->acquire(0);
        myWaitFor.erase(myWaitFor.find(hash));
        keySet.insert(hash);
    }
    lc->release(0);
    return st;
}

void ydb_server_2pl::releaseLocks(ydb_protocol::transaction_id txId) {
    lc->acquire(0);
    auto &keySet = keySetMap[txId];
    LOG("* release all locks "
        << "for TX " << txId << " " << keySet.size() << "\n");
    for (auto &lock : keySet) {
        auto st = lc->release(lock);
        if (st != lock_protocol::OK) {
            std::cerr << "release lock " << lock << " failed!\n";
        }
    }
    keySetMap.erase(txId);
    lc->release(0);
}

void ydb_server_2pl::updateWfg(ydb_protocol::transaction_id txId,
                               extent_protocol::extentid_t lockId) {
    while (wfg.size() <= (size_t)txId) {
        wfg.push_back(std::vector<ydb_protocol::transaction_id>());
    }
    auto &wfgVec = wfg[txId];
    const auto &waitSet = waitForLocks[txId];
    // std::vector<ydb_protocol::transaction_id> empty =
    // std::vector<ydb_protocol::transaction_id>();
    wfgVec.clear();
    for (auto &&lockId : waitSet) {
        LOG(txId << " check for lock " << lockId << "\n");
        for (auto &&tx : keySetMap) {
            if (tx.first == txId) continue;
            LOG(txId << " check for " << lockId << " in TX " << tx.first
                     << "\n");
            auto &locks = tx.second;
            if (locks.find(lockId) != locks.end()) {
                wfgVec.push_back(tx.first);
                LOG(txId << " -> " << tx.first << "\n");
            }
        }
    }
}

bool ydb_server_2pl::detectDeadLock() const {
    // cycle detection in wfg
    auto size = wfg.size();
    std::vector<bool> visited(size, false);
    for (size_t i = 0; i < size; i++) {
        std::vector<ydb_protocol::transaction_id> stack;
        std::vector<ydb_protocol::transaction_id> path;
        if (visited[i]) continue;
        const auto &vec = wfg[i];
        if (vec.size() == 0) continue;
        // DFS
        stack.push_back(i);
        while (!stack.empty()) {
            auto cur = stack.end() - 1;
            auto id = *cur;
            stack.erase(cur);
            if (std::find(path.begin(), path.end(), id) != path.end()) {
                LOG("CYCLE: ");
                for (auto &&t : path) {
                    std::cout << t << " -> ";
                }
                std::cout << id << "\n";
                return true;
            }
            visited[id] = true;
            path.push_back(id);
            if (wfg[id].size() == 0) path.pop_back();
            for (auto &adj : wfg[id]) {
                stack.push_back(adj);
            }
        }
    }
    return false;
}
