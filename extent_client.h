// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <memory>
#include <string>

#include "extent_protocol.h"
#include "extent_server.h"

class extent_client {
   protected:
    rpcc *cl;

   public:
    extent_client(std::string dst);

    virtual extent_protocol::status create(uint32_t type,
                                           extent_protocol::extentid_t &eid);
    virtual extent_protocol::status get(extent_protocol::extentid_t eid,
                                        std::string &buf);
    virtual extent_protocol::status getattr(extent_protocol::extentid_t eid,
                                            extent_protocol::attr &a);
    virtual extent_protocol::status put(extent_protocol::extentid_t eid,
                                        std::string buf);
    virtual extent_protocol::status remove(extent_protocol::extentid_t eid);
    /**
     * flush cached data (if any)
     */
    virtual extent_protocol::status flush(extent_protocol::extentid_t eid);
};

class cached_file {
   public:
    bool attrValid;
    bool dataValid;
    // An effective remove operation
    bool remove;
    bool dataDirty;  // file data has been modified
    extent_protocol::attr attr;
    std::string data;
};

class extent_client_cache : public extent_client {
   private:
    std::map<extent_protocol::extentid_t, std::shared_ptr<cached_file>> cache;
    std::shared_ptr<cached_file> setCachedFileData(
        extent_protocol::extentid_t id, std::string &buf);
    std::shared_ptr<cached_file> setCachedFileAttr(
        extent_protocol::extentid_t id, extent_protocol::attr &a);
    std::shared_ptr<cached_file> cachedGet(extent_protocol::extentid_t id);
    std::shared_ptr<cached_file> lookup(extent_protocol::extentid_t id) const;
    std::shared_ptr<cached_file> cacheRemove(extent_protocol::extentid_t id);

   public:
    extent_client_cache(std::string dst);
    extent_protocol::status create(uint32_t type,
                                   extent_protocol::extentid_t &eid);
    extent_protocol::status get(extent_protocol::extentid_t eid,
                                std::string &buf);
    extent_protocol::status getattr(extent_protocol::extentid_t eid,
                                    extent_protocol::attr &a);
    extent_protocol::status put(extent_protocol::extentid_t eid,
                                std::string buf);
    extent_protocol::status remove(extent_protocol::extentid_t eid);
    extent_protocol::status fullget(extent_protocol::extentid_t eid,
                                    std::string &buf);
    virtual extent_protocol::status flush(extent_protocol::extentid_t eid);
};

#endif