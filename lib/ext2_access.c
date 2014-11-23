// ext2 definitions from the real driver in the Linux kernel.
#include "ext2fs.h"

// This header allows your project to link against the reference library. If you
// complete the entire project, you should be able to remove this directive and
// still compile your code.
#include "reference_implementation.h"

// Definitions for ext2cat to compile against.
#include "ext2_access.h"


///////////////////////////////////////////////////////////
//  Accessors for the basic components of ext2.
///////////////////////////////////////////////////////////

// Return a pointer to the primary superblock of a filesystem.
struct ext2_super_block * get_super_block(void * fs) {
    //super block is offset 1024 bytes
    return fs+1024;
}


// Return the block size for a filesystem.
__u32 get_block_size(void * fs) {
    struct ext2_super_block *sb = get_super_block(fs);
    return 1024 << sb->s_log_block_size;
}


// Return a pointer to a block given its number.
// get_block(fs, 0) == fs;
void * get_block(void * fs, __u32 block_num) {
    __u32 block_size = get_block_size(fs);
    void* ours = (void*)((int)(block_size * (block_num-1)) + (int)get_super_block(fs));
    return ours;
}


// Return a pointer to the first block group descriptor in a filesystem. Real
// ext2 filesystems will have several of these, but, for simplicity, we will
// assume there is only one.
struct ext2_group_desc * get_block_group(void * fs, __u32 block_group_num) {
    struct ext2_super_block * sb= get_super_block(fs);
    int block_size = get_block_size(fs);
    struct ext2_group_desc * ours = (void*)((int)sb + block_size);
    return ours;
}


// Return a pointer to an inode given its number. In a real filesystem, this
// would require finding the correct block group, but you may assume it's in the
// first one.
struct ext2_inode * get_inode(void * fs, __u32 inode_num) {
    struct ext2_super_block* sb = get_super_block(fs);
    struct ext2_group_desc* gd = get_block_group(fs, 1);
    
    void* itable = get_block(fs, gd->bg_inode_table);
    __u32 node_index = (inode_num-1)%sb->s_inodes_per_group;
    void* ours = (void*)(node_index * sb->s_inode_size + (int)itable);
    return ours;
}



///////////////////////////////////////////////////////////
//  High-level code for accessing filesystem components by path.
///////////////////////////////////////////////////////////

// Chunk a filename into pieces.
// split_path("/a/b/c") will return {"a", "b", "c"}.
//
// This one's a freebie.
char ** split_path(char * path) {
    int num_slashes = 0;
    for (char * slash = path; slash != NULL; slash = strchr(slash + 1, '/')) {
        num_slashes++;
    }

    // Copy out each piece by advancing two pointers (piece_start and slash).
    char ** parts = (char **) calloc(num_slashes+1, sizeof(char *));
    char * piece_start = path + 1;
    int i = 0;
    for (char * slash = strchr(path + 1, '/');
         slash != NULL;
         slash = strchr(slash + 1, '/')) {
        int part_len = slash - piece_start;
        parts[i] = (char *) calloc(part_len + 1, sizeof(char));
        strncpy(parts[i], piece_start, part_len);
        piece_start = slash + 1;
        i++;
    }
    // Get the last piece.
    parts[i] = (char *) calloc(strlen(piece_start) + 1, sizeof(char));
    strncpy(parts[i], piece_start, strlen(piece_start));
    char* termination = "END_OF_PATH";
    parts[i+1] = (char *) calloc(strlen(termination)+1, sizeof(char));
    strncpy(parts[i+1], termination, strlen(termination));
    return parts;
}


// Convenience function to get the inode of the root directory.
struct ext2_inode * get_root_dir(void * fs) {
    return get_inode(fs, EXT2_ROOT_INO);
}


// Given the inode for a directory and a filename, return the inode number of
// that file inside that directory, or 0 if it doesn't exist there.
// 
// name should be a single component: "foo.txt", not "/files/foo.txt".
__u32 get_inode_from_dir(void * fs, struct ext2_inode * dir, 
        char * name) {
    int i;
    for(i=0; i < 12; i++){
        struct ext2_dir_entry* entry = get_block(fs, (__u32)dir->i_block[i]);
        while (entry->inode != 0){
            if (strncmp(entry->name, name, strlen(name)) == 0){
                return entry->inode;
            }
            entry = (struct ext2_dir_entry*)((int)entry + entry->rec_len);
        }
    }
    return 0;
}


// Find the inode number for a file by its full path.
// This is the functionality that ext2cat ultimately needs.
__u32 get_inode_by_path(void * fs, char * path) {
    struct ext2_inode * curr_dir = get_root_dir(fs);
    char** dirnames = split_path(path);
    char *c = dirnames[0];
    int length = 0;
    __u32 result = 0;
    while (1){
        if (c == NULL || (strcmp(c, "END_OF_PATH") == 0)){break;}
        length++;
        result = get_inode_from_dir(fs, curr_dir, c);
        if (result == 0){ return 0;}
        c = dirnames[length];
        if (c == NULL){break;}
        curr_dir = get_inode(fs, result);
    }
    return result;
}

