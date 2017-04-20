#define NUFS_SIZE 1048576 // 1MB
#define PAGE_COUNT 256

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

struct file;
typedef struct file file;
struct directory_entry;
typedef struct directory_entry directory_entry;
struct directory;
typedef struct directory directory;
struct inode;
typedef struct inode inode;
struct inode_table;
typedef struct inode_table inode_table;

struct file {
    char* data;
};

struct directory_entry {
    char name[48];
    inode* node;
};

struct directory {
    directory_entry entries[PAGE_COUNT]; // can't possibly have more 4k entries than this
};

struct inode {
    int refs; // reference count
    int mode; // permission & type
    int size; // bytes for file
    file* d_pages[12];
    inode* i_page;
    directory* dir;
};

// struct inode_table {
//     inode nodes[255];
// };

void storage_init(const char* path);
int get_stat(const char* path, struct stat* st);
const char* get_data(const char* path);
void pages_init(const char* path);
void pages_free();
void* pages_get_page(int pnum);
int pages_find_empty();
inode* get_inode_from_path(const char* path);
