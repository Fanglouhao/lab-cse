#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>


#include <map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"


class lock_server_cache {
 private:
  
    struct WaitList
    {
        std::string id;
        WaitList *tail;
        WaitList(std::string i) {id = i; tail = NULL;}
        void insert(std::string i)
        {
            WaitList *next = new WaitList(i);
            WaitList *thisList = this;
            while(thisList -> tail != NULL)
            {
                thisList = thisList -> tail;
            }
            thisList -> tail = next;
        }
    };
    
    struct lock_status
    {
        bool ifLocked;
        std::string client;
        WaitList* waitList;
        lock_status(std::string c){ifLocked = false; client = c; waitList = NULL;}
    };
    
    std::map<lock_protocol::lockid_t, lock_status*> lock_status_map;
    rlock_protocol::status RPC_call(lock_protocol::lockid_t, std::string, int);
    pthread_mutex_t map_mutex;
    
  int nacquire;
 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif
