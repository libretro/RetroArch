/* Setup scheduler only; exported Metal leases use the existing event mock. */
typedef void *dispatch_queue_t;
enum { DISPATCH_QUEUE_PRIORITY_DEFAULT = 0 };
static dispatch_queue_t dispatch_get_global_queue(int priority, unsigned long flags)
{ (void)priority; (void)flags; return NULL; }
static unsigned pool_setup_event, pool_setup_fail;
@interface MockDevice (PoolSetup)
- (id)newSharedEvent;
@end
@implementation MockDevice (PoolSetup)
- (id)newSharedEvent
{
   MockEvent *event;
   if (++pool_setup_event == pool_setup_fail) return nil;
   event = [MockEvent new]; event->_device = [self retain];
   return event;
}
@end
@interface MTLSharedEventListener (PoolSetup)
- (id)initWithDispatchQueue:(dispatch_queue_t)queue;
@end
@implementation MTLSharedEventListener (PoolSetup)
- (id)initWithDispatchQueue:(dispatch_queue_t)queue
{
   (void)queue;
   if (pool_setup_fail == 6) { [self release]; return nil; }
   return [self init];
}
@end
static int CVMetalTextureCacheCreate(const void *allocator, const void *attrs,
      id<MTLDevice> device, const void *cache_attrs, CVMetalTextureCacheRef *out)
{
   (void)allocator; (void)attrs; (void)cache_attrs;
   *out = [device retain];
   return pool_setup_fail == 5 ? -1 : kCVReturnSuccess;
}
static void test_cv_pool_setup(void)
{
   MockDevice *device = [MockDevice new];
   NSObject *unsupported = [NSObject new];
   unsigned i;
   assert(!metal_frame_cv_pool_create(NULL, 3));
   assert(!metal_frame_cv_pool_create(unsupported, 3));
   assert(!metal_frame_cv_pool_create(device, 0));
   assert(!metal_frame_cv_pool_create(device, 33));
   for (i = 0; i <= 6; i++)
   {
      metal_frame_cv_pool_t *pool;
      pool_setup_event = 0; pool_setup_fail = i;
      pool = metal_frame_cv_pool_create(device, 3);
      if (!i) { assert(pool && pool->count == 3); metal_frame_cv_pool_free(pool); }
      else assert(!pool);
      assert([device retainCount] == 1);
   }
   pool_setup_fail = pool_setup_event = 0;
   {
      metal_frame_cv_pool_t *pool = metal_frame_cv_pool_create(device, 32);
      assert(pool && pool->count == 32 && pool_setup_event == 33);
      metal_frame_cv_pool_free(pool); assert([device retainCount] == 1);
   }
   [unsupported release]; [device release];
   puts("CoreVideo Metal pool setup: capability, allocation failure and retained-device cleanup passed");
}
