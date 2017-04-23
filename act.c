#include "act.h"
#include "slist.h"

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fuse.h>
#include <stdlib.h>

int read_dir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset);

#define DIRSIZE 20
#define FPTLEN 40

static int   pages_fd   = -1;

static void* pages_base =  0;
inode* inode_table = 0;

int* inode_bitmap = 0;
int* page_bitmap = 0;

void storage_init(const char* path)
{
    ////printf("TODO: Store file system data in: %s\n", path);
    pages_init(path); // create file for file system data
}

int get_stat(const char* path, struct stat* st)
{
    //printf("getting stat for %s\n", path);
    inode* node = get_inode_from_path(path);
    if(!node) {
        //printf("can't get stat metadata from this path\n");
        return -1;
    }

    //printf("got file data");
    memset(st, 0, sizeof(struct stat));
    st->st_uid = getuid();
    st->st_mode = node->mode;
    st->st_size = node->size;
    return 0;
}

// const char* get_data(const char* path, size_t size, off_t offset)
// {
//     /*file_data* dat = get_file_data(path);
//     if (!dat) {
//         return 0;
//     }
//
//     return dat->data;*/
//     //printf("HIYA getting file data\n");
//     inode* file_inode = get_inode_from_path(path);
//     if(file_inode && file_inode->data_pnum >= 0) {
//         int data_remaining = size;
//         while(data_remaining > 0) {
//
//         }
//     }
//
//     return "hello";
// }

int get_data(const char* path, size_t size, off_t offset, char* res) {
    inode* file_inode = get_inode_from_path(path);
    printf("reading data from file!\n");
    //res = (char*)malloc(size);
    if(file_inode && file_inode->data_pnum >= 0) {
        int* page_nums = (int*)pages_get_page(file_inode->data_pnum);
        int page_nums_index = offset / 4096;
        int data_remaining = size;
        int off_in_page = offset - (4096 * page_nums_index);

        while(data_remaining) {
            printf("data remaining: %i\n", data_remaining);
            int page_num = page_nums[page_nums_index];
            if(page_num < 0) { // this page isn't pointing to anything yet
                printf("page does not exist :(\n");
                return -1;
            }
            char* data = (char*)(pages_get_page(page_num) + off_in_page);
            strncpy(res, data, min(data_remaining, 4096 - off_in_page));
            if(off_in_page + data_remaining > 4096) {
                ////printf("writing starts at %i and I have %i characters to write\n", off_in_page, data_remaining);
                data_remaining -= 4096 - off_in_page;
                res += 4096 - off_in_page;
                off_in_page = 0;
            }
            else {
                data_remaining -= data_remaining;
            }
            page_num++;
        }
        printf("READ SUCCESSFUL!\n");
        return size; // success
    }
    printf("someting went wrong\n");
    return -1; // error
}

void pages_init(const char* path)
{
    //printf("INITIALIZING!\n\n");
    //printf("%s\n", path);
    int exists = 0;
    if(access(path, F_OK) != -1) {
        exists = 1;
    }

    pages_fd = open(path, O_CREAT | O_RDWR, 0644);
    assert(pages_fd != -1);

    int rv = ftruncate(pages_fd, NUFS_SIZE);
    assert(rv == 0);

    pages_base = mmap(0, NUFS_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, pages_fd, 0);
    assert(pages_base != MAP_FAILED);

    // this should really only happen the first time
    // page bitmap
    if(!exists) {
        printf("creating!\n");
        page_bitmap = (int*)pages_get_page(0);
        page_bitmap[0] = 0; // first page stores the page bitmap
        page_bitmap[1] = 0; // second page stores the inode bitmap
        page_bitmap[2] = 0; // inodes themselves
        page_bitmap[3] = 0; // root directory structure
        for(int i = 4; i < 256; i++) { // remaining pages are free
            page_bitmap[i] = 1;
        }

        // inode bitmap
        inode_bitmap = (int*)pages_get_page(1);
        for(int i = 0; i < 256; i++) {
            inode_bitmap[i] = 1;
        }

        inode_table = (inode*)pages_get_page(2);
        inode_table[0].size = 0;
        inode_table[0].message = 513;
        inode_table[0].mode = 040755;

        inode_table[0].dir_pnum = 3;
        initialize_directory(3);
        inode_bitmap[0] = 0; // the 0th inode is taken
    }
    else {
        printf("No need to create!\n");
        page_bitmap = (int*)pages_get_page(0);
        inode_bitmap = (int*)pages_get_page(1);
        inode_table = (inode*)pages_get_page(2);
    }
}

void pages_free()
{
    int rv = munmap(pages_base, NUFS_SIZE);
    assert(rv == 0);
}

void* pages_get_page(int pnum)
{
    return pages_base + 4096 * pnum;
}

directory_entry* get_directory(int pnum) {
    return (directory_entry*)pages_get_page(pnum);
}

inode* get_inode(int idx) {
    return &inode_table[idx];
}

int pages_find_empty()
{
    int pnum = -1;
    for(int ii = 2; ii < PAGE_COUNT; ++ii) {
        if(page_bitmap[ii]) { // 1 free, 0 not free
            //printf("FREE PAGE: %i\n", ii);
            pnum = ii;
            break;
        }
    }
    return pnum;
}

int inode_get_empty() {
    for(int i = 0; i < 256; i++) {
        if(inode_bitmap[i] == 1) { // free
            return i;
        }
    }
    return -1;
}

int create_directory(const char* path, mode_t mode) {
    char* parent_path = level_up(path);
    //printf("parent path: %s\n", parent_path);
    inode* parent_inode = get_inode_from_path(parent_path);
    //printf("%i\n", parent_inode);
    if(parent_inode && parent_inode->dir_pnum >= 0) {
        directory_entry* parent_dir = get_directory(parent_inode->dir_pnum);
        //printf("parent_inode->dir_pnum %i\n", parent_inode->dir_pnum);
        //printf("get_directory(parent_inode->dir_pnum): %i\n", parent_dir);
        for(int i = 0; i < DIRSIZE; i++) {
            if(parent_dir[i].inode_idx < 0) { // free entry
                strcpy(parent_dir[i].name, get_leaf(path));

                int inode_idx = inode_get_empty();
                inode_bitmap[inode_idx] = 0;
                parent_dir[i].inode_idx = inode_idx; // TODO, CHECK IF INODE IS ACTUALLY FREE

                int new_dir_pnum = pages_find_empty();
                initialize_directory(new_dir_pnum);
                inode* new_dir_inode = get_inode(inode_idx);
                new_dir_inode->dir_pnum = new_dir_pnum;
                printf("dir pnum exists at %i\n", pages_get_page(new_dir_pnum));
                new_dir_inode->data_pnum = -1;
                new_dir_inode->size = 0;
                new_dir_inode->mode = 040755;

                return 0;
            }
        }
    }
    return -1; // error
}

// just create an empty directory on a page
int initialize_directory(int pnum) {
    //printf("taking %i for use as a directory\n", pnum);
    page_bitmap[pnum] = 0;
    directory_entry* entries = (directory_entry*)pages_get_page(pnum);
    for(int i = 0; i < DIRSIZE; i++) {
        strcpy(entries[i].name, "\0");
        entries[i].inode_idx = -1;
    }
}

int create_file(const char* path, mode_t mode) {
    char* parent_path = level_up(path);
    //printf("parent path: %s\n", parent_path);
    inode* parent_inode = get_inode_from_path(parent_path);
    //printf("%i\n", parent_inode);
    if(parent_inode && parent_inode->dir_pnum >= 0) {
        directory_entry* parent_dir = get_directory(parent_inode->dir_pnum);
        //printf("file parent_inode->dir_pnum %i\n", parent_inode->dir_pnum);
        //printf("file get_directory(parent_inode->dir_pnum): %i\n", parent_dir);
        for(int i = 0; i < DIRSIZE; i++) {
            if(parent_dir[i].inode_idx < 0) { // free entry
                //printf("by the way, i is %i\n", i);
                //printf("THE PRESENT NAME OF THE ENTRY IS %s\n", parent_dir[i].name);
                //printf("THE PRESENT NAME OF THE ENTRY IS %s\n", parent_dir[i - 1].name);
                strcpy(parent_dir[i].name, get_leaf(path));
                int new_file_inode_idx = inode_get_empty();
                inode_bitmap[new_file_inode_idx] = 0; // taken
                parent_dir[i].inode_idx = new_file_inode_idx;
                inode* new_file_inode = get_inode(new_file_inode_idx);

                int data_pnum = pages_find_empty();
                initialize_file_page_table(data_pnum);
                new_file_inode->data_pnum = data_pnum;
                new_file_inode->dir_pnum = -1; // not a directory
                new_file_inode->size = 0;
                new_file_inode->mode = 0100777;

                return 0;
            }
        }
    }
    return -1; // error
}

void initialize_file_page_table(int pnum) {
    //printf("taking %i for use as a fpt\n", pnum);
    page_bitmap[pnum] = 0;
    int* pnums = (int*)pages_get_page(pnum);
    ////printf("%i\n", pages_base);
    for(int i = 0; i < FPTLEN; i++) {
        //printf("writing -1 to address %i\n", &pnums[i]);
        pnums[i] = -1;
        //printf("%i\n", &pnums[i]);
    }
}

int truncate(const char* path, off_t size) {
    inode* file_inode = get_inode_from_path(path);
    if(file_inode && file_inode->data_pnum >= 0) {
        printf("truncating successful!\n");
        file_inode->size = size;
        return 0;
    }
    printf("truncate failed!\n");
    return -1;
}

int write_file(const char* path, const char* buf, size_t size, off_t offset) {
    inode* file_inode = get_inode_from_path(path);
    printf("writing data to file!\n");
    if(file_inode && file_inode->data_pnum >= 0) {
        file_inode->size += size;
        int* page_nums = (int*)pages_get_page(file_inode->data_pnum);
        int page_nums_index = offset / 4096;
        int data_remaining = size;
        int off_in_page = offset - (4096 * page_nums_index);

        data_remaining = size;
        while(data_remaining) {
            printf("data remaining: %i\n", data_remaining);
            int page_num = page_nums[page_nums_index];
            if(page_num < 0) { // this page isn't pointing to anything yet
                page_num = pages_find_empty(); // TODO: HANDLE PAGE NOT BEING FREE
                page_bitmap[page_num] = 0; // this page is taken now
                page_nums[page_nums_index] = page_num;
            }
            char* data = (char*)(pages_get_page(page_num) + off_in_page);
            strncpy(data, buf, min(data_remaining, 4096 - off_in_page));
            if(off_in_page + data_remaining > 4096) {
                ////printf("writing starts at %i and I have %i characters to write\n", off_in_page, data_remaining);
                data_remaining -= 4096 - off_in_page;
                buf += 4096 - off_in_page;
                off_in_page = 0;
            }
            else {
                data_remaining -= data_remaining;
            }
            page_num++;
        }
        printf("WRITE SUCCESSFUL!\n");
        return size; // success
    }
    printf("someting went wrong\n");
    return -1; // error
}

int min(int num1, int num2) {
    if(num2 < num1) {
        return num2;
    }
    return num1;
}

// actually initializing a file BLOCK, not the file itself
// int initialize_file(int pnum) {
//     page_bitmap[pnum] = 0;
//     directory_entry* entries = (directory_entry*)pages_get_page(pnum);
//     for(int i = 0; i < DIRSIZE; i++) {
//         strcpy(entries[i].name, "\0");
//         entries[i].inode_idx = -1;
//     }
// }

int read_dir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset) {
    inode* dir_inode = get_inode_from_path(path);
    if(dir_inode) {
        directory_entry* dir = get_directory(dir_inode->dir_pnum);
        for(int i = offset; i < DIRSIZE; i++) {
            if(dir[i].inode_idx != -1) {
                filler(buf, dir[i].name, 0, i + 1);
            }
        }
        return 0;
    }
    return -1;
}

char* level_up(const char* path) {
    int last_slash_index = 0;
    for(int i = 0; i < strlen(path); i++) {
        if(path[i] == '/') {
            last_slash_index = i;
        }
    }
    if(last_slash_index == 0) {
        char* parent_path = (char*)malloc(2);
        parent_path[0] = '/';
        parent_path[1] = 0;
        return parent_path;
    }
    else {
        char* parent_path = (char*)malloc(last_slash_index + 1);
        for(int i = 0; i < last_slash_index; i++) {
            parent_path[i] = path[i];
        }
        parent_path[last_slash_index] = 0;
        //printf("parent of %s is %s\n", path, parent_path);
        return parent_path;
    }
}

char* get_leaf(const char* path) {
    slist* split = s_split(path, '/');
    while(split->next) {
        split = split->next;
    }
    return split->data;
}

inode* get_inode_from_path(const char* path) {
    printf("Getting inode from path: %s\n", path);
    slist* split = s_split(path, '/')->next; // split path on /
    inode* curr_node = &inode_table[0]; // start at the root directory
    while(split) {
        if(curr_node->dir_pnum >= 0) { // this node points to a directory
            directory_entry* dir = get_directory(curr_node->dir_pnum);
            int entry_found = 0;
            for(int i = 0; i < DIRSIZE; i++) { // loop through all the entries in the directory pointed to by this inode
                if(dir[i].inode_idx != -1)
                    printf("ENTRY: %s, %i\n", dir[i].name, dir[i].inode_idx);
                if(strcmp(dir[i].name, split->data) == 0) { // current entry points to the inode for our path
                    //printf("entry found: %s\n", dir[i].name);
                    entry_found = 1;
                    curr_node = get_inode(dir[i].inode_idx);
                    //printf("the new current node has directory type: %i\n", curr_node->dir_pnum);
                    break;
                }
            }
            if(!entry_found) {
                printf("entry not found: %s\n", split->data);
                return 0;
            }
        }
        else { // can't search inside something that's not a directory
            //printf("boom %i bam %i\n", curr_node->dir_pnum, curr_node->data_pnum);
            printf("you're trying to search inside something that's not a directory\n");
            return 0;
        }
        split = split->next;
    }
    return curr_node;
}
