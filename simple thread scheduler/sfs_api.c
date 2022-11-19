#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include "disk_emu.h"
#include "sfs_api.h"
#include <stdbool.h>

inode inodetable[NUMBER_OF_INODES];

fd fdtable[NUMBER_OF_INODES];

directory directorytable[NUMBER_OF_INODES];

sb superblock;
int bitmap[NUMBER_OF_BLOCKS];

// locate where getnextfile should get global pointer
int location = 0;
void flush(){
    //set the buffer to zeros
    char buffer[20000];
    memset(buffer, 0, sizeof(buffer));

    //flush superBlock to disk
    memcpy(buffer, &superblock, sizeof(superblock));
    write_blocks(0, 1, buffer);

    memset(buffer, 0, sizeof(buffer));

    //flush inode table to disk
    memcpy(buffer, inodetable, sizeof(inodetable));
    write_blocks(1, superblock.inodetablelength, buffer);

    memset(buffer, 0, sizeof(buffer));

    //flush directory table to disk
    memcpy(buffer, directorytable, sizeof(directorytable));
    write_blocks(inodetable[superblock.rootinode].pointer[0], 12, buffer);

    memset(buffer, 0, sizeof(buffer));

    //flush bitmap to disk
    memcpy(buffer, bitmap, sizeof(bitmap));
    write_blocks(NUMBER_OF_BLOCKS-(sizeof(bitmap)/BLOCK_SIZE + 1), (sizeof(bitmap)/BLOCK_SIZE + 1), buffer);

}
void mksfs(int fresh){
    if(fresh){
        init_fresh_disk(OLD_DISK_NAME, BLOCK_SIZE, NUMBER_OF_BLOCKS);
        for(int i = 0; i < NUMBER_OF_INODES; i++){
            inodetable[i].free = true;
            fdtable[i].free = true;
            directorytable[i].free = true;
        }
        for(int j = 0; j < NUMBER_OF_BLOCKS; j++){
            bitmap[j] = 1;
        }
        superblock.blocksize = BLOCK_SIZE;
        superblock.FSsize = NUMBER_OF_BLOCKS;
        superblock.inodetablelength = sizeof(inodetable)/BLOCK_SIZE + 1;
        superblock.magicblock = MAGIC_NUMBER;
        superblock.rootinode = ROOT_INODE;
        bitmap[0] = 0;  //superblock
        bitmap[NUMBER_OF_BLOCKS-1] = 0; //bitmap block
        for(int i = NUMBER_OF_BLOCKS-(sizeof(bitmap)/BLOCK_SIZE+1); i < NUMBER_OF_BLOCKS ; i++){
            bitmap[i] = 0;
        }
        for(int i = 1; i <= superblock.inodetablelength ; i++){
            bitmap[i] = 0;
        }
        inodetable[superblock.rootinode].free = false;
        int forallocate = 0;
        for(int i = superblock.inodetablelength+1; i <= superblock.inodetablelength+12; i++){
            bitmap[i] = 0;
            inodetable[superblock.rootinode].pointer[forallocate] = i;
            forallocate++;
            if(forallocate == 12) break;
        }
        inodetable[superblock.rootinode].indirect = -1;
        inodetable[superblock.rootinode].link_cnt = 12;
        inodetable[superblock.rootinode].size = sizeof(directorytable);
        inodetable[superblock.rootinode].mode = 0; 
    }
    else{
        init_disk(OLD_DISK_NAME, BLOCK_SIZE, NUMBER_OF_BLOCKS);
        for(int i = 0; i < NUMBER_OF_INODES; i++){
            fdtable[i].free = true;
        }
        //obtain the rest from the disk by copying to buffer then setting it to approriate block/table
        char buffer[20000];
        memset(buffer, 0, sizeof(buffer));

        read_blocks(0, 1, buffer);
        memcpy(&superblock, buffer, sizeof(superblock));

        read_blocks(1, superblock.inodetablelength, buffer);
        memcpy(inodetable, buffer, sizeof(inodetable));

        read_blocks(inodetable[superblock.rootinode].pointer[0], 12, buffer);
        memcpy(directorytable, buffer, sizeof(directorytable));

        read_blocks(NUMBER_OF_BLOCKS-(sizeof(bitmap)/BLOCK_SIZE + 1), (sizeof(bitmap)/BLOCK_SIZE + 1), buffer);
        memcpy(bitmap, buffer, sizeof(bitmap));
    }
}

int sfs_getnextfilename(char *fname){

    for(int i = location; i < NUMBER_OF_INODES; i++){
        directory check = directorytable[i];
        if(!check.free){
            strcpy(fname, check.filename);
            i++;
            location = i;
            return 1;
        }
    }
    //if no next file
    location = 0;
    return 0;
}
int sfs_getfilesize(const char *path){
    for(int i = 0 ; i < NUMBER_OF_INODES; i++){
        directory check = directorytable[i];
        if(!check.free){
            if(strcmp(check.filename, path) == 0){
                return inodetable[check.inodenumber].size;
            }
        }
    }
    return -1;
}

int sfs_fopen(char *name){
    if(strlen(name) > MAXFILENAME){
        return -1;
    }
    for(int i = 0; i < NUMBER_OF_INODES; i++){
        directory check = directorytable[i];
        if(!check.free){
            if(strcmp(check.filename, name) == 0){
                for(int z = 0; z < NUMBER_OF_INODES; z++){
                    if(!fdtable[z].free){
                        if(fdtable[z].inodenumber == check.inodenumber){
                            return -1;
                        }
                    }
                }
                // add into file descriptor table
                int inodenum = check.inodenumber;
                for(int i = 0; i < NUMBER_OF_INODES; i++){
                    if(fdtable[i].free){
                        fdtable[i].free = false;
                        fdtable[i].inodenumber = inodenum;
                        fdtable[i].rwpointer = inodetable[inodenum].size;
                        int fileID = i;
                        return fileID;
                    }
                }
                return -1;
            }
        }
    }
    for(int i = 0; i < NUMBER_OF_INODES; i++){
        directory check = directorytable[i];
        if(check.free){
            for(int z = 0; z < NUMBER_OF_INODES; z++) {
                if (inodetable[z].free) {
                    inodetable[z].free = false;
                    inodetable[z].size = 0;
                    inodetable[z].link_cnt = 0;
                    inodetable[z].mode = 1;
                    check.free = false;
                    strcpy(check.filename, name);
                    check.inodenumber = z;
                    directorytable[i] = check;
                    flush();
                    int inodenum = check.inodenumber;
                    for(int i = 0; i < NUMBER_OF_INODES; i++){
                        if(fdtable[i].free){
                        fdtable[i].free = false;
                        fdtable[i].inodenumber = inodenum;
                        fdtable[i].rwpointer = inodetable[inodenum].size;
                        int fileID = i;
                        return fileID;
                        }
                    }
                return -1;
                }
            }
        }
    }
    return -1;
}

int sfs_fclose(int fileID){
    //grab the file in fdtable
    fd file = fdtable[fileID];
    //free the file in fdtable
    if(!file.free){
        file.free = true;
        fdtable[fileID] = file;
        return 0;
    }
    return -1;
}
//helper for write
int distribute(int fileID, int lengthinblocks) {
    int distributed = 0;
    //grab the inode
    inode inodes = inodetable[fdtable[fileID].inodenumber];
    for (int i = inodes.link_cnt; i < inodes.link_cnt + lengthinblocks; i++) {
        for (int block = 0; block < NUMBER_OF_BLOCKS; block++) {
            if (bitmap[block] == 1) {
                bitmap[block] = 0;
                //check whether we're distibuting direct or indirect pointers
                if (i < 12) {
                    inodes.pointer[i] = block;

                    distributed++;
                } else {
                    //check if we need to ultilize indirect block
                    bool indirect = false;
                    if(inodes.link_cnt + distributed == 12){
                        for (int indBlock = 0; indBlock < NUMBER_OF_BLOCKS; indBlock++) {
                            if (bitmap[indBlock] == 1) {
                                bitmap[indBlock] = 0;
                                inodes.indirect = indBlock;
                                int indirectPointers[256];
                                write_blocks(inodes.indirect, 1, indirectPointers);

                                indirect = true;
                                break;
                            }
                        }
                    }
                    //need to use indirect pointer
                    else{
                        indirect = true;
                    } 
                    //no direct pointer we break
                    if (indirect == false){
                        break;
                    }
                    //we then add block to indirect pointers array
                    int indirectPointers[256];
                    read_blocks(inodes.indirect, 1, indirectPointers);

                    indirectPointers[i-12] = block;
                    distributed++;

                    write_blocks(inodes.indirect, 1, indirectPointers);

                }
                break;
            }
        }
    }

    inodes.link_cnt += distributed;
    inodetable[fdtable[fileID].inodenumber] = inodes;
    //flush everything
    flush();
    return distributed;
}

int sfs_fwrite(int fileID, char *buf, int length){
    //check if file is open
    if(fdtable[fileID].free) return 0;
    //grab the inode
    inode inodes = inodetable[fdtable[fileID].inodenumber];
    //grab the pointer
    int pointer = fdtable[fileID].rwpointer;
    int blocknum = pointer/BLOCK_SIZE;
    int bytestartpos = pointer%BLOCK_SIZE;
    //Exact number of bytes to be written
    int bytenum = length;
    if (length + pointer > MAX_FILE_BLOCKS*BLOCK_SIZE){
        bytenum -= length + pointer - (MAX_FILE_BLOCKS*BLOCK_SIZE);
    }

    //calculating number of extra blocks needed
    int numberofextrablocks = 0;
    int temp = (pointer + bytenum)/BLOCK_SIZE;
    if ((pointer + bytenum)%BLOCK_SIZE != 0){
        temp++;
    } 
    if(temp >= inodes.link_cnt){
        numberofextrablocks = temp - inodes.link_cnt;
    }

    //distribute extra blocks needed
    int actual = distribute(fileID, numberofextrablocks);
    //update inode store
    inodes = inodetable[fdtable[fileID].inodenumber];
    if(numberofextrablocks > actual){
        numberofextrablocks = actual;
        bytenum -= length + pointer - (inodes.link_cnt*BLOCK_SIZE);
    }

    //initialize an all null buffer the size of all blocks occupied after write
    char Buffers[(inodes.link_cnt)*BLOCK_SIZE];
    memset(Buffers, 0, sizeof(Buffers));

    //Read whole file into aBuffer
    for (int i = 0; i < inodes.link_cnt; i++ ){
        if(i < 12) {
            read_blocks(inodes.pointer[i], 1, Buffers + (i * BLOCK_SIZE));
        }
        else{
            //for blocks through indirect pointer
            int indirectpointers[256];
            read_blocks(inodes.indirect, 1, indirectpointers);

            for(int z = i; z < inodes.link_cnt; z++){
                read_blocks(indirectpointers[z-12], 1, Buffers + (z * BLOCK_SIZE));
            }
            break;
        }
    }

    //write into buffer at right location
    memcpy(Buffers+(blocknum*BLOCK_SIZE)+bytestartpos, buf, bytenum);

    //Write back the buffer into the appropriate blocks
    for (int i = 0; i < inodes.link_cnt; i++ ){
        if(i < 12){

            write_blocks(inodes.pointer[i], 1, Buffers + (i*BLOCK_SIZE));
        }else{
            //for blocks through indirect pointer
            int indirectpointers[256];
            read_blocks(inodes.indirect, 1, indirectpointers);

            for(int z = i; z < inodes.link_cnt; z++){
                write_blocks(indirectpointers[z-12], 1, Buffers + (z * BLOCK_SIZE));
            }
            break;
        }
    }

    //update file size if necessary and flush inode cache
    if(pointer + bytenum > inodes.size){
        inodes.size = pointer + bytenum;
    }
    inodetable[fdtable[fileID].inodenumber] = inodes;
    //update write pointer
    fdtable[fileID].rwpointer += bytenum;

    //flush
    flush();

    return bytenum;
}
int sfs_fread(int fileID, char *buf, int length) {

    //check file is free or not in fdtable
    if(fdtable[fileID].free){
        return 0;
    }
    //get the inodes from the inodetable
    inode inodes = inodetable[fdtable[fileID].inodenumber];
    //get read write pointer and update 
    int pointer = fdtable[fileID].rwpointer; 
    int BlockNum = pointer / BLOCK_SIZE;
    int ByteStart = pointer % BLOCK_SIZE;
    //Exact number of bytes we can read
    int ByteNum = length;
    if (pointer + length > inodes.size) {
        ByteNum = inodes.size - pointer;
    }

    //initialize an a buffer and set it to zeros. 
    char Buffers[inodes.link_cnt * BLOCK_SIZE];
    memset(Buffers, 0, sizeof(Buffers));

    //Read whole file into Buffers
    for (int i = 0; i < inodes.link_cnt; i++ ){
        if(i < 12) {
            read_blocks(inodes.pointer[i], 1, Buffers + (i * BLOCK_SIZE));
        }else{
            // read blocks of indirect pointers
            int indirectpointers[256];
            read_blocks(inodes.indirect, 1, indirectpointers);

            for(int z = i; z < inodes.link_cnt; z++){
                read_blocks(indirectpointers[z-12], 1, Buffers + (z * BLOCK_SIZE));
            }
            break;
        }
    }
    memcpy(buf, Buffers + (BlockNum*BLOCK_SIZE) + ByteStart, ByteNum);
    fdtable[fileID].rwpointer += ByteNum;
    flush();
    return ByteNum;
}

int sfs_fseek(int fileID, int loc){
    //check whether it can be seeked.
    if(loc >= 0 && loc <= inodetable[fdtable[fileID].inodenumber].size){
        //move the read write pointer to location.
        fdtable[fileID].rwpointer = loc;
        return 0;
    }
    else{
        return -1;
    }
}
int sfs_remove(char *file){
    //create a cache like inode
    inode inodes;
    inodes.free = true;
    for(int i = 0; i < NUMBER_OF_INODES; i++){
        //grab the temporary directory and check
        directory temp = directorytable[i];
        //check if it contain stuff
        if(!temp.free){
            if(strcmp(temp.filename, file) == 0){
                //check if file is open
                for(int z = 0; z < NUMBER_OF_INODES; z++){
                    if(!fdtable[z].free){
                        //if not, exit
                        if(fdtable[z].inodenumber == temp.inodenumber){
                            return -1;
                        } 
                    }
                }
                inodes = inodetable[directorytable[i].inodenumber];
                //free inode
                inodetable[temp.inodenumber].free = true;
                //free directory entry
                directorytable[i].free = true;
                flush();
                break;
            }
        }
    }
    if(inodes.free){
        return -1;
    }
    for(int i = 0; i < inodes.link_cnt ; i++){
        if(i<12){
            bitmap[inodes.pointer[i]] = 1;
        }else{
            //free indirect pointer block
            bitmap[inodes.indirect] = 1;

            int indirectPointers[256];
            read_blocks(inodes.indirect, 1, indirectPointers);

            for(int z = i; z < inodes.link_cnt; z++){
                bitmap[indirectPointers[z-12]] = 1;
            }
            break;
        }
    }
    flush();
    return 0;
}

