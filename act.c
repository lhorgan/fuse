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

static int   pages_fd   = -1;
static void* pages_base =  0;
inode* page_table[255];

void storage_init(const char* path)
{
    //printf("TODO: Store file system data in: %s\n", path);
    pages_init(path); // create file for file system data
}

int get_stat(const char* path, struct stat* st)
{
    /*file_data* dat = get_file_data(path);
    if (!dat) {
        return -1;
    }

    memset(st, 0, sizeof(struct stat));
    st->st_uid  = getuid();
    st->st_mode = dat->mode;
    if (dat->data) {
        st->st_size = strlen(dat->data);
    }
    else {
        st->st_size = 0;
    }*/
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
    pages_fd = open(path, O_CREAT | O_RDWR, 0644);
    assert(pages_fd != -1);

    int rv = ftruncate(pages_fd, NUFS_SIZE);
    assert(rv == 0);

    pages_base = mmap(0, NUFS_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, pages_fd, 0);
    assert(pages_base != MAP_FAILED);
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
    for (int ii = 2; ii < PAGE_COUNT; ++ii) {
        if (0) { // if page is empty
            pnum = ii;
            break;
        }
    }
    return pnum;
}

inode* get_inode_from_path(const char* path) {
    slist* split = s_split(path, '/'); // split path on /
    inode* curr_node = page_table[0]; // start at the root directory
    while(split != 0) {
        if(curr_node->dir != 0) { // this node points to a directory
            for(int i = 0; i < PAGE_COUNT; i++) { // loop through all the entries in the directory pointed to by this inode
                directory_entry entry = curr_node->dir->entries[i]; // little unclear dir->entries shouldn't be dir.entries, but whatever
                if(entry.node != 0 && strcmp(entry.name, split->data) == 0) { // current entry points to the inode for our path
                    curr_node = entry.node;
                }
            }
        }
        split = split->next;
    }
    return curr_node;
}
