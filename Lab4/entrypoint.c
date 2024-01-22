#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "tcp_client.h"
#include "entrypoint.h"

#define ROOT_INODE 2756147


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kristina Kravtsova");
MODULE_VERSION("0.01");

struct entries entries;

struct file_system_type networkfs_fs_type =
        {
                .name = "networkfs",
                .mount = networkfs_mount,
                .kill_sb = networkfs_kill_sb
        };

struct file_operations networkfs_dir_ops =
        {
                .iterate = networkfs_iterate,
        };

struct inode_operations networkfs_inode_ops =
        {
                .lookup = networkfs_lookup,
                .create = networkfs_create,
                .unlink = networkfs_unlink,
                .mkdir = networkfs_mkdir,
                .rmdir = networkfs_rmdir
        };

int networkfs_rmdir(struct inode *parent_inode, struct dentry *child_dentry) {
    int res = rmdir_call(parent_inode->i_ino, child_dentry->d_name.name);

    return res;
}

int networkfs_mkdir(struct user_namespace *ns, struct inode *parent_inode,
                    struct dentry *child_dentry, umode_t t) {
    ino_t new_inode;
    struct inode * inode;
    int res = create_call(parent_inode->i_ino, child_dentry->d_name.name, 4, &new_inode);

    if (res) {
        return res;
    }

    inode = networkfs_get_inode(parent_inode->i_sb, NULL, S_IFDIR, new_inode);

    d_add(child_dentry, inode);
    return 0;
}


int networkfs_unlink(struct inode *parent_inode, struct dentry *child_dentry) {
    const char *name;
    ino_t root;
    name = child_dentry->d_name.name;
    root = parent_inode->i_ino;

    int64_t res = unlink_call(root, name);

    return res;
}


int networkfs_create(struct user_namespace *ns, struct inode *parent_inode,
                     struct dentry *child_dentry, umode_t mode, bool b) {
    ino_t new_inode;
    ino_t root;
    struct inode *inode;
    const char *name;

    name = child_dentry->d_name.name;
    root = parent_inode->i_ino;

    int64_t res = create_call(root, name, 8, &new_inode); //create FILE

    if (res) {
        return res;
    }

    inode = networkfs_get_inode(parent_inode->i_sb, NULL, S_IFREG, new_inode);
    d_add(child_dentry, inode);
    return 0;
}


struct inode *networkfs_get_inode(struct super_block *sb,
                                  const struct inode *dir, umode_t mode,
                                  int i_ino) {
    struct inode *inode;

    inode = new_inode(sb);
    if (inode != NULL) {
        inode->i_ino = i_ino;
        inode->i_op = &networkfs_inode_ops;
        if (mode & S_IFDIR) {
            inode->i_fop = &networkfs_dir_ops;
        }
        inode_init_owner(&init_user_ns, inode, dir, mode | S_IRWXUGO);
    }
    return inode;
}


int networkfs_iterate(struct file *filp, struct dir_context *ctx) {
    char fsname[256];
    struct dentry *dentry;
    struct inode *inode;
    unsigned long offset;
    unsigned char ftype;
    int stored;
    ino_t ino;
    ino_t dino;

    dentry = filp->f_path.dentry;
    inode = dentry->d_inode;
    offset = filp->f_pos;
    stored = 0;
    ino = inode->i_ino;

    printk("request for %ld\n", ino);
    int64_t res = list_call(ino, &entries);
    if(res != 0) return -1;
    int i;
    i = 0;
    /*int cnt = entries.entries_count;
    entries.entries_count += 1;
    entries.entries[cnt].entry_type=4;
    entries.entries[cnt].ino=ino;
    sprintf(entries.entries[cnt].name,".");*/

    while (i < entries.entries_count) {
        struct entry en = entries.entries[i];
        printk("type:%d,ino:%ld,name:%s\n", en.entry_type, en.ino, en.name);
        i++;
    }
    if (res) {
        return -1;
    }

    while (true) {
        if (offset == 0) {
            strcpy(fsname, ".");
            ftype = DT_DIR;
            dino = ino;
        } else if (offset == 1) {
            strcpy(fsname, "..");
            ftype = DT_DIR;
            dino = dentry->d_parent->d_inode->i_ino;
        } else if (offset - 2 < entries.entries_count) {
            strcpy(fsname, entries.entries[offset - 2].name);
            ftype = entries.entries[offset - 2].entry_type;
            dino = entries.entries[offset - 2].ino;
        } else {
            return stored;
        }
        dir_emit(ctx, fsname, strlen(fsname), dino, ftype);
        stored++;
        offset++;
        ctx->pos = offset;
    }
    return stored;
}


struct dentry *networkfs_lookup(struct inode *parent_inode,
                                struct dentry *child_dentry,
                                unsigned int flag) {
    ino_t root;
    struct inode *inode;
    struct entry_info e_info;

    const char *name = child_dentry->d_name.name;

    root = parent_inode->i_ino;

    int64_t res = lookup_call(root, name, &e_info);

    if (res) {
        return NULL;
    }

    if (e_info.entry_type == DT_DIR) {
        inode = networkfs_get_inode(parent_inode->i_sb, NULL, S_IFDIR, e_info.ino);
    } else {
        inode = networkfs_get_inode(parent_inode->i_sb, NULL, S_IFREG, e_info.ino);
    }

    d_add(child_dentry, inode);
    return child_dentry;
}


//task 3
void networkfs_kill_sb(struct super_block *sb) {
    kfree(sb->s_root->d_fsdata);
    printk(KERN_INFO
    "networkfs super block is destroyed. Unmount successfully.\n");
}


int networkfs_fill_super(struct super_block *sb, void *data, int silent) {
    struct inode *inode;
    inode = networkfs_get_inode(sb, NULL, S_IFDIR, ROOT_INODE);
    sb->s_root = d_make_root(inode);
    if (sb->s_root == NULL) {
        return -ENOMEM;
    }
    printk(KERN_INFO
    "return 0\n");
    return 0;
}

struct dentry *mount_nodev(struct file_system_type *fs_type, int flags, void *data,
                           int (*fill_super)(struct super_block *, void *, int));

struct dentry *networkfs_mount(struct file_system_type *fs_type, int flags, const char *tkn, void *data) {
    struct dentry *ret;
    ret = mount_nodev(fs_type, flags, data, networkfs_fill_super);
    if (ret == NULL) {
        printk(KERN_ERR
        "Can't mount file system");
    } else {
        printk(KERN_INFO
        "Mounted successfuly");
    }


    return ret;
}


int networkfs_init(void) {
    register_filesystem(&networkfs_fs_type);
    printk(KERN_INFO
    "Hello, World!\n");
    return 0;
}

void networkfs_exit(void) {
    unregister_filesystem(&networkfs_fs_type);
    printk(KERN_INFO
    "Goodbye!\n");
}

module_init(networkfs_init);
module_exit(networkfs_exit);