#include <linux/module.h>       /* Needed by all modules */
#include <linux/kernel.h>       /* Needed for KERN_INFO  */
#include <linux/init.h>         /* Needed for the macros */
#include <linux/fs.h>           /* libfs stuff           */
#include <linux/buffer_head.h>  /* buffer_head           */
#include <linux/slab.h>         /* kmem_cache            */
#include "assoofs.h"

/*
 *  Operaciones sobre ficheros
 */
ssize_t assoofs_read(struct file * filp, char __user * buf, size_t len, loff_t * ppos);
ssize_t assoofs_write(struct file * filp, const char __user * buf, size_t len, loff_t * ppos);
const struct file_operations assoofs_file_operations = {
    .read = assoofs_read,
    .write = assoofs_write,
};

ssize_t assoofs_read(struct file * filp, char __user * buf, size_t len, loff_t * ppos) {
    printk(KERN_INFO "Read request\n");
    return 0;
}

ssize_t assoofs_write(struct file * filp, const char __user * buf, size_t len, loff_t * ppos) {
    printk(KERN_INFO "Write request\n");
    return 0;
}

/*
 *  Operaciones sobre directorios
 */
static int assoofs_iterate(struct file *filp, struct dir_context *ctx);
const struct file_operations assoofs_dir_operations = {
    .owner = THIS_MODULE,
    .iterate = assoofs_iterate,
};

static int assoofs_iterate(struct file *filp, struct dir_context *ctx) {
    printk(KERN_INFO "Iterate request\n");
    return 0;
}

/*
 *  Operaciones sobre inodos
 */
static int assoofs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl);
struct dentry *assoofs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags);
static int assoofs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);
static struct inode_operations assoofs_inode_ops = {
    .create = assoofs_create,
    .lookup = assoofs_lookup,
    .mkdir = assoofs_mkdir,
};

struct assoofs_inode_info *assoofs_get_inode_info(struct super_block *sb, uint64_t inode_no) {
    struct assoofs_inode_info *inode_info = NULL;
    struct buffer_head *bh;
    bh = sb_bread(sb, ASSOOFS_INODESTORE_BLOCK_NUMBER);
    inode_info = (struct assoofs_inode_info *)bh->b_data;
    struct assoofs_super_block_info *afs_sb = sb->s_fs_info;
    struct assoofs_inode_info *buffer = NULL;
    int i;
    for (i = 0; i < afs_sb->inodes_count; i++) {
        if (inode_info->inode_no == inode_no) {
            buffer = kmalloc(sizeof(struct assoofs_inode_info), GFP_KERNEL);
            memcpy(buffer, inode_info, sizeof(*buffer));
            break;
        }
        inode_info++;
    }
    brelse(bh);
    return buffer;
};

static struct inode *assoofs_get_inode(struct super_block *sb, int ino){
    struct inode *inode;
    struct assoofs_inode_info *inode_info;
    inode_info = assoofs_get_inode_info(sb, ino);
   // uint64_t count;
   // count=((struct assoofs_super_block_info *)sb->s_fs_info)->inodes_count; // obtengo el n ́umero de inodos de lainformaci ́on persistente del superbloque;
    if (S_ISDIR(inode_info->mode))
        inode->i_fop = &assoofs_dir_operations;
    else if (S_ISREG(inode_info->mode))
        inode->i_fop = &assoofs_file_operations;
    else
        printk(KERN_ERR "Unknown inode type. Neither a directory nor a file.");
    inode=new_inode(sb);

    inode->i_ino = ino; // ńumero de inodo
    inode->i_sb = sb; // puntero al superbloque
    inode->i_op = &assoofs_inode_ops; // direcci ́on de una variable de tipo struct inode_operations previamente declarada
    inode->i_fop = &assoofs_dir_operations; // direccion de una variable de tipo struct file_operations previamente declarada
    inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
    inode->i_private = inode_info; // Informaci ́on persistente del inodo

    return inode;
};

struct dentry *assoofs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags) {
    printk(KERN_INFO"Lookup request\n");
    struct assoofs_inode_info *parent_info = parent_inode->i_private;
    struct super_block *sb = parent_inode->i_sb;
    struct buffer_head *bh;
    bh = sb_bread(sb, parent_info->data_block_number);
    struct assoofs_dir_record_entry *record;
    printk(KERN_INFO"Lookup in: \nino=%llu\nb=%llu\n", parent_info->inode_no, parent_info->data_block_number);
    record = (struct assoofs_dir_record_entry *) bh->b_data;
    int i=0;
    for (i = 0; i < parent_info->dir_children_count; i++) {
        if (!strcmp(record->filename, child_dentry->d_name.name)) {
            struct inode *inode = assoofs_get_inode(sb,record->inode_no); // Funci ́on auxiliar que obtine la informaci ́on deun inodo a partir de su n ́umero de inodo.
            inode_init_owner(inode, parent_inode, ((struct assoofs_inode_info *) inode->i_private)->mode);
            d_add(child_dentry, inode);
            return NULL;
        }
        record++;
    }
}
static int assoofs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl) {
    printk(KERN_INFO "New file request\n");
    return 0;
}

static int assoofs_mkdir(struct inode *dir , struct dentry *dentry, umode_t mode) {
    printk(KERN_INFO "New directory request\n");
    return 0;
}

/*
 *  Operaciones sobre el superbloque
 */
static const struct super_operations assoofs_sops = {
    .drop_inode = generic_delete_inode,
};
//OBTENER INFORMACION OERSISTENTE DE UN INODO

/*
 *  Inicialización del superbloque
 */
int assoofs_fill_super(struct super_block *sb, void *data, int silent) {   
    printk(KERN_INFO "assoofs_fill_super request\n");
    // 1.- Leer la información persistente del superbloque del dispositivo de bloques
    struct buffer_head *bh;
    struct assoofs_super_block_info *assoofs_sb;
    bh = sb_bread(sb, ASSOOFS_SUPERBLOCK_BLOCK_NUMBER); // sb lo recibe assoofs_fill_super como argumento
    assoofs_sb = (struct assoofs_super_block_info *)bh->b_data;
    brelse(bh);
    // 2.- Comprobar los parámetros del superbloque
    if(unlikely(assoofs_sb->magic!= ASSOOFS_MAGIC)) {
        printk(KERN_ERR "assoofs_fill_super: wrong magic number, this is not a filesystem of type ASSOOFS\n");
        brelse(bh);
        return -EPERM;
    }
     if(unlikely(assoofs_sb->block_size!= ASSOOFS_DEFAULT_BLOCK_SIZE)){
        printk(KERN_INFO "assoofs_fill_super: wrong blocksize\n");
         brelse(bh);
        return -EPERM;
    }
    printk(KERN_INFO "ASSOOFS FILESYSTEM WITH \nVERSION: %llu \nBLOCKSIZE: %llu",assoofs_sb->version, assoofs_sb->block_size);
    // 3.- Escribir la información persistente leída del dispositivo de bloques en el superbloque sb, incluído el campo s_op con las operaciones que soporta.
    sb->s_magic=ASSOOFS_MAGIC;
    sb->s_fs_info=assoofs_sb;
    sb->s_op=&assoofs_sops;
sb->s_maxbytes=ASSOOFS_DEFAULT_BLOCK_SIZE;
//Para no tener que acceder al bloque 0 del disco constantemente guardaremos la informacion léıda
// del bloque 0 n el campo s_fs_info del superbloque sb.
struct assoofs_super_block *asb = sb->s_fs_info;
bh = sb_bread(sb, ASSOOFS_SUPERBLOCK_BLOCK_NUMBER);
bh->b_data = (char *)sb;
mark_buffer_dirty(bh);
sync_dirty_buffer(bh);
brelse(bh);
    // 4.- Crear el inodo raíz y asignarle operaciones sobre inodos (i_op) y sobre directorios (i_fop)
    struct inode *root_inode;
root_inode = new_inode(sb);
inode_init_owner(root_inode, NULL, S_IFDIR); // S_IFDIR para directorios, S_IFREG para ficheros.

root_inode->i_ino = ASSOOFS_ROOTDIR_INODE_NUMBER; // ńumero de inodo
root_inode->i_sb = sb; // puntero al superbloque
root_inode->i_op = &assoofs_inode_ops; // direcci ́on de una variable de tipo struct inode_operations previamente declarada
root_inode->i_fop = &assoofs_dir_operations; // direccion de una variable de tipo struct file_operations previamente declarada
root_inode->i_atime = root_inode->i_mtime = root_inode->i_ctime = current_time(root_inode); // fechas.
root_inode->i_private = assoofs_get_inode_info(sb, ASSOOFS_ROOTDIR_INODE_NUMBER); // Informaci ́on persistente del inodo
sb->s_root = d_make_root(root_inode);
if (!sb->s_root){
    brelse(bh);
    return -ENOMEM;
}
return 0;

}

/*
 *  Montaje de dispositivos assoofs
 */
static struct dentry *assoofs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data) {
    printk(KERN_INFO "assoofs_mount request\n");
    struct dentry *ret = mount_bdev(fs_type, flags, dev_name, data, assoofs_fill_super);
    // Control de errores a partir del valor de ret. En este caso se puede utilizar la macro IS_ERR: if (IS_ERR(ret)) ...
};

/*
 *  assoofs file system type
 */
static struct file_system_type assoofs_type = {
    .owner   = THIS_MODULE,
    .name    = "assoofs",
    .mount   = assoofs_mount,
    .kill_sb = kill_litter_super,
};

static struct kmem_cache *assoofs_inode_cache;

static int __init assoofs_init(void) {
    printk(KERN_INFO "assoofs_init request\n");
    assoofs_inode_cache = kmem_cache_create("assoofs_inode_cache",sizeof(struct assoofs_inode_info),0,(SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD), NULL);
    if(!assoofs_inode_cache) return -ENOMEM;

    int ret = register_filesystem(&assoofs_type);
    // Control de errores a partir del valor de ret
    if(ret == 0)
        printk(KERN_INFO "Succesfully registered assoofs\n");
    else
    printk(KERN_ERR "Failed to register assooff Error %d",ret);
    return ret;
}

static void __exit assoofs_exit(void) {
    printk(KERN_INFO "assoofs_exit request\n");
    int ret = unregister_filesystem(&assoofs_type);
    // Control de errores a partir del valor de ret
    kmem_cache_destroy(assoofs_inode_cache);
    if(ret == 0)
        printk(KERN_INFO "Succesfully unregistered assoofs\n");
    else
    printk(KERN_ERR "Failed to unregister assoofs, Error %d",ret);
}

module_init(assoofs_init);
module_exit(assoofs_exit);
