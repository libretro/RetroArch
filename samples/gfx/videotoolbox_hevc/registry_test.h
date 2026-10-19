static void test_backend_withdrawal(void)
{
   rh265_hw_backend_t first, replacement;
   memset(&first, 0, sizeof(first)); memset(&replacement, 0, sizeof(replacement));
   assert(!rh265_unset_hw_backend(NULL));
   rh265_set_hw_backend(&first);
   assert(!rh265_unset_hw_backend(&replacement));
   assert(rh265_unset_hw_backend(&first));
   assert(!rh265_unset_hw_backend(&first));
   rh265_set_hw_backend(&replacement);
   assert(!rh265_unset_hw_backend(&first));
   assert(rh265_unset_hw_backend(&replacement));
   puts("HEVC backend withdrawal: replacement offers survive stale teardown");
}

#ifdef VIDEOTOOLBOX_HEVC_REGISTRY
typedef struct registry_device { retro_atomic_int_t refs; } registry_device_t;
static void *registry_retain(void *device)
{
   registry_device_t *ref = (registry_device_t*)device;
   assert(retro_atomic_fetch_add_int(&ref->refs, 1) > 0); return device;
}
static void registry_release(void *device)
{
   registry_device_t *ref = (registry_device_t*)device;
   assert(retro_atomic_fetch_sub_int(&ref->refs, 1) > 1);
}
typedef struct registry_race
{
   videotoolbox_hevc_registry_t *registry;
   registry_device_t *first, *second;
   retro_atomic_int_t reader_ready, writer_done;
} registry_race_t;
static void registry_writer(void *opaque)
{
   registry_race_t *work = (registry_race_t*)opaque;
   unsigned i;
   while (!retro_atomic_load_acquire_int(&work->reader_ready)) sthread_yield();
   for (i = 0; i < 1000; i++)
   {
      assert(videotoolbox_hevc_registry_publish(work->registry, work,
               (i & 1) ? work->first : work->second));
      if (!(i & 7)) assert(videotoolbox_hevc_registry_withdraw(work->registry, work));
      assert(retro_atomic_load_acquire_int(&work->first->refs) >= 2);
   }
   retro_atomic_store_release_int(&work->writer_done, 1);
}
static void registry_reader(void *opaque)
{
   registry_race_t *work = (registry_race_t*)opaque;
   unsigned i;
   void *held = videotoolbox_hevc_registry_acquire(work->registry);
   assert(held == work->first);
   retro_atomic_store_release_int(&work->reader_ready, 1);
   for (i = 0; i < 1000; i++)
   {
      void *device = videotoolbox_hevc_registry_acquire(work->registry);
      (void)videotoolbox_hevc_registry_is_owner(work->registry, work);
      if (device) registry_release(device);
   }
   while (!retro_atomic_load_acquire_int(&work->writer_done)) sthread_yield();
   registry_release(held);
}
#endif

static void test_registry_lifetime(void)
{
#ifdef VIDEOTOOLBOX_HEVC_REGISTRY
   videotoolbox_hevc_registry_t registry;
   registry_device_t first, second;
   rh265_hw_backend_t foreign;
   registry_race_t work;
   owner_queue_t queue;
   slock_t *lock = slock_new();
   sthread_t *writer, *reader;
   void *snapshot;
   assert(lock);
   memset(&queue, 0, sizeof(queue)); memset(&foreign, 0, sizeof(foreign));
   retro_atomic_int_init(&first.refs, 1); retro_atomic_int_init(&second.refs, 1);
   assert(videotoolbox_hevc_registry_init(&registry, lock, registry_retain,
            registry_release, NULL, NULL, owner_enqueue, &queue, true));
   assert(!videotoolbox_hevc_registry_publish(&registry, NULL, &first));
   assert(!registry.backend.hdr_enabled(&registry));
   assert(videotoolbox_hevc_registry_publish_support(&registry, &first, &first, true));
   assert(registry.backend.hdr_enabled(&registry));
   assert(!videotoolbox_hevc_registry_withdraw(&registry, &second));
   assert(registry.backend.hdr_enabled(&registry));
   assert(videotoolbox_hevc_registry_withdraw(&registry, &first));
   assert(!registry.backend.hdr_enabled(&registry));
   assert(!videotoolbox_hevc_registry_publish(&registry, &first, NULL));
   assert(videotoolbox_hevc_registry_publish(&registry, &first, &first));
   snapshot = videotoolbox_hevc_registry_acquire(&registry); assert(snapshot == &first);
   assert(videotoolbox_hevc_registry_publish(&registry, &second, &second));
   assert(!videotoolbox_hevc_registry_withdraw(&registry, &first));
   assert(videotoolbox_hevc_registry_is_owner(&registry, &second));
   rh265_set_hw_backend(&foreign);
   assert(videotoolbox_hevc_registry_withdraw(&registry, &second));
   assert(!rh265_unset_hw_backend(&registry.backend.offer));
   assert(rh265_unset_hw_backend(&foreign));
   assert(!videotoolbox_hevc_registry_acquire(&registry));
   assert(retro_atomic_load_acquire_int(&first.refs) == 2);
   registry_release(snapshot);
   work.registry = &registry; work.first = &first; work.second = &second;
   retro_atomic_int_init(&work.reader_ready, 0); retro_atomic_int_init(&work.writer_done, 0);
   assert(videotoolbox_hevc_registry_publish(&registry, &work, &first));
   writer = sthread_create(registry_writer, &work); reader = sthread_create(registry_reader, &work);
   assert(writer && reader); sthread_join(writer); sthread_join(reader);
   assert(videotoolbox_hevc_registry_withdraw(&registry, &work));
   assert(!videotoolbox_hevc_registry_is_owner(&registry, &work));
   assert(retro_atomic_load_acquire_int(&first.refs) == 1);
   assert(retro_atomic_load_acquire_int(&second.refs) == 1 && !queue.submitted);
   slock_free(lock);
   puts("VideoToolbox registry: device replacement, stale withdrawal and 1000 concurrent snapshots passed");
#endif
}
