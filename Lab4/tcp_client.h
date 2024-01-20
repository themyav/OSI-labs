
#ifndef NETWORKFS_TCP
#define NETWORKFS_TCP

#include <linux/module.h>
#include <linux/kernel.h>
#include <net/sock.h>
#include <linux/inet.h>
#include <linux/socket.h>

#define MAX_ENTRIES 16
#define MAX_NAME_LENGTH 256

struct entry {
    unsigned char entry_type;
    ino_t ino;
    char name[MAX_NAME_LENGTH];
};

struct entries {
    size_t entries_count;
    struct entry entries[MAX_ENTRIES];
};

struct entry_info {
    unsigned char entry_type;
    ino_t ino;
};

int list_call(ino_t inode, struct entries *result);

int lookup_call(ino_t parent_inode, char *name, struct entry_info *result);

int create_call(ino_t parent_inode, char *name, int type, ino_t *result);

int unlink_call(ino_t parent_inode, char *name);

int rmdir_call(ino_t parent_inode, char *name);


#endif