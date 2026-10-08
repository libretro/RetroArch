// Copyright (c) 2017-2026 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT
//

#include "loader_properties.hpp"
#include <platform_utils.hpp>

#include <string>

#include "loader_lockfree.h"

namespace {

typedef struct loader_property {
    const char* name;
    const char* value;
} loader_property_t;

/* The published override set, and the sets it replaced: freed at exit,
 * when no reader can still be walking one. */
struct LoaderPropertyStore {
    retro_atomic_ptr_t current;
    mpsc_stack_t retired;

    ~LoaderPropertyStore() {
        free(retro_atomic_exchange_ptr(&current, NULL));
        loader_snapshot_free_retired(&retired);
    }
};

LoaderPropertyStore loader_property_store;

/* The first override named name, as the map's insert kept the first. */
const char* loader_property_find(const char* name) {
    size_t i;
    const loader_property_t* props;
    const loader_snapshot_t* s = (const loader_snapshot_t*)retro_atomic_load_acquire_ptr(&loader_property_store.current);
    if (!s) {
        return NULL;
    }
    props = LOADER_SNAPSHOT_ITEMS(s, const loader_property_t);
    for (i = 0; i < s->count; i++) {
        if (!strcmp(props[i].name, name)) {
            return props[i].value;
        }
    }
    return NULL;
}

}  // namespace

// Loader property overrides take precedence over system environment variables because environment variables are not always
// safe to use (and thus would be ignored). For example, override properties may be the only way to redirect XR_RUNTIME_JSON
// from an elevated process on Windows.

namespace LoaderProperty {

std::string Get(const std::string& name) {
    const char* value = loader_property_find(name.c_str());
    if (value) {
        return value;
    }
    return PlatformUtilsGetEnv(name.c_str());
}

std::string GetSecure(const std::string& name) {
    const char* value = loader_property_find(name.c_str());
    if (value) {
        return value;
    }
    return PlatformUtilsGetSecureEnv(name.c_str());
}

bool IsSet(const std::string& name) { return loader_property_find(name.c_str()) || PlatformUtilsGetEnvSet(name.c_str()); }

bool SetOverrides(const XrLoaderInitPropertyValueEXT* values, uint32_t count) {
    uint32_t i;
    size_t bytes = 0;
    char* text;
    loader_property_t* props;
    loader_snapshot_t* previous;
    loader_snapshot_t* s;

    for (i = 0; i < count; i++) {
        bytes += strlen(values[i].name) + strlen(values[i].value) + 2;
    }
    if (!(s = loader_snapshot_new(count, sizeof(loader_property_t), bytes))) {
        return false;
    }
    props = LOADER_SNAPSHOT_ITEMS(s, loader_property_t);
    text = (char*)(props + count);
    for (i = 0; i < count; i++) {
        size_t name_len = strlen(values[i].name) + 1;
        size_t value_len = strlen(values[i].value) + 1;
        memcpy(text, values[i].name, name_len);
        props[i].name = text;
        text += name_len;
        memcpy(text, values[i].value, value_len);
        props[i].value = text;
        text += value_len;
    }

    previous = (loader_snapshot_t*)retro_atomic_exchange_ptr(&loader_property_store.current, s);
    if (previous) {
        mpsc_stack_push(&loader_property_store.retired, &previous->retire.node);
    }
    return true;
}

}  // namespace LoaderProperty
