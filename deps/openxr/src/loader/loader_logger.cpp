// Copyright (c) 2017-2026 The Khronos Group Inc.
// Copyright (c) 2017-2019 Valve Corporation
// Copyright (c) 2017-2019 LunarG, Inc.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// Initial Author: Mark Young <marky@lunarg.com>
//

#include "loader_logger.hpp"

#include "extra_algorithms.h"
#include "loader_logger_recorders.hpp"
#include "loader_properties.hpp"
#include "platform_utils.hpp"

#include <openxr/openxr.h>

#include <algorithm>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// For routing platform_utils.hpp messages into the LoaderLogger.
void LogPlatformUtilsError(const std::string& message) { LoaderLogger::LogErrorMessage("platform_utils", message); }

bool LoaderLogRecorder::LogDebugUtilsMessage(XrDebugUtilsMessageSeverityFlagsEXT /*message_severity*/,
                                             XrDebugUtilsMessageTypeFlagsEXT /*message_type*/,
                                             const XrDebugUtilsMessengerCallbackDataEXT* /*callback_data*/) {
    return false;
}

// Utility functions for converting to/from XR_EXT_debug_utils values

XrLoaderLogMessageSeverityFlags DebugUtilsSeveritiesToLoaderLogMessageSeverities(
    XrDebugUtilsMessageSeverityFlagsEXT utils_severities) {
    XrLoaderLogMessageSeverityFlags log_severities = 0UL;
    if ((utils_severities & XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) != 0u) {
        log_severities |= XR_LOADER_LOG_MESSAGE_SEVERITY_VERBOSE_BIT;
    }
    if ((utils_severities & XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) != 0u) {
        log_severities |= XR_LOADER_LOG_MESSAGE_SEVERITY_INFO_BIT;
    }
    if ((utils_severities & XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0u) {
        log_severities |= XR_LOADER_LOG_MESSAGE_SEVERITY_WARNING_BIT;
    }
    if ((utils_severities & XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0u) {
        log_severities |= XR_LOADER_LOG_MESSAGE_SEVERITY_ERROR_BIT;
    }
    return log_severities;
}

XrDebugUtilsMessageSeverityFlagsEXT LoaderLogMessageSeveritiesToDebugUtilsMessageSeverities(
    XrLoaderLogMessageSeverityFlags log_severities) {
    XrDebugUtilsMessageSeverityFlagsEXT utils_severities = 0UL;
    if ((log_severities & XR_LOADER_LOG_MESSAGE_SEVERITY_VERBOSE_BIT) != 0u) {
        utils_severities |= XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
    }
    if ((log_severities & XR_LOADER_LOG_MESSAGE_SEVERITY_INFO_BIT) != 0u) {
        utils_severities |= XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
    }
    if ((log_severities & XR_LOADER_LOG_MESSAGE_SEVERITY_WARNING_BIT) != 0u) {
        utils_severities |= XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    }
    if ((log_severities & XR_LOADER_LOG_MESSAGE_SEVERITY_ERROR_BIT) != 0u) {
        utils_severities |= XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    }
    return utils_severities;
}

XrLoaderLogMessageTypeFlagBits DebugUtilsMessageTypesToLoaderLogMessageTypes(XrDebugUtilsMessageTypeFlagsEXT utils_types) {
    XrLoaderLogMessageTypeFlagBits log_types = 0UL;
    if ((utils_types & XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) != 0u) {
        log_types |= XR_LOADER_LOG_MESSAGE_TYPE_GENERAL_BIT;
    }
    if ((utils_types & XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) != 0u) {
        log_types |= XR_LOADER_LOG_MESSAGE_TYPE_SPECIFICATION_BIT;
    }
    if ((utils_types & XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) != 0u) {
        log_types |= XR_LOADER_LOG_MESSAGE_TYPE_PERFORMANCE_BIT;
    }
    return log_types;
}

XrDebugUtilsMessageTypeFlagsEXT LoaderLogMessageTypesToDebugUtilsMessageTypes(XrLoaderLogMessageTypeFlagBits log_types) {
    XrDebugUtilsMessageTypeFlagsEXT utils_types = 0UL;
    if ((log_types & XR_LOADER_LOG_MESSAGE_TYPE_GENERAL_BIT) != 0u) {
        utils_types |= XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
    }
    if ((log_types & XR_LOADER_LOG_MESSAGE_TYPE_SPECIFICATION_BIT) != 0u) {
        utils_types |= XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    }
    if ((log_types & XR_LOADER_LOG_MESSAGE_TYPE_PERFORMANCE_BIT) != 0u) {
        utils_types |= XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    }
    return utils_types;
}

/* One recorder table entry: the recorder, and the instance it was made
 * for (XR_NULL_HANDLE for the loader's own). */
typedef struct loader_log_entry {
    LoaderLogRecorder* recorder;
    XrInstance instance;
} loader_log_entry_t;

/* A removed recorder, kept alive until the logger goes. */
typedef struct loader_log_retired {
    mpsc_stack_node_t node;
    LoaderLogRecorder* recorder;
} loader_log_retired_t;

enum { LOADER_LOG_DROP_NONE = 0, LOADER_LOG_DROP_ID, LOADER_LOG_DROP_INSTANCE };

static bool loader_log_drops(const loader_log_entry_t* entry, int drop, uint64_t drop_id, XrInstance drop_instance) {
    if (drop == LOADER_LOG_DROP_ID) {
        return entry->recorder->UniqueId() == drop_id;
    }
    if (drop == LOADER_LOG_DROP_INSTANCE) {
        return entry->instance != XR_NULL_HANDLE && entry->instance == drop_instance;
    }
    return false;
}

LoaderLogger::LoaderLogger() {
    retro_atomic_ptr_init(&_snapshot, NULL);
    mpsc_stack_init(&_retired_snapshots);
    mpsc_stack_init(&_retired_recorders);

    std::string debug_string = LoaderProperty::Get("XR_LOADER_DEBUG");

    // Add an error logger by default so that we at least get errors out to std::cerr.
    // Normally we enable stderr output. But if the XR_LOADER_DEBUG environment variable is
    // present as "none" then we don't.
    if (debug_string != "none") {
        AddLogRecorder(MakeStdErrLoaderLogRecorder(nullptr));
#ifdef __ANDROID__
        // Add a logcat logger by default.
        AddLogRecorder(MakeLogcatLoaderLogRecorder());
#endif  // __ANDROID__
    }

#ifdef _WIN32
    // Add an debugger logger by default so that we at least get errors out to the debugger.
    AddLogRecorder(MakeDebuggerLoaderLogRecorder(nullptr));
#endif

    // If the environment variable to enable loader debugging is set, then enable the
    // appropriate logging out to std::cout.
    if (!debug_string.empty()) {
        XrLoaderLogMessageSeverityFlags debug_flags = {};
        if (debug_string == "error") {
            debug_flags = XR_LOADER_LOG_MESSAGE_SEVERITY_ERROR_BIT;
        } else if (debug_string == "warn") {
            debug_flags = XR_LOADER_LOG_MESSAGE_SEVERITY_ERROR_BIT | XR_LOADER_LOG_MESSAGE_SEVERITY_WARNING_BIT;
        } else if (debug_string == "info") {
            debug_flags = XR_LOADER_LOG_MESSAGE_SEVERITY_ERROR_BIT | XR_LOADER_LOG_MESSAGE_SEVERITY_WARNING_BIT |
                          XR_LOADER_LOG_MESSAGE_SEVERITY_INFO_BIT;
        } else if (debug_string == "all" || debug_string == "verbose") {
            debug_flags = XR_LOADER_LOG_MESSAGE_SEVERITY_ERROR_BIT | XR_LOADER_LOG_MESSAGE_SEVERITY_WARNING_BIT |
                          XR_LOADER_LOG_MESSAGE_SEVERITY_INFO_BIT | XR_LOADER_LOG_MESSAGE_SEVERITY_VERBOSE_BIT;
        }
        AddLogRecorder(MakeStdOutLoaderLogRecorder(nullptr, debug_flags));
    }
}

LoaderLogger::~LoaderLogger() {
    size_t i;
    mpsc_stack_node_t* node;
    loader_snapshot_t* current = (loader_snapshot_t*)retro_atomic_exchange_ptr(&_snapshot, NULL);
    if (current) {
        loader_log_entry_t* entries = LOADER_SNAPSHOT_ITEMS(current, loader_log_entry_t);
        for (i = 0; i < current->count; i++) {
            delete entries[i].recorder;
        }
        free(current);
    }
    loader_snapshot_free_retired(&_retired_snapshots);
    node = mpsc_stack_drain(&_retired_recorders);
    while (node) {
        loader_log_retired_t* retired = (loader_log_retired_t*)node;
        node = node->next;
        delete retired->recorder;
        free(retired);
    }
}

// The current table less the entries drop selects, plus add when given,
// published in one compare-exchange.
void LoaderLogger::Publish(LoaderLogRecorder* add, XrInstance add_instance, int drop, uint64_t drop_id,
                           XrInstance drop_instance) {
    for (;;) {
        size_t i;
        size_t dropped = 0;
        size_t count = 0;
        const loader_log_entry_t* entries = NULL;
        loader_log_entry_t* next_entries;
        loader_snapshot_t* next;
        loader_snapshot_t* current = (loader_snapshot_t*)retro_atomic_load_acquire_ptr(&_snapshot);

        if (current) {
            count = current->count;
            entries = LOADER_SNAPSHOT_ITEMS(current, const loader_log_entry_t);
        }
        for (i = 0; i < count; i++) {
            if (loader_log_drops(&entries[i], drop, drop_id, drop_instance)) {
                dropped++;
            }
        }
        if (!add && !dropped) {
            return;
        }
        if (!(next = loader_snapshot_new(count - dropped + (add ? 1 : 0), sizeof(loader_log_entry_t), 0))) {
            delete add;
            return;
        }
        next_entries = LOADER_SNAPSHOT_ITEMS(next, loader_log_entry_t);
        for (i = 0; i < count; i++) {
            if (!loader_log_drops(&entries[i], drop, drop_id, drop_instance)) {
                *next_entries++ = entries[i];
            }
        }
        if (add) {
            next_entries->recorder = add;
            next_entries->instance = add_instance;
        }

        if (retro_atomic_cas_ptr(&_snapshot, current, next)) {
            if (current) {
                for (i = 0; i < count; i++) {
                    loader_log_retired_t* retired;
                    if (!loader_log_drops(&entries[i], drop, drop_id, drop_instance)) {
                        continue;
                    }
                    /* Out of memory, the recorder is leaked rather than freed under a reader. */
                    if ((retired = (loader_log_retired_t*)malloc(sizeof(*retired)))) {
                        retired->recorder = entries[i].recorder;
                        mpsc_stack_push(&_retired_recorders, &retired->node);
                    }
                }
                mpsc_stack_push(&_retired_snapshots, &current->retire.node);
            }
            return;
        }
        free(next);
    }
}

void LoaderLogger::AddLogRecorder(std::unique_ptr<LoaderLogRecorder>&& recorder) {
    Publish(recorder.release(), XR_NULL_HANDLE, LOADER_LOG_DROP_NONE, 0, XR_NULL_HANDLE);
}

void LoaderLogger::AddLogRecorderForXrInstance(XrInstance instance, std::unique_ptr<LoaderLogRecorder>&& recorder) {
    Publish(recorder.release(), instance, LOADER_LOG_DROP_NONE, 0, XR_NULL_HANDLE);
}

void LoaderLogger::RemoveLogRecorder(uint64_t unique_id) {
    Publish(NULL, XR_NULL_HANDLE, LOADER_LOG_DROP_ID, unique_id, XR_NULL_HANDLE);
}

void LoaderLogger::RemoveLogRecordersForXrInstance(XrInstance instance) {
    Publish(NULL, XR_NULL_HANDLE, LOADER_LOG_DROP_INSTANCE, 0, instance);
}

bool LoaderLogger::LogMessage(XrLoaderLogMessageSeverityFlagBits message_severity, XrLoaderLogMessageTypeFlags message_type,
                              const std::string& message_id, const std::string& command_name, const std::string& message,
                              const std::vector<XrSdkLogObjectInfo>& objects) {
    XrLoaderLogMessengerCallbackData callback_data = {};
    callback_data.message_id = message_id.c_str();
    callback_data.command_name = command_name.c_str();
    callback_data.message = message.c_str();

    auto names_and_labels = data_.PopulateNamesAndLabels(objects);
    callback_data.objects = names_and_labels.sdk_objects.empty() ? nullptr : names_and_labels.sdk_objects.data();
    callback_data.object_count = static_cast<uint8_t>(names_and_labels.objects.size());

    callback_data.session_labels = names_and_labels.labels.empty() ? nullptr : names_and_labels.labels.data();
    callback_data.session_labels_count = static_cast<uint8_t>(names_and_labels.labels.size());

    bool exit_app = false;
    const loader_snapshot_t* snapshot = (const loader_snapshot_t*)retro_atomic_load_acquire_ptr(&_snapshot);
    size_t snapshot_count = snapshot ? snapshot->count : 0;
    const loader_log_entry_t* entries = snapshot ? LOADER_SNAPSHOT_ITEMS(snapshot, const loader_log_entry_t) : NULL;
    for (size_t i = 0; i < snapshot_count; ++i) {
        LoaderLogRecorder* recorder = entries[i].recorder;
        if ((recorder->MessageSeverities() & message_severity) == message_severity &&
            (recorder->MessageTypes() & message_type) == message_type) {
            exit_app |= recorder->LogMessage(message_severity, message_type, &callback_data);
        }
    }
    return exit_app;
}

// Extension-specific logging functions
bool LoaderLogger::LogDebugUtilsMessage(XrDebugUtilsMessageSeverityFlagsEXT message_severity,
                                        XrDebugUtilsMessageTypeFlagsEXT message_type,
                                        const XrDebugUtilsMessengerCallbackDataEXT* callback_data) {
    bool exit_app = false;
    XrLoaderLogMessageSeverityFlags log_message_severity = DebugUtilsSeveritiesToLoaderLogMessageSeverities(message_severity);
    XrLoaderLogMessageTypeFlags log_message_type = DebugUtilsMessageTypesToLoaderLogMessageTypes(message_type);

    AugmentedCallbackData augmented_data;
    data_.WrapCallbackData(&augmented_data, callback_data);

    // Loop through the recorders
    const loader_snapshot_t* snapshot = (const loader_snapshot_t*)retro_atomic_load_acquire_ptr(&_snapshot);
    size_t snapshot_count = snapshot ? snapshot->count : 0;
    const loader_log_entry_t* entries = snapshot ? LOADER_SNAPSHOT_ITEMS(snapshot, const loader_log_entry_t) : NULL;
    for (size_t i = 0; i < snapshot_count; ++i) {
        LoaderLogRecorder* recorder = entries[i].recorder;
        // Only send the message if it's a debug utils recorder and of the type the recorder cares about.
        if (recorder->Type() != XR_LOADER_LOG_DEBUG_UTILS ||
            (recorder->MessageSeverities() & log_message_severity) != log_message_severity ||
            (recorder->MessageTypes() & log_message_type) != log_message_type) {
            continue;
        }

        exit_app |= recorder->LogDebugUtilsMessage(message_severity, message_type, augmented_data.exported_data);
    }
    return exit_app;
}

void LoaderLogger::AddObjectName(uint64_t object_handle, XrObjectType object_type, const std::string& object_name) {
    data_.AddObjectName(object_handle, object_type, object_name);
}

void LoaderLogger::BeginLabelRegion(XrSession session, const XrDebugUtilsLabelEXT* label_info) {
    data_.BeginLabelRegion(session, *label_info);
}

void LoaderLogger::EndLabelRegion(XrSession session) { data_.EndLabelRegion(session); }

void LoaderLogger::InsertLabel(XrSession session, const XrDebugUtilsLabelEXT* label_info) {
    data_.InsertLabel(session, *label_info);
}

void LoaderLogger::DeleteSessionLabels(XrSession session) { data_.DeleteSessionLabels(session); }
