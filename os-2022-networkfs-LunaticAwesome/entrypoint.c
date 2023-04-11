#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "http.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zakharov Kirill");
MODULE_VERSION("0.01");

void get_adv_name_len(char *value, const char *name_entry, u64 len) {
  size_t i;
  size_t cur = 0;

  for (i = 0; i < len; i++) {
    if ((name_entry[i] >= 'a' && name_entry[i] <= 'z') ||
        (name_entry[i] >= 'A' && name_entry[i] <= 'Z') ||
        (name_entry[i] >= '0' && name_entry[i] <= '9')) {
      value[cur++] = name_entry[i];
    } else {
      value[cur++] = '%';
      cur += sprintf(value + cur, "%02x", name_entry[i]);
    }
  }
  value[cur] = '\0';
}

void get_adv_name(char *value, const char *name_entry) {
  get_adv_name_len(value, name_entry, strlen(name_entry));
}

struct entries {
  size_t entries_count;
  struct entry {
    unsigned char entry_type;  // DT_DIR (4) or DT_REG (8)
    ino_t ino;
    char name[256];
  } entries[16];
};

int fs_list(const char *token, ino_t inode, struct entries *entries) {
  char ino_val[20];
  sprintf(ino_val, "%lu", inode);
  return networkfs_http_call(token, "list", (char *)entries,
                             sizeof(struct entries), 1, "inode", ino_val);
}

struct entry_info {
  unsigned char entry_type;  // DT_DIR (4) or DT_REG (8)
  ino_t ino;
};

int fs_lookup(const char *token, ino_t parent, const char *name,
              struct entry_info *entry_info) {
  char parent_val[20];
  char value[769];
  get_adv_name(value, name);

  sprintf(parent_val, "%lu", parent);
  return networkfs_http_call(token, "lookup", (char *)entry_info,
                             sizeof(struct entry_info), 2, "parent", parent_val,
                             "name", value);
}

int fs_create(const char *token, ino_t parent, const char *name,
              const char *type, ino_t *ret) {
  char parent_val[20];
  char value[769];

  get_adv_name(value, name);
  sprintf(parent_val, "%lu", parent);
  return networkfs_http_call(token, "create", (char *)ret, sizeof(ino_t), 3,
                             "parent", parent_val, "type", type, "name", value);
}

int fs_unlink(const char *token, ino_t parent, const char *name) {
  char parent_val[20];
  char value[769];

  sprintf(parent_val, "%lu", parent);
  get_adv_name(value, name);
  return networkfs_http_call(token, "unlink", NULL, 0, 2, "parent", parent_val,
                             "name", value);
}

int fs_rmdir(const char *token, ino_t parent, const char *name) {
  char parent_val[20];
  char value[769];

  sprintf(parent_val, "%lu", parent);
  get_adv_name(value, name);
  return networkfs_http_call(token, "rmdir", NULL, 0, 2, "parent", parent_val,
                             "name", value);
}

struct content {
  u64 content_length;
  char content[512];  // MAX_SIZE
};

int fs_read(const char *token, ino_t inode, struct content *ret) {
  char ino_val[20];

  sprintf(ino_val, "%lu", inode);
  return networkfs_http_call(token, "read", (char *)ret, sizeof(struct content),
                             1, "inode", ino_val);
}

int fs_write(const char *token, ino_t inode, char const *buffer, size_t size) {
  char ino_val[20];
  char value[512 * 3 + 1];

  sprintf(ino_val, "%lu", inode);
  get_adv_name_len(value, buffer, size);
  return networkfs_http_call(token, "write", NULL, 0, 2, "inode", ino_val,
                             "content", value);
}

int fs_link(const char *token, ino_t source, ino_t parent, const char *name) {
  char source_val[20];
  char parent_val[20];

  sprintf(source_val, "%lu", source);
  sprintf(parent_val, "%lu", parent);
  return networkfs_http_call(token, "link", NULL, 0, 3, "source", source_val,
                             "parent", parent_val, "name", name);
}

struct inode *networkfs_get_inode(struct super_block *sb,
                                  const struct inode *dir, umode_t mode,
                                  ino_t i_ino);

extern struct inode_operations networkfs_inode_ops;
extern struct file_operations networkfs_dir_ops;

struct dentry *networkfs_lookup(struct inode *parent_inode,
                                struct dentry *child_dentry,
                                unsigned int flag) {
  ino_t root;
  struct inode *inode;
  struct entry_info child_info;
  const char *name;

  root = parent_inode->i_ino;
  name = child_dentry->d_name.name;
  int64_t err =
      fs_lookup(parent_inode->i_sb->s_fs_info, root, name, &child_info);
  if (err != 0) {
    return NULL;
  }

  if (child_info.entry_type == DT_DIR) {
    inode =
        networkfs_get_inode(parent_inode->i_sb, NULL, S_IFDIR, child_info.ino);
  } else if (child_info.entry_type == DT_REG) {
    inode =
        networkfs_get_inode(parent_inode->i_sb, NULL, S_IFREG, child_info.ino);
  }
  d_add(child_dentry, inode);
  return child_dentry;
}

int networkfs_create(struct user_namespace *, struct inode *parent_inode,
                     struct dentry *child_dentry, umode_t mode, bool b) {
  struct inode *inode;
  const char *name;
  ino_t root;
  int64_t err;
  ino_t child_inode;

  if (child_dentry == NULL || parent_inode == NULL) {
    return -1;
  }
  name = child_dentry->d_name.name;
  root = parent_inode->i_ino;
  err = fs_create(parent_inode->i_sb->s_fs_info, root, name, "file",
                  &child_inode);
  if (err != 0) {
    return err;
  }
  inode = networkfs_get_inode(parent_inode->i_sb, NULL, S_IFREG, child_inode);
  d_add(child_dentry, inode);
  return 0;
}

int networkfs_unlink(struct inode *parent_inode, struct dentry *child_dentry) {
  const char *name = child_dentry->d_name.name;
  ino_t root;
  root = parent_inode->i_ino;
  return fs_unlink(parent_inode->i_sb->s_fs_info, root, name);
}

int networkfs_mkdir(struct user_namespace *, struct inode *parent_inode,
                    struct dentry *child_dentry, umode_t mode) {
  struct inode *inode;
  const char *name;
  ino_t root;
  int64_t err;
  ino_t child_inode;

  if (child_dentry == NULL || parent_inode == NULL) {
    return -1;
  }
  name = child_dentry->d_name.name;
  root = parent_inode->i_ino;
  err = fs_create(parent_inode->i_sb->s_fs_info, root, name, "directory",
                  &child_inode);
  if (err != 0) {
    printk(KERN_ERR "mkdir error: %s size = %zu %lld\n", name, strlen(name),
           err);
    return err;
  }
  inode = networkfs_get_inode(parent_inode->i_sb, NULL, S_IFDIR, child_inode);
  d_add(child_dentry, inode);
  return 0;
}

int networkfs_rmdir(struct inode *parent_inode, struct dentry *child_dentry) {
  const char *name = child_dentry->d_name.name;
  ino_t root;
  root = parent_inode->i_ino;
  int64_t err = fs_rmdir(parent_inode->i_sb->s_fs_info, root, name);
  return err;
}

int networkfs_link(struct dentry *old_dentry, struct inode *parent_dir,
                   struct dentry *new_dentry) {
  return fs_link(parent_dir->i_sb->s_fs_info, old_dentry->d_inode->i_ino,
                 parent_dir->i_ino, new_dentry->d_name.name);
}

struct inode_operations networkfs_inode_ops = {
    .lookup = networkfs_lookup,
    .create = networkfs_create,
    .unlink = networkfs_unlink,
    .mkdir = networkfs_mkdir,
    .rmdir = networkfs_rmdir,
    .link = networkfs_link,
};

int networkfs_iterate(struct file *filp, struct dir_context *ctx) {
  char fsname[256];
  struct dentry *dentry;
  struct inode *inode;
  unsigned long offset;
  int stored;
  unsigned char ftype;
  ino_t ino;
  ino_t dino;
  struct entries *ents;
  int64_t err;

  dentry = filp->f_path.dentry;
  inode = dentry->d_inode;
  offset = filp->f_pos;
  stored = 0;
  ino = inode->i_ino;
  ents = kmalloc(sizeof(struct entries), GFP_KERNEL);
  if (ents == NULL) {
    return -1;
  }
  err = fs_list(dentry->d_sb->s_fs_info, ino, ents);
  if (err != 0) {
    return err;
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
    } else if (offset - 2 < ents->entries_count) {
      strcpy(fsname, ents->entries[offset - 2].name);
      if (ents->entries[offset - 2].entry_type == 4) {
        ftype = DT_DIR;
      } else {
        ftype = DT_REG;
      }
      dino = ents->entries[offset - 2].ino;
    } else {
      kfree(ents);
      return stored;
    }
    dir_emit(ctx, fsname, strlen(fsname), dino, ftype);
    stored++;
    offset++;
    ctx->pos = offset;
  }
  kfree(ents);
  return stored;
}

ssize_t networkfs_read(struct file *filp, char *buffer, size_t len,
                       loff_t *offset) {
  struct dentry *dentry;
  struct inode *inode;
  ino_t ino;
  struct content readed;
  ssize_t ret;

  dentry = filp->f_path.dentry;
  inode = dentry->d_inode;
  ino = inode->i_ino;
  if (fs_read(dentry->d_sb->s_fs_info, ino, &readed) != 0) {
    return -1;
  }
  if (*offset + len > readed.content_length) {
    ret = readed.content_length - *offset;
  } else {
    ret = len;
  }
  copy_to_user(buffer, readed.content + *offset, ret);
  *offset += ret;
  return ret;
}

ssize_t networkfs_write(struct file *filp, char const *buffer, size_t len,
                        loff_t *offset) {
  struct dentry *dentry;
  struct inode *inode;
  ino_t ino;
  ssize_t ret;
  struct content readed;

  dentry = filp->f_path.dentry;
  inode = dentry->d_inode;
  ino = inode->i_ino;
  if (*offset != 0 && fs_read(dentry->d_sb->s_fs_info, ino, &readed) != 0) {
    return -1;
  }
  if (len + *offset > 512) {
    ret = 512 - *offset;
  } else {
    ret = len;
  }
  copy_from_user(readed.content + *offset, buffer, ret);

  if (fs_write(dentry->d_sb->s_fs_info, ino, readed.content, *offset + ret) !=
      0) {
    return -1;
  }
  *offset += ret;
  return ret;
}

struct file_operations networkfs_dir_ops = {
    .iterate = networkfs_iterate,
    .write = networkfs_write,
    .read = networkfs_read,
};

struct file_operations networkfs_file_ops = {
    .write = networkfs_write,
    .read = networkfs_read,
};

struct inode *networkfs_get_inode(struct super_block *sb,
                                  const struct inode *dir, umode_t mode,
                                  ino_t i_ino) {
  struct inode *inode;
  inode = new_inode(sb);
  if (inode != NULL) {
    inode->i_op = &networkfs_inode_ops;
    if (mode | S_IFDIR) {
      inode->i_fop = &networkfs_dir_ops;
    } else {
      inode->i_fop = &networkfs_file_ops;
    }
    inode->i_ino = i_ino;
    mode |= S_IRWXUGO;
    inode_init_owner(&init_user_ns, inode, dir, mode);
  }
  return inode;
}

int networkfs_fill_super(struct super_block *sb, void *data, int silent) {
  struct inode *inode;
  inode = networkfs_get_inode(sb, NULL, S_IFDIR, 1000);
  sb->s_root = d_make_root(inode);
  if (sb->s_root == NULL) {
    return -ENOMEM;
  }
  printk(KERN_INFO "return 0\n");
  return 0;
}

struct dentry *networkfs_mount(struct file_system_type *fs_type, int flags,
                               const char *token, void *data) {
  struct dentry *ret;
  char *info;

  info = kmalloc(37, GFP_KERNEL);
  if (info != NULL) {
    ret = mount_nodev(fs_type, flags, data, networkfs_fill_super);
    if (ret == NULL) {
      kfree(info);
      printk(KERN_ERR "Can't mount file system");
    } else {
      printk(KERN_INFO "Mounted successfuly");
      ret->d_sb->s_fs_info = info;
      strcpy((char *)ret->d_sb->s_fs_info, token);
    }
    return ret;
  }
  printk(KERN_ERR "Can't mount file system");
  return NULL;
}

void networkfs_kill_sb(struct super_block *sb) {
  kfree(sb->s_fs_info);
  printk(KERN_INFO
         "networkfs super block is destroyed. Unmount successfully.\n");
}

struct file_system_type networkfs_fs_type = {.name = "networkfs",
                                             .mount = networkfs_mount,
                                             .kill_sb = networkfs_kill_sb};

// 6d3e63a1-c14e-45e1-8f90-31474fdb35cb
int networkfs_init(void) { return register_filesystem(&networkfs_fs_type); }

void networkfs_exit(void) { unregister_filesystem(&networkfs_fs_type); }

module_init(networkfs_init);
module_exit(networkfs_exit);
