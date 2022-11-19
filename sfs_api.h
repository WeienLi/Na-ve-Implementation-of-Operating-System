#include <stdbool.h>
#ifndef SFS_API_H
#define SFS_API_H

// You can add more into this file.
#define OLD_DISK_NAME "sfs_disk"
#define NUMBER_OF_INODES 100
#define NUMBER_OF_BLOCKS 2000
#define BLOCK_SIZE 1024
#define MAGIC_NUMBER 0xACBD0005
#define ROOT_INODE 0
#define SUPERBLOCK_BLOCK 0
#define FIRST_INODE_BLOCK 1
#define MAXFILENAME 35  //with extension and dot
#define MAX_FILE_BLOCKS (12+256)

typedef struct inode { //from instructions
    bool free;
    int mode;
    int link_cnt;
    int uid;
    int gid;
    int size;
    int pointer[12];
    int indirect;
}inode;

typedef struct FileDescriptor{
    bool free;
    int inodenumber;
    int rwpointer;
}fd;

typedef struct super_block{
    int magicblock;
    int blocksize;
    int FSsize; //file system size
    int inodetablelength;
    int rootinode;
}sb;

typedef struct directory{
    bool free;
    int inodenumber;
    //filename is maxlength + 1 to account for null terminator
    char filename[MAXFILENAME+1];
}directory;

void mksfs(int);

int sfs_getnextfilename(char*);

int sfs_getfilesize(const char*);

int sfs_fopen(char*);

int sfs_fclose(int);

int sfs_fwrite(int, char*, int); //has const

int sfs_fread(int, char*, int);

int sfs_fseek(int, int);

int sfs_remove(char*);

#endif
