// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"
#include <unistd.h>


int lock_client_cache::last_port = 0;

lock_client_cache::lock_client_cache(std::string xdst, 
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  srand(time(NULL)^last_port);
  rlock_port = ((rand()%32000) | (0x1 << 10));
  char hname[100];
  VERIFY(gethostname(hname, sizeof(hname)) == 0);
  std::ostringstream host;
  host << hname << ":" << rlock_port;
  id = host.str();
  last_port = rlock_port;
  rpcs *rlsrpc = new rpcs(rlock_port);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  int ret = lock_protocol::OK;
    pthread_mutex_lock(&map_mutex);
    if(lock_map.find(lid) == lock_map.end())
    {
        lock_status* new_lock = new lock_status();
        lock_map.insert(std::pair<lock_protocol::lockid_t, lock_status*>(lid, new_lock));
    }
    lock_status* theLock = lock_map[lid];
    if(theLock -> status == NOLOCK)
    {
        theLock -> status = ACQUIRING;
        pthread_mutex_unlock(&map_mutex);
        ret = get_RPC_ret_acquire(lid, theLock);
    }
    else if(theLock -> status == FREE)
    {
        theLock -> status = LOCKED;
        pthread_mutex_unlock(&map_mutex);
    }
    else
    {
        while((theLock -> status != NOLOCK) && (theLock -> status != FREE))
        {
            pthread_cond_wait(&(theLock -> lock_status_cv), &map_mutex);
        }
        if(theLock -> status == NOLOCK)
        {
            theLock -> status = ACQUIRING;
            pthread_mutex_unlock(&map_mutex);
            ret = get_RPC_ret_acquire(lid, theLock);
        }
        else if(theLock -> status == FREE)
        {
            theLock -> status = LOCKED;
            pthread_mutex_unlock(&map_mutex);
        }
    }
  return ret;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
    lock_protocol::status ret = lock_protocol::OK;
    pthread_mutex_lock(&map_mutex);
    if(lock_map.find(lid) == lock_map.end())
    {
        pthread_mutex_unlock(&map_mutex);
        return lock_protocol::NOENT;
    }
    lock_status* theLock = lock_map[lid];
    if(theLock -> status != LOCKED)
    {
        pthread_mutex_unlock(&map_mutex);
        return lock_protocol::NOENT;
    }
    else
    {
        if(theLock -> ifRevoked == false)
        {
            theLock -> status = FREE;
            pthread_cond_signal(&theLock -> lock_status_cv);
            pthread_mutex_unlock(&map_mutex);
        }
        else
        {
            theLock -> status = RELEASING;
            theLock -> ifRevoked = false;
            pthread_mutex_unlock(&map_mutex);
            ret = get_RPC_ret_release(lid, theLock);
        }
    }
    return ret;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                  int &)
{
    rlock_protocol::status ret = rlock_protocol::OK;
    pthread_mutex_lock(&map_mutex);
    
    if(lock_map.find(lid) == lock_map.end())
    {
        pthread_mutex_unlock(&map_mutex);
        return ret;
    }
    lock_status* theLock = lock_map[lid];
    if((theLock -> status == LOCKED) || (theLock -> status == ACQUIRING) || (theLock -> status == RELEASING))
    {
        theLock -> ifRevoked = true;
        pthread_mutex_unlock(&map_mutex);
    }
    else if(theLock -> status == FREE)
    {
        theLock -> status = RELEASING;
        theLock -> ifRevoked = false;
        pthread_mutex_unlock(&map_mutex);
        ret = get_RPC_ret_release(lid, theLock);
    }
    else
    {
        pthread_mutex_unlock(&map_mutex);
    }
    
    return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                 int &)
{
    rlock_protocol::status ret = rlock_protocol::OK;
    pthread_mutex_lock(&map_mutex);
    if(lock_map.find(lid) == lock_map.end())
    {
        pthread_mutex_unlock(&map_mutex);
        return ret;
    }
    lock_status * theLock = lock_map[lid];
    if(theLock -> status == ACQUIRING)
    {
        theLock -> status = LOCKED;
    }
    pthread_mutex_unlock(&map_mutex);
    return ret;
}

bool
lock_client_cache::ifLocked(lock_status* theLock)
{
    pthread_mutex_lock(&map_mutex);
    bool ret = (theLock -> status == LOCKED);
    pthread_mutex_unlock(&map_mutex);
    return ret;
}

lock_protocol::status
lock_client_cache::get_RPC_ret_acquire(lock_protocol::lockid_t lid, lock_status* theLock)
{
    int r;
    lock_protocol::status ret = cl -> call(lock_protocol::acquire, lid, id, r);
    while(ret != lock_protocol::OK)
    {
        if(ifLocked(theLock))
        {
            break;
        }
        if(ret != lock_protocol::RETRY)
        {
            ret = cl -> call(lock_protocol::acquire, lid, id, r);
        }
    }
    pthread_mutex_lock(&map_mutex);
    theLock -> status = LOCKED;
    pthread_mutex_unlock(&map_mutex);
    return ret;
}

lock_protocol::status
lock_client_cache::get_RPC_ret_release(lock_protocol::lockid_t lid, lock_status* theLock)
{
    int r;
    while((cl -> call(lock_protocol::release, lid, id, r)) != lock_protocol::OK)
    {
    }
    lock_protocol::status ret = lock_protocol::OK;
    pthread_mutex_lock(&map_mutex);
    theLock -> status = NOLOCK;
    pthread_mutex_unlock(&map_mutex);
    pthread_cond_signal(&theLock -> lock_status_cv);
    return ret;
    
}
