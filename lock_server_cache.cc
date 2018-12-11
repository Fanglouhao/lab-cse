// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"


lock_server_cache::lock_server_cache()
{
}


int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
                               int &)
{
  lock_protocol::status ret = lock_protocol::OK;
    pthread_mutex_lock(&map_mutex);
    if(lock_status_map.find(lid) == lock_status_map.end())
    {
        lock_status* new_lock = new lock_status(id);
        lock_status_map.insert(std::pair<lock_protocol::lockid_t, lock_status*>(lid, new_lock));
    }
    lock_status* theLock = lock_status_map[lid];
    if(!(theLock -> ifLocked))
    {
        theLock -> ifLocked = true;
        theLock -> client = id;
        pthread_mutex_unlock(&map_mutex);
    }
    else
    {
        if(theLock -> waitList != NULL)
        {
            theLock -> waitList -> insert(id);
            ret = lock_protocol::RETRY;
            pthread_mutex_unlock(&map_mutex);
        }
        else
        {
            theLock -> waitList = new WaitList(id);
            ret = lock_protocol::RETRY;
            pthread_mutex_unlock(&map_mutex);
            while(RPC_call(lid, theLock -> client, rlock_protocol::revoke) != rlock_protocol::OK)
            {
            }
        }
    }
  return ret;
}

int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
         int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
    pthread_mutex_lock(&map_mutex);
    if(lock_status_map.find(lid) == lock_status_map.end())
    {
        pthread_mutex_unlock(&map_mutex);
        return lock_protocol::NOENT;
    }
    lock_status* theLock = lock_status_map[lid];
    if(!(theLock -> ifLocked))
    {
        ret = lock_protocol::NOENT;
        pthread_mutex_unlock(&map_mutex);
    }
    else
    {
        if(theLock -> client != id)
        {
            ret = lock_protocol::NOENT;
            pthread_mutex_unlock(&map_mutex);
        }
        else
        {
            if(theLock -> waitList == NULL)
            {
                theLock -> ifLocked = false;
                theLock -> client = "";
                pthread_mutex_unlock(&map_mutex);
            }
            else
            {
                theLock -> ifLocked = true;
                theLock -> client = theLock -> waitList -> id;
                theLock -> waitList = theLock -> waitList -> tail;
                pthread_mutex_unlock(&map_mutex);
                while(RPC_call(lid, theLock -> client, rlock_protocol::retry) != rlock_protocol::OK)
                {
                }
                while(RPC_call(lid, theLock -> client, rlock_protocol::revoke) != rlock_protocol::OK)
                {
                }
            }
        }
    }
    
  return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

rlock_protocol::status
lock_server_cache::RPC_call(lock_protocol::lockid_t lid, std::string id, int protocol)
{
    int r;
    lock_protocol::status ret = lock_protocol::OK;
    
    handle h(id);
    if(h.safebind())
    {
        ret = h.safebind() -> call(protocol, lid, r);
    }
    else
    {
        ret = lock_protocol::RPCERR;
    }
    return ret;
}
