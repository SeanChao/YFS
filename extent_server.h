// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <map>
#include <string>

#include "extent_protocol.h"
#include "inode_manager.h"

class extent_server {
   protected:
#if 0
  typedef struct extent {
    std::string data;
    struct extent_protocol::attr attr;
  } extent_t;
  std::map <extent_protocol::extentid_t, extent_t> extents;
#endif
    inode_manager *im;

   public:
    extent_server();

    int create(uint32_t type, extent_protocol::extentid_t &id);
    int create_n_file(int n, std::vector<extent_protocol::extentid_t> &vec);
    int put(extent_protocol::extentid_t id, std::string, int &);
    int get(extent_protocol::extentid_t id, std::string &);
    int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
    int remove(extent_protocol::extentid_t id, int &);

   private:
    // only preallocate FILE inodes, NOT DIR/LINK
    std::vector<extent_protocol::extentid_t> preallocated;
};

#endif
