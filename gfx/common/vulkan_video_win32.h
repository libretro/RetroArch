/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
/* Included by vulkan_video_decode.c on desktop Windows. */

bool vulkan_video_decode_win32_fallback_init(const vulkan_context_t *ctx, unsigned formats)
{
   static const GUID factory_iid =
      { 0x770aae78, 0xf26f, 0x4dba, { 0xa8, 0x29, 0x25, 0x3c, 0x83, 0xd1, 0xb3, 0x87 } };
   VkPhysicalDeviceIDProperties id;
   VkPhysicalDeviceProperties2 properties;
   VkPhysicalDeviceExternalImageFormatInfo external;
   VkPhysicalDeviceImageFormatInfo2 image;
   VkExternalImageFormatProperties shared;
   VkImageFormatProperties2 result;
   VkPhysicalDeviceExternalSemaphoreInfo semaphore;
   VkExternalSemaphoreProperties semaphore_properties;
   VkSemaphoreTypeCreateInfo semaphore_type;
   PFN_vkGetPhysicalDeviceProperties2 get_properties;
   PFN_vkGetPhysicalDeviceImageFormatProperties2 get_image;
   PFN_vkGetPhysicalDeviceExternalSemaphoreProperties get_semaphore;
   HRESULT (WINAPI *create_factory)(REFIID, void**);
   IDXGIFactory1 *factory = NULL;
   IDXGIAdapter1 *adapter = NULL;
   DXGI_ADAPTER_DESC1 description;
   HMODULE dxgi;
   unsigned index;
   bool ok = false;
   if (!ctx || !vvd.up || vvd.device != ctx->device
         || vvd.win32_fallback
#ifndef HAVE_RVP9
         || !vvd.h264_broken
#endif
         || !(ctx->flags & VK_CTX_FLAG_TIMELINE_SEMAPHORE) || !formats)
      return false;
   get_properties = (PFN_vkGetPhysicalDeviceProperties2)
      vulkan_symbol_wrapper_instance_proc_addr()(ctx->instance, "vkGetPhysicalDeviceProperties2");
   get_image = (PFN_vkGetPhysicalDeviceImageFormatProperties2)
      vulkan_symbol_wrapper_instance_proc_addr()(ctx->instance, "vkGetPhysicalDeviceImageFormatProperties2");
   get_semaphore = (PFN_vkGetPhysicalDeviceExternalSemaphoreProperties)
      vulkan_symbol_wrapper_instance_proc_addr()(ctx->instance, "vkGetPhysicalDeviceExternalSemaphoreProperties");
   if (!get_properties || !get_image || !get_semaphore)
      return false;
   memset(&id, 0, sizeof(id));
   memset(&properties, 0, sizeof(properties));
   memset(&external, 0, sizeof(external));
   memset(&image, 0, sizeof(image));
   memset(&shared, 0, sizeof(shared));
   memset(&result, 0, sizeof(result));
   memset(&semaphore, 0, sizeof(semaphore));
   memset(&semaphore_properties, 0, sizeof(semaphore_properties));
   memset(&semaphore_type, 0, sizeof(semaphore_type));
   id.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
   properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
   properties.pNext = &id;
   get_properties(ctx->gpu, &properties);
   if (!id.deviceLUIDValid)
      return false;
   external.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
   external.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;
   image.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
   image.pNext = &external;
   image.format = VK_FORMAT_R16G16B16A16_SFLOAT;
   image.type = VK_IMAGE_TYPE_2D;
   image.tiling = VK_IMAGE_TILING_OPTIMAL;
   image.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
   shared.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;
   result.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
   result.pNext = &shared;
   if (get_image(ctx->gpu, &image, &result) != VK_SUCCESS
         || !(shared.externalMemoryProperties.externalMemoryFeatures
            & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT))
      return false;
   semaphore.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO;
   semaphore_type.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
   semaphore_type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
   semaphore.pNext = &semaphore_type;
   semaphore.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT;
   semaphore_properties.sType = VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES;
   get_semaphore(ctx->gpu, &semaphore, &semaphore_properties);
   if (!(semaphore_properties.externalSemaphoreFeatures & VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT))
      return false;
   dxgi = LoadLibraryA("dxgi.dll");
   if (!dxgi)
      return false;
   create_factory = (HRESULT (WINAPI *)(REFIID, void**))(void (*)(void))
      GetProcAddress(dxgi, "CreateDXGIFactory1");
   if (create_factory && SUCCEEDED(create_factory(&factory_iid, (void**)&factory)))
   {
      for (index = 0; factory->lpVtbl->EnumAdapters1(factory, index, &adapter) == S_OK; index++)
      {
         bool match = SUCCEEDED(adapter->lpVtbl->GetDesc1(adapter, &description))
            && !memcmp(&description.AdapterLuid, id.deviceLUID, sizeof(description.AdapterLuid));
         if (match)
            ok = d3d11_video_decode_init_packed_adapter(adapter, formats,
                  vvd.h264_broken);
         adapter->lpVtbl->Release(adapter);
         if (match) break;
      }
      factory->lpVtbl->Release(factory);
   }
   FreeLibrary(dxgi);
   vvd.win32_fallback = ok;
   return ok;
}
