#include <linux/module.h>
#include <linux/slab.h>

MODULE_LICENSE( "GPL" );

static struct kmem_cache *my_cachep;

typedef struct storage {
  int field1;
  int field2;
  int field3;
  char field4;
}storage;

storage *my_storage;


void cache_constructor(void * buff)
{
  storage *tmp_storage = buff;
  tmp_storage->field1 = 1;
  tmp_storage->field2 = 10;
  tmp_storage->field3 = 100;
  tmp_storage->field4 = 'a';
}

int cache_test( void )
{
  void *object = 0;

  printk( "Cache object size is %d\n", kmem_cache_size( my_cachep ) );

  object = kmem_cache_alloc( my_cachep, GFP_KERNEL );

  if (object) {
    printk("Allocation succeeded");
    my_storage = (storage *)object;

    printk("Field1 = %d, Field2 = %d, Field3 = %d, Field4 = %c", my_storage->field1, my_storage->field2, my_storage->field3, my_storage->field4);
    kmem_cache_free( my_cachep, object );
    
  }
  else
    printk("Allocation failed");

  return 0;
}


static int cache_init( void )
{

   printk( "Init module");

   my_cachep = kmem_cache_create( 
 		  "my_cache",            /* Name */
                  sizeof(storage),                    /* Object Size */
                  0,                     /* Alignment */
                  SLAB_HWCACHE_ALIGN,    /* Flags */
                  cache_constructor );          /* Constructor*/

  if(my_cachep)
    printk("Cache created");
  else
    printk("Cache not created");

  cache_test();

  return 0;
}


static void cache_clean(void)
{
	
   printk( "Quit" );	
   
   if (my_cachep) kmem_cache_destroy( my_cachep );

   return;

}

module_init( cache_init );
module_exit(cache_clean);
