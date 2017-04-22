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

struct file {
    char* data;
};

struct directory_entry {
    char name[48];
    int inode_pnum;
};

/*struct directory {
    directory_entry* entries; // can't possibly have more 4k entries than this
};*/

struct inode {
    int refs; // reference count
    int message;
    mode_t mode; // permission & type
    int size; // bytes for file
    int dir_pnum;
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
int create_directory(const char* path, mode_t mode);
inode* get_inode_from_path(const char* path);
char* level_up(const char* path);
char* get_leaf(const char* path);
directory_entry* get_directory(int pnum);
inode* get_inode(int pnum);
int initialize_directory(int pnum);
