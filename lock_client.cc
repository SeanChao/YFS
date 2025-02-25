// RPC stubs for clients to talk to lock_server

#include "lock_client.h"

#include <arpa/inet.h>
#include <stdio.h>

#include <iostream>
#include <sstream>

#include "rpc.h"

/*
 * Lock client: talk to server to acquire/release locks
 */

lock_client::lock_client(std::string dst) {
    sockaddr_in dstsock;
    make_sockaddr(dst.c_str(), &dstsock);
    cl = new rpcc(dstsock);
    if (cl->bind() < 0) {
        printf("lock_client: call bind\n");
    }
}

int lock_client::stat(lock_protocol::lockid_t lid) {
    int r;
    lock_protocol::status ret = cl->call(lock_protocol::stat, cl->id(), lid, r);
    VERIFY(ret == lock_protocol::OK);
    return r;
}

lock_protocol::status lock_client::acquire(lock_protocol::lockid_t lid) {
    // Your lab2 part2 code goes here
    int r;
    // printf("CLIENT try to acqurie %lld\n", lid);
    lock_protocol::status ret =
        cl->call(lock_protocol::acquire, cl->id(), lid, r);
    VERIFY(ret == lock_protocol::OK);
    // printf("CLIENT acquried %lld\n", lid);
    return r;
}

lock_protocol::status lock_client::release(lock_protocol::lockid_t lid) {
    // Your lab2 part2 code goes here
    int r;
    lock_protocol::status ret =
        cl->call(lock_protocol::release, cl->id(), lid, r);
    VERIFY(ret == lock_protocol::OK);
    // printf("CLIENT released %lld\n", lid);
    return r;
}
