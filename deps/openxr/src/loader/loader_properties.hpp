// Copyright (c) 2017-2026 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT
//

#pragma once

#include <openxr/openxr.h>

#include <string>

// Exposes a centralized way to read properties which may be passed to the loader through xrInitializeLoaderKHR or available through
// environment variables.
namespace LoaderProperty {
std::string Get(const std::string& name);
std::string GetSecure(const std::string& name);
bool IsSet(const std::string& name);
// Replaces every override with the given set, in one step: a reader sees
// the old set or the new one, never a set half written.
bool SetOverrides(const XrLoaderInitPropertyValueEXT* values, uint32_t count);
}  // namespace LoaderProperty
