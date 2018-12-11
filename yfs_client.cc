// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)


yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
    ec = new extent_client(extent_dst);
    lc = new lock_client_cache(lock_dst);
    lc->acquire(1);
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n");
    lc->release(1);
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
    lc->acquire(inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        lc->release(inum);
        return false;
    }
    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        lc->release(inum);
        return true;
    }
    printf("isfile: %lld is not a file\n", inum);
    lc->release(inum);
    return false;
}

/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 *
 * */
//====================================================================================//
// format: "inum|name/inum|name/inum|name" //
yfs_client::directoryList::directoryList()
{
    
}

void
yfs_client::directoryList::createList(std::string path)
{
    unsigned int index = 0;
    unsigned int find_target = 0;
    while(index < path.size())
    {
        find_target = path.find('|', index);
        std::string inum_str = path.substr(index, find_target - index);
        index = find_target + 1;
        inum theInum = n2i(inum_str);
        find_target = path.find('/', index);
        std::string filename = path.substr(index, find_target - index);
        index = find_target + 1;
        dirent theDirent;
        theDirent.name = filename;
        theDirent.inum = theInum;
        dir_list.push_back(theDirent);
    }
}

std::string
yfs_client::directoryList::toString()
{
    std::string theStr = "";
    for (std::list<dirent>::iterator it = dir_list.begin(); it != dir_list.end(); it++)
    {
        std::string inum_str = filename(it -> inum);
        std::string filename = it -> name;
        theStr.append(inum_str);
        theStr.append("|");
        theStr.append(filename);
        theStr.append("/");
    }
    return theStr;
}

void
yfs_client::directoryList::add(inum theInum, std::string filename)
{
    dirent theDirent;
    theDirent.name = filename;
    theDirent.inum = theInum;
    dir_list.push_back(theDirent);
}

void
yfs_client::directoryList::del(std::string filename)
{
    std::list<dirent>::iterator it;
    for (it = dir_list.begin(); it != dir_list.end(); it++)
    {
        if(it -> name == filename)
        {
            break;
        }
    }
    if(it != dir_list.end())
    {
        dir_list.erase(it);
    }
}

std::list <yfs_client::dirent>
yfs_client::directoryList::getlist()
{
    return dir_list;
}

void
yfs_client::directoryList::lookup(std::string filename, inum& theInum, bool& found)
{
    std::list<dirent>::iterator it;
    for (it = dir_list.begin(); it != dir_list.end(); it++)
    {
        if(it -> name == filename)
        {
            break;
        }
    }
    if (it != dir_list.end()){
        found = true;
        theInum = it -> inum;
    }
    else
    {
        found = false;
        theInum = -1;
    }
}
//====================================================================================//
int
yfs_client::readlink(inum ino, std::string &link)
{
    lc->acquire(ino);
    int r = OK;
    EXT_RPC(ec->get(ino, link));
    
release:
    lc->release(ino);
    return r;
}



int
yfs_client::symlink(inum parent, const char *name, const char *link, inum &ino_out)
{
    lc->acquire(parent);
    int r = OK;
    directoryList dirList;
    std::string buf;
    EXT_RPC(ec->get(parent, buf));
    dirList.createList(buf);
    EXT_RPC(ec->create(extent_protocol::T_SYMLINK, ino_out));
    EXT_RPC(ec->put(ino_out, std::string(link)));
    dirList.add(ino_out, std::string(name));
    EXT_RPC(ec->put(parent, dirList.toString()));
release:
    lc->release(parent);
    return r;
}

bool
yfs_client::issymlink(inum inum)
{
    lc->acquire(inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK){
        lc->release(inum);
        return false;
    }
    lc->release(inum);
    return (a.type == extent_protocol::T_SYMLINK);
}

bool
yfs_client::isdir(inum inum)
{
    lc->acquire(inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK){
        lc->release(inum);
        return false;
    }
    lc->release(inum);
    return (a.type == extent_protocol::T_DIR);
}


int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    lc->acquire(inum);
    int r = OK;
    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    
    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);
    
release:
    lc->release(inum);
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    lc->acquire(inum);
    int r = OK;
    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;
    
release:
    lc->release(inum);
    return r;
}

// Only support set size of attr
int
yfs_client::setattr(inum ino, size_t size)
{
    lc->acquire(ino);
    int r = OK;
    /*
     * your code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */
    std::string buf;
    EXT_RPC(ec->get(ino, buf));
    if(size != buf.size())
    {
        buf.resize(size, '\0');
        EXT_RPC(ec->put(ino, buf));
    }
release:
    lc->release(ino);
    return r;
}


int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    lc->acquire(parent);
    int r = OK;
    /*
     * your code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    std::string buf;
    inum theInum;
    bool found = false;
    directoryList dirList;
    EXT_RPC(ec->get(parent, buf));
    dirList.createList(buf);
    dirList.lookup((std::string)name, theInum, found);
    if (found){
        lc->release(parent);
        return EXIST;
    }
    EXT_RPC(ec->create(extent_protocol::T_FILE, ino_out));
    dirList.add(ino_out, std::string(name));
    EXT_RPC(ec->put(parent, dirList.toString()));
    
release:
    lc->release(parent);
    return r;
}

int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    lc->acquire(parent);
    int r = OK;
    /*
     * your code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    std::string buf;
    inum theInum;
    bool found = false;
    directoryList dirList;
    EXT_RPC(ec->get(parent, buf));
    dirList.createList(buf);
    dirList.lookup((std::string)name, theInum, found);
    if (found){
        lc->release(parent);
        return EXIST;
    }
    EXT_RPC(ec->create(extent_protocol::T_DIR, ino_out));
    dirList.add(ino_out, std::string(name));
    EXT_RPC(ec->put(parent,dirList.toString()));
    
release:
    lc->release(parent);
    return r;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    lc->acquire(parent);
    int r = OK;
    found = false;
    /*
     * your code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
    std::string buf;
    directoryList dirList;
    EXT_RPC(ec->get(parent, buf));
    dirList.createList(buf);
    dirList.lookup((std::string)name, ino_out, found);
    
release:
    lc->release(parent);
    return r;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    lc->acquire(dir);
    int r = OK;
    /*
     * your code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */
    std::string buf;
    directoryList dirList;
    EXT_RPC(ec->get(dir, buf));
    dirList.createList(buf);
    list = dirList.getlist();
    
release:
    lc->release(dir);
    return r;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    lc->acquire(ino);
    int r = OK;
    /*
     * your code goes here.
     * note: read using ec->get().
     */
    std::string buf;
    unsigned int size_read_to_end;
    EXT_RPC(ec->get(ino, buf));
    size_read_to_end = buf.size() - off;
    if(size > size_read_to_end)
    {
        data = buf.substr(off, size_read_to_end);
    }
    else
    {
        data = buf.substr(off, size);
    }
    
release:
    lc->release(ino);
    return r;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
                  size_t &bytes_written)
{
    lc->acquire(ino);
    int r = OK;
    /*
     * your code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */
    std::string buf;
    std::string data_replace = std::string(data, size);
    EXT_RPC(ec->get(ino, buf));
    if (off + size > buf.size()){
        buf.resize(off + size, '\0');
    }
    buf.replace(off, size, data_replace);
    EXT_RPC(ec->put(ino, buf));
    bytes_written = size;
    
release:
    lc->release(ino);
    return r;
}


int
yfs_client::unlink(inum parent,const char *name)
{
    lc->acquire(parent);
    int r = OK;
    /*
     * your code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
    std::string buf;
    inum theInum;
    bool found = false;
    directoryList dirList;
    EXT_RPC(ec->get(parent, buf));
    dirList.createList(buf);
    dirList.lookup((std::string)name, theInum, found);
    if (!found){
        lc->release(parent);
        return NOENT;
    }
    dirList.del(std::string(name));
    EXT_RPC(ec->remove(theInum));
    EXT_RPC(ec->put(parent, dirList.toString()));
    
release:
    lc->release(parent);
    return r;
}
