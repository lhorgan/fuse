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

static int   pages_fd   = -1;
static void* pages_base =  0;
inode** inode_table = 0;
int* page_table = 0;

void storage_init(const char* path)
{
    //printf("TODO: Store file system data in: %s\n", path);
    pages_init(path); // create file for file system data
}

int get_stat(const char* path, struct stat* st)
{
    inode* node = get_inode_from_path(path);
    if(!node) {
        printf("can't get file data from this path");
        return -1;
    }

    memset(st, 0, sizeof(struct stat));
    st->st_uid = getuid();
    st->st_mode = node->mode;
    st->st_size = node->size;
    return 0;
}

const char* get_data(const char* path)
{
    /*file_data* dat = get_file_data(path);
    if (!dat) {
        return 0;
    }

    return dat->data;*/
    return "hello";
}

void pages_init(const char* path)
{
    printf("INITIALIZING!\n\n");
    printf("%s\n", path);
    pages_fd = open(path, O_CREAT | O_RDWR, 0644);
    assert(pages_fd != -1);

    int rv = ftruncate(pages_fd, NUFS_SIZE);
    assert(rv == 0);

    pages_base = mmap(0, NUFS_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, pages_fd, 0);
    assert(pages_base != MAP_FAILED);

    // this should really only happen the first time
    page_table = (int*)pages_get_page(0);
    page_table[0] = 0; // first page stores the page table itself (so meta)
    page_table[1] = 0; // second page stores the inode table
    for(int i = 2; i < 256; i++) { // remaining pages are free
        page_table[i] = 1;
    }
    inode_table = (inode**)pages_get_page(1);
    for(int i = 0; i < 256; i++) {
        inode_table[i] = 0;
    }
    printf("success!");
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

int pages_find_empty()
{
    int pnum = -1;
    for(int ii = 2; ii < PAGE_COUNT; ++ii) {
        if(page_table[ii]) { // 1 free, 0 not free
            pnum = ii;
            break;
        }
    }
    return pnum;
}

inode* get_free_inode() {
    for(int i = 0; i < 256; i++) {
        if(inode_table[i] == 0) {
            return inode_table[i];
        }
    }
    return 0;
}

int create_directory(const char* path, mode_t mode) {
    char* parent_path = level_up(path);
    char* leaf = get_leaf(path);

    inode* parent_dir_inode = get_inode_from_path(parent_path); // inode pointing to the parent directory
    if(parent_dir_inode->dir) { // parent directory is in fact a directory
        directory* parent_dir = parent_dir_inode->dir;
        for(int i = 0; i < PAGE_COUNT; i++) {
            if(!parent_dir->entries[i].node) { // empty slot!
                strcpy(parent_dir->entries[i].name, leaf);
                inode* leaf_dir_inode = get_free_inode();
                if(leaf_dir_inode) {
                    int page_idx = pages_find_empty();
                    if(page_idx >= 0) { // we found a page for  our directory
                        printf("directory created!");
                        directory* leaf_dir = (directory*)pages_get_page(page_idx);
                        page_table[page_idx] = 0; // this page is no longer free
                        leaf_dir_inode->dir = leaf_dir;
                        leaf_dir_inode->size = 0;
                        leaf_dir_inode->mode = mode;
                        return 0; // success!
                    }
                    else {
                        printf("no free pages");
                        return -1;
                    }
                }
                else {
                    printf("emtpy inode not found");
                    return -1; //
                }
                break;
            }
        }
    }
    return -1; // error
}

char* level_up(const char* path) {
    slist* split = s_split(path, '/');
    int total_len = 0;
    while(split->next) {
        total_len += strlen(split->data);
        split = split->next;
    }
    char* parent_path = (char*)malloc(total_len + 1);
    int idx = 0;
    while(split->next) {
        for(int i = 0; i < strlen(split->next->data); i++) {
            parent_path[idx] = split->next->data[i];
            idx++;
        }
    }
    parent_path[total_len] = 0;
    return parent_path;
}

char* get_leaf(const char* path) {
    slist* split = s_split(path, '/');
    while(split->next) {
        split = split->next;
    }
    return split->data;
}

inode* get_inode_from_path(const char* path) {
    slist* split = s_split(path, '/'); // split path on /
    inode* curr_node = inode_table[0]; // start at the root directory
    while(split) {
        if(curr_node->dir) { // this node points to a directory
            int entry_found = 0;
            for(int i = 0; i < PAGE_COUNT; i++) { // loop through all the entries in the directory pointed to by this inode
                directory_entry entry = curr_node->dir->entries[i]; // little unclear dir->entries shouldn't be dir.entries, but whatever
                if(entry.node && strcmp(entry.name, split->data) == 0) { // current entry points to the inode for our path
                    curr_node = entry.node;
                    entry_found = 1;
                    break;
                }
            }
            if(!entry_found) {
                printf("entry not found");
                return 0;
            }
        }
        else { // can't search inside something that's not a directory
            printf("you're trying to search inside something that's not a directory");
            return 0;
        }
        split = split->next;
    }
    return curr_node;
}
