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
int assoofs_save_inode_info(struct super_block *sb, struct assoofs_inode_info *inode_info);
static struct kmem_cache *assoofs_inode_cache;


const struct file_operations assoofs_file_operations = {
    .read = assoofs_read,
    .write = assoofs_write,
};

ssize_t assoofs_read(struct file * filp, char __user * buf, size_t len, loff_t * ppos) {
    printk(KERN_INFO "Read request\n");
struct assoofs_inode_info *inode_info ;
struct buffer_head *bh;
char *buffer;
int nbytes;
inode_info= filp->f_path.dentry->d_inode->i_private;

if (*ppos >= inode_info->file_size) return 0;

bh = sb_bread(filp->f_path.dentry->d_inode->i_sb, inode_info->data_block_number);
buffer = (char *)bh->b_data;
nbytes = min((size_t) inode_info->file_size, len); // Hay que comparar len con el tama~no del fichero por si llegamos alfinal del fichero
copy_to_user(buf, buffer, nbytes);
*ppos += nbytes;
return nbytes;
}

ssize_t assoofs_write(struct file * filp, const char __user * buf, size_t len, loff_t * ppos) {
    printk(KERN_INFO "Write request\n");
struct assoofs_inode_info *inode_info;
inode_info= filp->f_path.dentry->d_inode->i_private;
if (*ppos >= inode_info->file_size) return 0;
struct buffer_head *bh;
char *buffer;
bh = sb_bread(filp->f_path.dentry->d_inode->i_sb, inode_info->data_block_number);
buffer = (char *)bh->b_data;
buffer += *ppos;
copy_from_user(buffer, buf, len);
inode_info->file_size = *ppos;
assoofs_save_inode_info(filp->f_path.dentry->d_inode->i_sb, inode_info);
return len;
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
    struct inode *inode;
    struct super_block *sb;
    struct assoofs_inode_info *inode_info;
    struct buffer_head *bh;
    int i = 0;
    printk(KERN_INFO "Iterate request\n");
    inode = filp->f_path.dentry->d_inode;
    sb = inode->i_sb;
    inode_info = inode->i_private;
    if (ctx->pos) return 0;
    if ((!S_ISDIR(inode_info->mode))) return -1;
    bh = sb_bread(sb, inode_info->data_block_number);
    struct assoofs_dir_record_entry *record ;
         record = (struct assoofs_dir_record_entry *) bh->b_data;
    for (i = 0; i < inode_info->dir_children_count; i++) {
        dir_emit(ctx, record->filename, ASSOOFS_FILENAME_MAXLEN, record->inode_no, DT_UNKNOWN);
        ctx->pos += sizeof(struct assoofs_dir_record_entry);
        record++;
    }

    brelse(bh);
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
    struct assoofs_super_block_info *afs_sb ;
    struct assoofs_inode_info *buffer = NULL;
    int i=0;
    bh = sb_bread(sb, ASSOOFS_INODESTORE_BLOCK_NUMBER);
    inode_info = (struct assoofs_inode_info *)bh->b_data;
    afs_sb = sb->s_fs_info;
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
    struct inode *inod;
    struct assoofs_inode_info *inode_info=NULL;
    inode_info = assoofs_get_inode_info(sb, ino);
   // uint64_t count;
   // count=((struct assoofs_super_block_info *)sb->s_fs_info)->inodes_count; // obtengo el n ́umero de inodos de lainformaci ́on persistente del superbloque;


    inod=new_inode(sb);
    inod->i_ino = ino; // ńumero de inodo
    inod->i_sb = sb; // puntero al superbloque
    inod->i_op = &assoofs_inode_ops; // direcci ́on de una variable de tipo struct inode_operations previamente declarada
    if (S_ISDIR(inode_info->mode))
        inod->i_fop = &assoofs_dir_operations;
    else if (S_ISREG(inode_info->mode))
        inod->i_fop = &assoofs_file_operations;
    else
        printk(KERN_ERR "Unknown inode type. Neither a directory nor a file."); // direccion de una variable de tipo struct file_operations previamente declarada
    inod->i_atime = inod->i_mtime = inod->i_ctime = current_time(inod);
    inod->i_private = inode_info; // Informaci ́on persistente del inodo
    inode_info = kmem_cache_alloc(assoofs_inode_cache, GFP_KERNEL);
    return inod;

};

struct dentry *assoofs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags) {
    printk(KERN_INFO"Lookup request\n");
    struct assoofs_inode_info *parent_info=NULL;
    struct super_block *sb;
    struct buffer_head *bh;
    struct assoofs_dir_record_entry *record;
    struct inode *inod;
    parent_info = parent_inode->i_private;
    sb=parent_inode->i_sb;
    bh = sb_bread(sb, parent_info->data_block_number);
    printk(KERN_INFO"Lookup in: \nino=%llu   b=%llu\n", parent_info->inode_no, parent_info->data_block_number);
    record = (struct assoofs_dir_record_entry *) bh->b_data;
    int i=0;
    for (i = 0; i < parent_info->dir_children_count; i++) {
        if (!strcmp(record->filename, child_dentry->d_name.name)) {
            inod= assoofs_get_inode(sb,record->inode_no); // Funcion auxiliar que obtiene la informacion de un inodo a partir de su numero de inodo.
            inode_init_owner(inod, parent_inode, ((struct assoofs_inode_info *) inod->i_private)->mode);
            d_add(child_dentry, inod);
            return NULL;
        }
        record++;
    }
    return NULL;
}

struct assoofs_inode_info *assoofs_search_inode_info(struct super_block *sb, struct assoofs_inode_info *start, struct assoofs_inode_info *search){
    uint64_t count = 0;
    while (start->inode_no != search->inode_no && count < ((struct assoofs_super_block_info *)sb->s_fs_info)->inodes_count) {
        count++;
        start++;
    }
    if (start->inode_no == search->inode_no)
        return start;
    else
        return NULL;
}

int assoofs_save_inode_info(struct super_block *sb, struct assoofs_inode_info *inode_info){
    struct buffer_head *bh;
    struct assoofs_inode_info *inode_pos=NULL;
    bh = sb_bread(sb, ASSOOFS_INODESTORE_BLOCK_NUMBER);
    inode_pos = assoofs_search_inode_info(sb, (struct assoofs_inode_info *)bh->b_data, inode_info);
    memcpy(inode_pos, inode_info, sizeof(*inode_pos));
    inode_pos = kmem_cache_alloc(assoofs_inode_cache, GFP_KERNEL);
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    return 0;
}


void assoofs_save_sb_info(struct super_block *vsb){
    struct buffer_head *bh;
    struct assoofs_super_block *sb ;
            sb = vsb->s_fs_info; // Informaci ́on persistente del superbloque en memoria
    bh = sb_bread(vsb, ASSOOFS_SUPERBLOCK_BLOCK_NUMBER);
    bh->b_data = (char *)sb; // Sobreescribo los datos de disco con la informaci ́on en memoria
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);
}

void assoofs_add_inode_info(struct super_block *sb, struct assoofs_inode_info *inode){
    struct buffer_head *bh;
    struct assoofs_super_block_info *assoofs_sb = sb->s_fs_info;
    bh = sb_bread(sb, ASSOOFS_INODESTORE_BLOCK_NUMBER);
    inode = (struct assoofs_inode_info *)bh->b_data;
    inode += assoofs_sb->inodes_count;
    memcpy(inode, inode, sizeof(struct assoofs_inode_info));
    inode = kmem_cache_alloc(assoofs_inode_cache, GFP_KERNEL);
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    assoofs_sb->inodes_count++;
    assoofs_save_sb_info(sb);
}

int assoofs_sb_get_a_freeblock(struct super_block *sb, uint64_t *block){
    struct assoofs_super_block_info *assoofs_sb ;
    int i=0;
    assoofs_sb= sb->s_fs_info;
    for (i = 2; i < ASSOOFS_MAX_FILESYSTEM_OBJECTS_SUPPORTED; i++)
        if (assoofs_sb->free_blocks & (1 << i))
            break; // cuando aparece el primer bit 1 en free_block dejamos de recorrer el mapa de bits, i tiene la posici ́ondel primer bloque libre
    *block = i; // Escribimos el valor de i en la direcci ́on de memoria indicada como segundo argumento en la funci ́on
    assoofs_sb->free_blocks &= ~(1 << i);
    assoofs_save_sb_info(sb);
    return 0;
}

static int assoofs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl) {
    printk(KERN_INFO "New file request\n");
    struct inode *inode;
    uint64_t count;
    struct buffer_head *bh;
    struct super_block *sb;
    struct assoofs_inode_info *parent_inode_info=NULL;
    struct assoofs_dir_record_entry *dir_contents;
    struct assoofs_inode_info *inode_info;
    // obtengo un puntero al superbloque desde dir
    sb = dir->i_sb;
     // obtengo el n ́umero de inodos de lainformaci ́on persistente del superbloque
    count = ((struct assoofs_super_block_info *)sb->s_fs_info)->inodes_count;

    if(count<ASSOOFS_MAX_FILESYSTEM_OBJECTS_SUPPORTED) {
        //CREACION NUEVO INODO
        inode = new_inode(sb);
        inode->i_ino = count + 1;
        inode_info = kmalloc(sizeof(struct assoofs_inode_info), GFP_KERNEL);
        inode_info->inode_no = inode->i_ino;
        inode_info->mode = mode; // El segundo mode me llega como argumento
        inode_info->file_size = 0;
        inode->i_private = inode_info;
        inode_init_owner(inode, dir, mode);
        d_add(dentry, inode);

        /*if (S_ISDIR(inode_info->mode))
            inode->i_fop = &assoofs_dir_operations;
        else if (S_ISREG(inode_info->mode))

        else
            printk(KERN_ERR "Unknown inode type. Neither a directory nor a file.");
*/      inode->i_fop = &assoofs_file_operations;
        inode_info = kmem_cache_alloc(assoofs_inode_cache, GFP_KERNEL);
        assoofs_sb_get_a_freeblock(sb, &inode_info->data_block_number);
        assoofs_add_inode_info(sb, inode_info);// Asigno n ́umero al nuevo inodo a partir de count

        //MODIFICAR EL CONTENIDO DEL DIRECTORIO PADRE  ADJUNTANDO UNA NUEVA ENTRADA PARA EL NUEVO FICHERO O DIRECTORIO

        parent_inode_info = dir->i_private;
        bh = sb_bread(sb, parent_inode_info->data_block_number);
        dir_contents = (struct assoofs_dir_record_entry *)bh->b_data;
        dir_contents += parent_inode_info->dir_children_count;
        dir_contents->inode_no = inode_info->inode_no; // inode_info es la informaci ́on persistente del inodo creado en el paso 2.
        strcpy(dir_contents->filename, dentry->d_name.name);
        mark_buffer_dirty(bh);
        sync_dirty_buffer(bh);
        brelse(bh);

        //ACTUALIZAR LA INFORMACON DEL INODO PADRE INDICANDO QUE AHORA TIENE UN ARCHIVO MAS
        parent_inode_info->dir_children_count++;
        assoofs_save_inode_info(sb, parent_inode_info);
        return 0;
    }else {
        printk(KERN_ERR "MAXIMUM NUMBER OF OBJECTS EXCEEDED\n");
        return -1;
    }

    return 0;
}

static int assoofs_mkdir(struct inode *dir , struct dentry *dentry, umode_t mode) {
    printk(KERN_INFO "New directory request\n");
    struct inode *inode;
    uint64_t count;
    struct buffer_head *bh;
    struct super_block *sb;
    struct assoofs_inode_info *parent_inode_info;
    struct assoofs_dir_record_entry *dir_contents;
    struct assoofs_inode_info *inode_info;

    // obtengo un puntero al superbloque desde dir
    sb = dir->i_sb;

    // obtengo el n ́umero de inodos de lainformaci ́on persistente del superbloque
    count = ((struct assoofs_super_block_info *)sb->s_fs_info)->inodes_count;
    inode = new_inode(sb);
    if(count<ASSOOFS_MAX_FILESYSTEM_OBJECTS_SUPPORTED) {

        //CREACION NUEVO INODO
        inode->i_ino = count + 1;
        inode_info = kmalloc(sizeof(struct assoofs_inode_info), GFP_KERNEL);
        inode_info->inode_no = inode->i_ino;
        inode_info->mode = S_IFDIR | mode; // El segundo mode me llega como argumento
        inode_info->file_size = 0;
        inode_info->dir_children_count = 0;
        inode->i_private = inode_info;
        inode_init_owner(inode, dir, mode);
        d_add(dentry, inode);
        inode->i_fop=&assoofs_dir_operations;
        inode_info = kmem_cache_alloc(assoofs_inode_cache, GFP_KERNEL);
        assoofs_sb_get_a_freeblock(sb, &inode_info->data_block_number);
        assoofs_add_inode_info(sb, inode_info);// Asigno numero al nuevo inodo a partir de count


        //MODIFICAR EL CONTENIDO DEL DIRECTORIO PADRE  ADJUNTANDO UNA NUEVA ENTRADA PARA EL NUEVO FICHERO O DIRECTORIO
        parent_inode_info = dir->i_private;
        bh = sb_bread(sb, parent_inode_info->data_block_number);
        dir_contents = (struct assoofs_dir_record_entry *)bh->b_data;
        dir_contents += parent_inode_info->dir_children_count;
        dir_contents->inode_no = inode_info->inode_no; // inode_info es la informaci ́on persistente del inodo creado en el paso 2.
        strcpy(dir_contents->filename, dentry->d_name.name);
        mark_buffer_dirty(bh);
        sync_dirty_buffer(bh);
        brelse(bh);
        //ACTUALOZAR LA INFORMACON DEL INODO PADRE INDICANDO QUE AHORA TIENE UN ARCHIVO MAS
        parent_inode_info->dir_children_count++;
        assoofs_save_inode_info(sb, parent_inode_info);
        return 0;

    }else{
        printk(KERN_ERR "MAXIMUM NUMBER OF OBJECTS EXCEEDED\n");
        return -1;
    }
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
    struct inode *root_inode;
    struct assoofs_super_block *asb;
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
    printk(KERN_INFO "ASSOOFS FILESYSTEM WITH \nVERSION: %llu \nBLOCKSIZE: %llu\nMAGIC NUMBER= %llu",assoofs_sb->version, assoofs_sb->block_size, assoofs_sb->magic);
    // 3.- Escribir la información persistente leída del dispositivo de bloques en el superbloque sb, incluído el campo s_op con las operaciones que soporta.
    sb->s_magic=ASSOOFS_MAGIC;
    sb->s_fs_info=assoofs_sb;
    sb->s_op=&assoofs_sops;
sb->s_maxbytes=ASSOOFS_DEFAULT_BLOCK_SIZE;
//Para no tener que acceder al bloque 0 del disco constantemente guardaremos la informacion léıda
// del bloque 0 n el campo s_fs_info del superbloque sb.
  asb=sb->s_fs_info;
bh = sb_bread(sb, ASSOOFS_SUPERBLOCK_BLOCK_NUMBER);
bh->b_data = (char *)asb;
mark_buffer_dirty(bh);
sync_dirty_buffer(bh);
brelse(bh);
    // 4.- Crear el inodo raíz y asignarle operaciones sobre inodos (i_op) y sobre directorios (i_fop)

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
    if (IS_ERR(ret)) {
        printk(KERN_ERR"Failed to unregister assoofs");
    }
};

void assoofs_destroy_inode(struct inode *inode) {
    struct assoofs_inode *inode_info = inode->i_private;
    printk(KERN_INFO "Freeing private data of inode %p ( %lu)\n", inode_info, inode->i_ino);
    kmem_cache_free(assoofs_inode_cache, inode_info);
}
/*
 *  assoofs file system type
 */
static struct file_system_type assoofs_type = {
    .owner   = THIS_MODULE,
    .name    = "assoofs",
    .mount   = assoofs_mount,
    .kill_sb = kill_litter_super,
};


static int __init assoofs_init(void) {
    printk(KERN_INFO "assoofs_init request\n");
    assoofs_inode_cache = kmem_cache_create("assoofs_inode_cache",sizeof(struct assoofs_inode_info),0,(SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD), NULL);
    if(!assoofs_inode_cache) return -ENOMEM;

    int ret = register_filesystem(&assoofs_type);
    // Control de errores a partir del valor de ret
    if(ret == 0) {
        printk(KERN_INFO
        "Succesfully registered assoofs\n");
        return 0;
    }else{
        printk(KERN_ERR "Failed to register assooff");
        return -EPERM;
    }

}

static void __exit assoofs_exit(void) {
    printk(KERN_INFO "assoofs_exit request\n");
    int ret = unregister_filesystem(&assoofs_type);
    // Control de errores a partir del valor de ret
    kmem_cache_destroy(assoofs_inode_cache);
    if(ret == 0) {
        printk(KERN_INFO
        "Succesfully unregistered assoofs\n");
    }else{
        printk(KERN_ERR"Failed to unregister assoofs");
    }
}

module_init(assoofs_init);
module_exit(assoofs_exit);
