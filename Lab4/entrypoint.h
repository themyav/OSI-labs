#ifndef ENTRYPOINT
#define ENTRYPOINT

#include <linux/fs.h>


void networkfs_register(void);
void networkfs_unregister(void);
struct dentry *networkfs_mount(struct file_system_type *, int, const char *,
                               void *);
int networkfs_fill_super(struct super_block *, void *, int);
struct inode *networkfs_get_inode(struct super_block *, const struct inode *,
                                  umode_t, int);
void networkfs_kill_sb(struct super_block *);
struct dentry *networkfs_lookup(struct inode *, struct dentry *, unsigned int);
int networkfs_iterate(struct file *, struct dir_context *);

#endif
