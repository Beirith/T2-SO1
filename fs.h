#ifndef FS_H
#define FS_H

#include <vector>
#include "disk.h"

class INE5412_FS
{
public:
    static const unsigned int FS_MAGIC = 0xf0f03410;
    static const unsigned short int INODES_PER_BLOCK = 128;
    static const unsigned short int POINTERS_PER_INODE = 5;
    static const unsigned short int POINTERS_PER_BLOCK = 1024;

    class aux {
        public:
            int ninodeblocks;
            int ninodes;
            int direct;
            int indirect;
            int pointers;
    };

    class fs_bitmap {
        public:
            std::vector<bool> free_blocks;
    };

    class fs_superblock {
        public:
            unsigned int magic;
            int nblocks;
            int ninodeblocks;
            int ninodes;
    }; 

    class fs_inode {
        public:
            int isvalid;
            int size;
            int direct[POINTERS_PER_INODE];
            int indirect;
    };

    union fs_block {
        public:
            fs_superblock super;
            fs_inode inode[INODES_PER_BLOCK];
            int pointers[POINTERS_PER_BLOCK];
            char data[Disk::DISK_BLOCK_SIZE];
    };

public:

    INE5412_FS(Disk *d) {
        disk = d;
        mounted = false;
    } 

    void fs_debug();
    int  fs_format();
    int  fs_mount();

    int  fs_create();
    int  fs_delete(int inumber);
    int  fs_getsize(int inumber);

    int  fs_read(int inumber, char *data, int length, int offset);
    int  fs_write(int inumber, const char *data, int length, int offset);

private:
    Disk *disk;
    bool mounted;
    fs_bitmap bitmap;

    void inode_load(int inumber, class fs_inode *inode);
    void inode_save(int inumber, class fs_inode *inode);
    void inode_format(class fs_inode *inode);
    void print_bitmap(int nblocks);
    void fs_test();
};

#endif