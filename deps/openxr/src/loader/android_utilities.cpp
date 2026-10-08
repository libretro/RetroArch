// Copyright (c) 2020-2026 The Khronos Group Inc.
// Copyright (c) 2020-2021, Collabora, Ltd.
//
// SPDX-License-Identifier:  Apache-2.0 OR MIT
//
// Initial Author: Rylie Pavlik <rylie.pavlik@collabora.com>
//
// RetroArch: rewritten on raw JNI through libretro-common's rjni layer.
// The android-jni-wrappers and jnipp dependencies are gone, and with
// them the only exception-throwing code in the loader, so this file is
// now safe under -fno-exceptions unconditionally.

#include "android_utilities.h"

#ifdef __ANDROID__
#include "loader_json.hpp"

#include <openxr/openxr.h>

#include <jni/rjni.h>

#include <dlfcn.h>
#include <string>
#include <android/log.h>

#define LOG_TAG "OpenXR-Loader"
#include "android_logging.h"

namespace openxr_android {

// Code in here corresponds roughly to the Java "BrokerContract" class and subclasses.
namespace {
constexpr auto AUTHORITY = "org.khronos.openxr.runtime_broker";
constexpr auto SYSTEM_AUTHORITY = "org.khronos.openxr.system_runtime_broker";
constexpr auto BASE_PATH = "openxr";
constexpr auto ABI_PATH = "abi";
constexpr auto RUNTIMES_PATH = "runtimes";

constexpr const char *getBrokerAuthority(bool systemBroker) { return systemBroker ? SYSTEM_AUTHORITY : AUTHORITY; }
constexpr const char *getBrokerTypeName(bool systemBroker) { return systemBroker ? "system" : "installable"; }

namespace active_runtime {
/// Final path component of /openxr/[major_ver]/abi/[abi]/runtimes/active,
/// a "table" of at most one row: the currently active runtime.
constexpr auto TABLE_PATH = "active";
namespace Columns {
constexpr auto PACKAGE_NAME = "package_name";
constexpr auto NATIVE_LIB_DIR = "native_lib_dir";
constexpr auto SO_FILENAME = "so_filename";
constexpr auto HAS_FUNCTIONS = "has_functions";
}  // namespace Columns
}  // namespace active_runtime

namespace functions {
/// Final path component of the per-package function-remapping table,
/// /openxr/[major_ver]/abi/[abi]/runtimes/[package]/functions.
constexpr auto TABLE_PATH = "functions";
namespace Columns {
constexpr auto FUNCTION_NAME = "function_name";
constexpr auto SYMBOL_NAME = "symbol_name";
}  // namespace Columns
}  // namespace functions

#if defined(__arm__)
constexpr auto ABI = "armeabi-v7l";
#elif defined(__aarch64__)
constexpr auto ABI = "arm64-v8a";
#elif defined(__i386__)
constexpr auto ABI = "x86";
#elif defined(__x86_64__)
constexpr auto ABI = "x86_64";
#else
#error "Unknown ABI!"
#endif

// rjni once-caches: every class global reference and method ID below is
// resolved exactly once per process, lock-free.
rjni_ref cls_uri_builder, cls_uri, cls_context, cls_resolver, cls_cursor, cls_string;
rjni_ref mid_builder_init, mid_scheme, mid_authority, mid_append_path, mid_build, mid_uri_to_string;
rjni_ref mid_get_resolver, mid_query;
rjni_ref mid_count, mid_move_first, mid_move_next, mid_col_index, mid_get_string, mid_get_int, mid_close;

struct BrokerIds {
    jclass uri_builder, uri, context, resolver, cursor, string;
    jmethodID builder_init, scheme, authority, append_path, build, uri_to_string;
    jmethodID get_resolver, query;
    jmethodID count, move_first, move_next, col_index, get_string, get_int, close;
};

bool resolveBrokerIds(JNIEnv *env, BrokerIds &ids) {
    if (!(ids.uri_builder = rjni_class(&cls_uri_builder, env, "android/net/Uri$Builder"))) return false;
    if (!(ids.uri = rjni_class(&cls_uri, env, "android/net/Uri"))) return false;
    if (!(ids.context = rjni_class(&cls_context, env, "android/content/Context"))) return false;
    if (!(ids.resolver = rjni_class(&cls_resolver, env, "android/content/ContentResolver"))) return false;
    if (!(ids.cursor = rjni_class(&cls_cursor, env, "android/database/Cursor"))) return false;
    if (!(ids.string = rjni_class(&cls_string, env, "java/lang/String"))) return false;

    ids.builder_init = rjni_method(&mid_builder_init, env, ids.uri_builder, "<init>", "()V");
    ids.scheme = rjni_method(&mid_scheme, env, ids.uri_builder, "scheme", "(Ljava/lang/String;)Landroid/net/Uri$Builder;");
    ids.authority = rjni_method(&mid_authority, env, ids.uri_builder, "authority", "(Ljava/lang/String;)Landroid/net/Uri$Builder;");
    ids.append_path =
        rjni_method(&mid_append_path, env, ids.uri_builder, "appendPath", "(Ljava/lang/String;)Landroid/net/Uri$Builder;");
    ids.build = rjni_method(&mid_build, env, ids.uri_builder, "build", "()Landroid/net/Uri;");
    ids.uri_to_string = rjni_method(&mid_uri_to_string, env, ids.uri, "toString", "()Ljava/lang/String;");
    ids.get_resolver =
        rjni_method(&mid_get_resolver, env, ids.context, "getContentResolver", "()Landroid/content/ContentResolver;");
    ids.query = rjni_method(&mid_query, env, ids.resolver, "query",
                            "(Landroid/net/Uri;[Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;)"
                            "Landroid/database/Cursor;");
    ids.count = rjni_method(&mid_count, env, ids.cursor, "getCount", "()I");
    ids.move_first = rjni_method(&mid_move_first, env, ids.cursor, "moveToFirst", "()Z");
    ids.move_next = rjni_method(&mid_move_next, env, ids.cursor, "moveToNext", "()Z");
    ids.col_index = rjni_method(&mid_col_index, env, ids.cursor, "getColumnIndex", "(Ljava/lang/String;)I");
    ids.get_string = rjni_method(&mid_get_string, env, ids.cursor, "getString", "(I)Ljava/lang/String;");
    ids.get_int = rjni_method(&mid_get_int, env, ids.cursor, "getInt", "(I)I");
    ids.close = rjni_method(&mid_close, env, ids.cursor, "close", "()V");

    return ids.builder_init && ids.scheme && ids.authority && ids.append_path && ids.build && ids.uri_to_string &&
           ids.get_resolver && ids.query && ids.count && ids.move_first && ids.move_next && ids.col_index && ids.get_string &&
           ids.get_int && ids.close;
}

std::string toStdString(JNIEnv *env, jstring str) {
    std::string out;
    if (!str) return out;
    if (const char *chars = env->GetStringUTFChars(str, nullptr)) {
        out = chars;
        env->ReleaseStringUTFChars(str, chars);
    }
    return out;
}

/// builder = builder.<method>(string); drops the returned duplicate
/// local reference (Uri.Builder returns this).
bool builderCallString(JNIEnv *env, jobject builder, jmethodID method, const char *arg) {
    jstring jarg = env->NewStringUTF(arg);
    if (!jarg) {
        rjni_exception_clear(env);
        return false;
    }
    jobject ret = env->CallObjectMethod(builder, method, jarg);
    env->DeleteLocalRef(jarg);
    if (rjni_exception_clear(env)) return false;
    if (ret) env->DeleteLocalRef(ret);
    return true;
}

/// Build the broker content URI
/// /openxr/[major_ver]/abi/[abi]/runtimes[/package]/[table]; @packageName
/// may be null. Returns a local reference or null.
jobject makeBrokerUri(JNIEnv *env, const BrokerIds &ids, bool systemBroker, int majorVersion, const char *packageName,
                      const char *tablePath) {
    jobject uri = nullptr;
    const std::string major = std::to_string(majorVersion);
    jobject builder = env->NewObject(ids.uri_builder, ids.builder_init);
    if (rjni_exception_clear(env) || !builder) return nullptr;

    if (builderCallString(env, builder, ids.scheme, "content") &&
        builderCallString(env, builder, ids.authority, getBrokerAuthority(systemBroker)) &&
        builderCallString(env, builder, ids.append_path, BASE_PATH) &&
        builderCallString(env, builder, ids.append_path, major.c_str()) &&
        builderCallString(env, builder, ids.append_path, ABI_PATH) && builderCallString(env, builder, ids.append_path, ABI) &&
        builderCallString(env, builder, ids.append_path, RUNTIMES_PATH) &&
        (!packageName || builderCallString(env, builder, ids.append_path, packageName)) &&
        builderCallString(env, builder, ids.append_path, tablePath)) {
        uri = env->CallObjectMethod(builder, ids.build);
        if (rjni_exception_clear(env)) uri = nullptr;
    }
    env->DeleteLocalRef(builder);
    return uri;
}

std::string uriToString(JNIEnv *env, const BrokerIds &ids, jobject uri) {
    jstring jstr = (jstring)env->CallObjectMethod(uri, ids.uri_to_string);
    if (rjni_exception_clear(env)) return std::string();
    std::string out = toStdString(env, jstr);
    if (jstr) env->DeleteLocalRef(jstr);
    return out;
}

/// A new String[] local reference holding @n UTF strings, or null.
jobjectArray makeProjection(JNIEnv *env, const BrokerIds &ids, const char *const *names, int n) {
    jobjectArray arr = env->NewObjectArray(n, ids.string, nullptr);
    if (rjni_exception_clear(env) || !arr) return nullptr;
    for (int i = 0; i < n; ++i) {
        jstring s = env->NewStringUTF(names[i]);
        if (!s) {
            rjni_exception_clear(env);
            env->DeleteLocalRef(arr);
            return nullptr;
        }
        env->SetObjectArrayElement(arr, i, s);
        env->DeleteLocalRef(s);
    }
    return arr;
}

jint getColumnIndex(JNIEnv *env, const BrokerIds &ids, jobject cursor, const char *name) {
    jstring jname = env->NewStringUTF(name);
    if (!jname) {
        rjni_exception_clear(env);
        return -1;
    }
    jint idx = env->CallIntMethod(cursor, ids.col_index, jname);
    env->DeleteLocalRef(jname);
    if (rjni_exception_clear(env)) return -1;
    return idx;
}

std::string cursorString(JNIEnv *env, const BrokerIds &ids, jobject cursor, jint column) {
    jstring jstr = (jstring)env->CallObjectMethod(cursor, ids.get_string, column);
    if (rjni_exception_clear(env)) return std::string();
    std::string out = toStdString(env, jstr);
    if (jstr) env->DeleteLocalRef(jstr);
    return out;
}

void closeCursor(JNIEnv *env, const BrokerIds &ids, jobject cursor) {
    env->CallVoidMethod(cursor, ids.close);
    rjni_exception_clear(env);
    env->DeleteLocalRef(cursor);
}

/// Generic content resolver query function
bool getCursor(JNIEnv *env, const BrokerIds &ids, jobject context, jobjectArray projection, jobject uri, bool systemBroker,
               const char *contentDesc, jobject &out_cursor) {
    out_cursor = nullptr;
    ALOGI("getCursor: Querying URI: %s", uriToString(env, ids, uri).c_str());

    jobject resolver = env->CallObjectMethod(context, ids.get_resolver);
    if (rjni_exception_clear(env) || !resolver) {
        ALOGI("Exception when querying %s content resolver for %s.", getBrokerTypeName(systemBroker), contentDesc);
        return false;
    }
    jobject cursor =
        env->CallObjectMethod(resolver, ids.query, uri, projection, (jobject) nullptr, (jobject) nullptr, (jobject) nullptr);
    env->DeleteLocalRef(resolver);
    if (rjni_exception_clear(env)) {
        ALOGI("Exception when querying %s content resolver for %s.", getBrokerTypeName(systemBroker), contentDesc);
        if (cursor) env->DeleteLocalRef(cursor);
        return false;
    }
    if (!cursor) {
        ALOGI("Null cursor when querying %s content resolver for %s.", getBrokerTypeName(systemBroker), contentDesc);
        return false;
    }
    jint count = env->CallIntMethod(cursor, ids.count);
    if (rjni_exception_clear(env) || count < 1) {
        ALOGI("Non-null but empty cursor when querying %s content resolver for %s.", getBrokerTypeName(systemBroker), contentDesc);
        closeCursor(env, ids, cursor);
        return false;
    }
    out_cursor = cursor;
    return true;
}

Json::Value makeMinimumVirtualRuntimeManifest(const std::string &libraryPath) {
    Json::Value root_node(Json::objectValue);

    root_node["file_format_version"] = "1.0.0";
    root_node["instance_extensions"] = Json::Value(Json::arrayValue);
    root_node["functions"] = Json::Value(Json::objectValue);
    root_node["runtime"] = Json::objectValue;
    root_node["runtime"]["library_path"] = libraryPath;
    return root_node;
}

int populateRuntimeFunctions(JNIEnv *env, const BrokerIds &ids, jobject context, bool systemBroker,
                             const std::string &packageName, Json::Value &manifest) {
    const char *const names[] = {functions::Columns::FUNCTION_NAME, functions::Columns::SYMBOL_NAME};
    jobjectArray projection = makeProjection(env, ids, names, 2);
    if (!projection) return -1;

    jobject uri = makeBrokerUri(env, ids, systemBroker, XR_VERSION_MAJOR(XR_CURRENT_API_VERSION), packageName.c_str(),
                                functions::TABLE_PATH);
    if (!uri) {
        env->DeleteLocalRef(projection);
        return -1;
    }
    ALOGI("populateFunctions: Querying URI: %s", uriToString(env, ids, uri).c_str());

    jobject cursor = nullptr;
    bool have = getCursor(env, ids, context, projection, uri, systemBroker, "functions", cursor);
    env->DeleteLocalRef(uri);
    env->DeleteLocalRef(projection);
    if (!have) return -1;

    jint functionIndex = getColumnIndex(env, ids, cursor, functions::Columns::FUNCTION_NAME);
    jint symbolIndex = getColumnIndex(env, ids, cursor, functions::Columns::SYMBOL_NAME);
    while (env->CallBooleanMethod(cursor, ids.move_next) && !rjni_exception_clear(env)) {
        manifest["functions"][cursorString(env, ids, cursor, functionIndex)] = cursorString(env, ids, cursor, symbolIndex);
    }

    closeCursor(env, ids, cursor);
    return 0;
}

/// Get cursor for active runtime, parameterized by whether or not we use the system broker
bool getActiveRuntimeCursor(JNIEnv *env, const BrokerIds &ids, jobject context, jobjectArray projection, bool systemBroker,
                            jobject &cursor) {
    jobject uri = makeBrokerUri(env, ids, systemBroker, XR_VERSION_MAJOR(XR_CURRENT_API_VERSION), nullptr,
                                active_runtime::TABLE_PATH);
    if (!uri) {
        cursor = nullptr;
        return false;
    }
    ALOGI("getActiveRuntimeCursor: Querying URI: %s", uriToString(env, ids, uri).c_str());
    bool ret = getCursor(env, ids, context, projection, uri, systemBroker, "active runtime", cursor);
    env->DeleteLocalRef(uri);
    return ret;
}
}  // namespace

int getActiveRuntimeVirtualManifest(jobject context, Json::Value &virtualManifest) {
    JNIEnv *env = rjni_env();
    BrokerIds ids;

    if (!env) {
        ALOGW("getActiveRuntimeVirtualManifest: no JNIEnv for this thread.");
        return -1;
    }
    if (!resolveBrokerIds(env, ids)) {
        ALOGW("getActiveRuntimeVirtualManifest: could not resolve broker classes.");
        return -1;
    }

    const char *const names[] = {active_runtime::Columns::PACKAGE_NAME, active_runtime::Columns::NATIVE_LIB_DIR,
                                 active_runtime::Columns::SO_FILENAME, active_runtime::Columns::HAS_FUNCTIONS};
    jobjectArray projection = makeProjection(env, ids, names, 4);
    if (!projection) return -1;

    // First, try getting the installable broker's provider
    bool systemBroker = false;
    jobject cursor = nullptr;
    if (!getActiveRuntimeCursor(env, ids, context, projection, systemBroker, cursor)) {
        // OK, try the system broker as a fallback.
        systemBroker = true;
        getActiveRuntimeCursor(env, ids, context, projection, systemBroker, cursor);
    }
    env->DeleteLocalRef(projection);

    if (!cursor) {
        // Couldn't find either broker
        ALOGI("Could access neither the installable nor system runtime broker.");
        return -1;
    }

    env->CallBooleanMethod(cursor, ids.move_first);
    if (rjni_exception_clear(env)) {
        closeCursor(env, ids, cursor);
        return -1;
    }

    do {
        std::string filename = cursorString(env, ids, cursor, getColumnIndex(env, ids, cursor, active_runtime::Columns::SO_FILENAME));
        std::string libDir = cursorString(env, ids, cursor, getColumnIndex(env, ids, cursor, active_runtime::Columns::NATIVE_LIB_DIR));
        std::string packageName =
            cursorString(env, ids, cursor, getColumnIndex(env, ids, cursor, active_runtime::Columns::PACKAGE_NAME));

        bool hasFunctions =
            env->CallIntMethod(cursor, ids.get_int, getColumnIndex(env, ids, cursor, active_runtime::Columns::HAS_FUNCTIONS)) == 1;
        if (rjni_exception_clear(env)) break;
        ALOGI("Got runtime: package: %s, so filename: %s, native lib dir: %s, has functions: %s", packageName.c_str(),
              filename.c_str(), libDir.c_str(), (hasFunctions ? "yes" : "no"));

        std::string lib_path = libDir + "/" + filename;
        void *lib = dlopen(lib_path.c_str(), RTLD_LAZY | RTLD_LOCAL);
        if (lib) {
            // we found a runtime that we can dlopen, use it.
            dlclose(lib);

            Json::Value manifest = makeMinimumVirtualRuntimeManifest(lib_path);
            if (hasFunctions) {
                int result = populateRuntimeFunctions(env, ids, context, systemBroker, packageName, manifest);
                if (result != 0) {
                    ALOGW("Unable to populate functions from runtime: %s, checking for more records...", lib_path.c_str());
                    continue;
                }
            }
            virtualManifest = manifest;
            closeCursor(env, ids, cursor);
            return 0;
        }
        // this runtime was not accessible, see if the broker has more runtimes on
        // offer.
        ALOGV("Unable to open broker provided runtime at %s, checking for more records...", lib_path.c_str());
    } while (env->CallBooleanMethod(cursor, ids.move_next) && !rjni_exception_clear(env));

    ALOGW("Unable to open any of the broker provided runtimes.");
    closeCursor(env, ids, cursor);
    return -1;
}
}  // namespace openxr_android

#endif  // __ANDROID__
