/* Shared by real Metal and the Objective-C ownership mock. */
static void pool_test_wait(void *event, void *listener, uint64_t value)
{
#ifdef __APPLE__
   dispatch_semaphore_t finished = dispatch_semaphore_create(0);
   [(__bridge id<RArchMetalSharedEvent>)event notifyListener:(__bridge id)listener
      atValue:value block:^(id unused, uint64_t actual) {
         (void)unused; assert(actual >= value); dispatch_semaphore_signal(finished);
      }];
   assert(!dispatch_semaphore_wait(finished, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC)));
#else
   (void)listener;
   assert([(__bridge id<RArchMetalSharedEvent>)event signaledValue] >= value);
#endif
}

static void test_cv_pool(void *device, void *cache, void *image,
      unsigned pixfmt, unsigned width, unsigned height)
{
#ifdef __APPLE__
   id ready = [(__bridge id<MTLDevice>)device newSharedEvent];
   id done[2] = { [(__bridge id<MTLDevice>)device newSharedEvent],
      [(__bridge id<MTLDevice>)device newSharedEvent] };
   id listener = [[MTLSharedEventListener alloc]
      initWithDispatchQueue:dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0)];
#else
   id ready = [MockEvent new], done[2] = { [MockEvent new], [MockEvent new] };
   id listener = [MTLSharedEventListener new];
#endif
   void *events[2] = { (__bridge void*)done[0], (__bridge void*)done[1] };
   metal_frame_cv_pool_t *pool;
   metal_frame_view_t metadata;
   video_hw_frame_t frames[3], probe, sentinel;
   assert(ready && done[0] && done[1] && listener);
   memset(&metadata, 0, sizeof(metadata));
   metadata.pixfmt = pixfmt; metadata.width = width; metadata.height = height;
   assert(!metal_frame_cv_pool_new(device, cache, (__bridge void*)ready,
            (__bridge void*)listener, events, 2));
   [ready setSignaledValue:1];
   events[1] = events[0];
   assert(!metal_frame_cv_pool_new(device, cache, (__bridge void*)ready,
            (__bridge void*)listener, events, 2));
   events[1] = (__bridge void*)done[1];
   assert(!metal_frame_cv_pool_new(device, cache, (__bridge void*)ready,
            (__bridge void*)listener, events, 33));
   pool = metal_frame_cv_pool_new(device, cache, (__bridge void*)ready,
         (__bridge void*)listener, events, 2);
   assert(pool);
   memset(&probe, 0xa5, sizeof(probe)); sentinel = probe;
   metadata.crop_x = 1;
   assert(metal_frame_cv_pool_export(pool, &metadata, image, &probe) == -1);
   assert(!memcmp(&probe, &sentinel, sizeof(probe)) && !pool->value[0]);
   metadata.crop_x = 0;
   assert(!metal_frame_cv_pool_export(pool, &metadata, image, &frames[0]));
   assert(!metal_frame_cv_pool_export(pool, &metadata, image, &frames[1]));
   assert(frames[0].done != frames[1].done);
   assert(frames[0].done_value == 1 && frames[1].done_value == 1);
   assert(metal_frame_cv_pool_export(pool, &metadata, image, &probe) == 1);
   assert(!memcmp(&probe, &sentinel, sizeof(probe)));
   video_hw_frame_abandon(&frames[1]);
   pool_test_wait(events[1], (__bridge void*)listener, 1);
   assert(!metal_frame_cv_pool_export(pool, &metadata, image, &frames[2]));
   assert(frames[2].done == (uint64_t)(uintptr_t)events[1] && frames[2].done_value == 2);
   assert(metal_frame_cv_pool_export(pool, &metadata, image, &probe) == 1);
   assert(!memcmp(&probe, &sentinel, sizeof(probe)));
   /* Outstanding frames own all the objects they need after pool destruction. */
   metal_frame_cv_pool_free(pool);
   video_hw_frame_abandon(&frames[0]); video_hw_frame_abandon(&frames[2]);
   pool_test_wait(events[0], (__bridge void*)listener, 1);
   pool_test_wait(events[1], (__bridge void*)listener, 2);
   RARCH_RELEASE(ready); RARCH_RELEASE(done[0]); RARCH_RELEASE(done[1]); RARCH_RELEASE(listener);
   puts("CoreVideo Metal pool: bounded pressure, independent completion, reuse and teardown passed");
}
