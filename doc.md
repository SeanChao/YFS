# YDB Documentation

**The code includes c++17 features**

## Part1: Simple Database without Transaction

`ydb` is a simple KV-store. Both key and value are `std::string`.

`ydb_server` creates all inodes during initialization.

For a given `key`, it is hashed to a number in a fix range based on the inodes initialization, in this case it's 2 to 1024. We call `std::hash` to get an `unsigned long long` hash code and then do the mapping. The value of the key is stored in an inode whose id is the hashed value.

For a `get` request, we make a `get` RPC with `extent_client` to get the value of the key.

For a `set` request, we make a `set` RPC with `extent_client` to set the value of the key.

## Part2: Two Phase Locking

### Transaction Support

Firstly, we add transaction support to `ydb_server`, both `ydb_server_2pl` and `ydb_server_occ` extend from `ydb_server`, so we make common transaction support in `ydb_server` to reduce redundant code.

We add an enum `transactionState` to indicate the state of a transaction, including `NONE`, `STARTED`, `COMMITTED` and `ABORTED`,
a map `ydb_protocol::transaction_id -> transactionState` to record the state for every transaction
and a function `txUpdateable` to verify whether a transaction is still ongoing.

`transaction_begin`, `transaction_commit` and `transaction_abort` are implemented to updated transaction states.

### Two Phase Locking

Each `get` and `set` request will call `tryAcquire` function to acquire the corresponding lock for each shared variable. 

`tryAcquire` is a imdempotent lock acquiration function. The `ydb_server_2pl` class maintains a map `std::map<ydb_protocol::transaction_id, std::set<extent_protocol::extentid_t> > keySetMap`
to record all acquired locks for every transaction. Therefore, it'll never try to acquire an acquired lock.

Each ongoing transaction has a cache to record it's read and write result called `txTmp`. All `get` and `set` will operate on it. A cache miss will lead to an actual read or write.
In commit operation, all updates are applied.

Once a transaction is to be committed or aborted, all locks are released. Since the transaction is not ongoing anymore, we'll not acquire any other lock.

### Deadlock Detection

To detect an operation that will lead to a deadlock, we maintain a wait-for-graph `wfg`. In the WFG, a node is a transaction and an edge indicates that a transaction is trying to acquire a lock another transaction is currently holding.

Before a lock is acquired, we perform a deadlock check `detectDeadLock`. It simply traverse the WFG and check if the `WFG` contains a cycle. If so, the current transaction will be aborted.

## Part3: Optimistic Concurrency Control

OCC implementation is intuitional. `ydb_server_occ` has `txRSet` and `txWSet` to handle local read and write.

Before commit, it'll check every variable in the read set. If some of them are modified, the transaction will be aborted.
