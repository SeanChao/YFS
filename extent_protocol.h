// extent wire protocol

#ifndef extent_protocol_h
#define extent_protocol_h

#include "rpc.h"

class extent_protocol {
   public:
    typedef int status;
    typedef unsigned long long extentid_t;
    enum xxstatus { OK, RPCERR, NOENT, IOERR };
    enum rpc_numbers {
        put = 0x6001,
        get,
        getattr,
        remove,
        create,
        create_n_file,
    };

    enum types {
        T_DIR = 1,
        T_FILE,
        T_SYMLINK,
    };

    struct attr {
        uint32_t type;
        unsigned int atime;
        unsigned int mtime;
        unsigned int ctime;
        unsigned int size;
    };
};

inline unmarshall &operator>>(unmarshall &u, extent_protocol::attr &a) {
    u >> a.type;
    u >> a.atime;
    u >> a.mtime;
    u >> a.ctime;
    u >> a.size;
    return u;
}

inline marshall &operator<<(marshall &m, extent_protocol::attr a) {
    m << a.type;
    m << a.atime;
    m << a.mtime;
    m << a.ctime;
    m << a.size;
    return m;
}

inline unmarshall &operator>>(unmarshall &u,
                              std::vector<extent_protocol::extentid_t> &vec) {
    unsigned size;
    u >> size;
    for (uint32_t i = 0; i < size; i++) {
        extent_protocol::extentid_t t;
        u >> t;
        vec.push_back(t);
    }
    return u;
}

inline marshall &operator<<(marshall &m,
                            std::vector<extent_protocol::extentid_t> vec) {
    m << (unsigned)(vec.size());
    for (auto it = vec.begin(); it != vec.end(); it++) {
        m << *it;
    }
    return m;
}

#endif
