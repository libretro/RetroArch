/* Hermetic compile-only stand-in for the NDK header. */
#ifndef STUB_SYS_SYSTEM_PROPERTIES_H
#define STUB_SYS_SYSTEM_PROPERTIES_H
#define PROP_NAME_MAX 32
#define PROP_VALUE_MAX 92
int __system_property_get(const char *name, char *value);
#endif
