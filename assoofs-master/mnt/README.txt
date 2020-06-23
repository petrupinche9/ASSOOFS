librearias hello.c
estructura sencilla ??? esta en assofs.h
metodo init
metodo exit (module????)


static struct kmem_cache *assoofs_inode_cache;
cambiar makefile

static int __init_assofs_init(void)
int ret
assofs_inode_cache = kmem_cache_create("assoofs_inode_cache",sizeof(struct assoofs_inode_info),0,(SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD), NULL);
if(!assofs_inode_cache) return -ENOMEM;

ret = register filesystem(&assoofs_type);
if(ret ==0)
	printk(KERN_INFO "Succesfully registered assoofs\n");
else
	printk(KERN_ERR "Failed to register assooff Error %d",ret);
return ret;


static void __exit_assoofs_exit(void) {
	int ret;
	ret = unregister_filesystem(&assoofs_type);
	kmem_cache_destroy(assoofs_inode_cache);
	if(ret == 0)
		printk(KERN_INFO "Succesfully unregistered assoofs\n");
 	else
		printk(KERN_ERR "Failed to unregister assoofs, Error %d",ret);
}

