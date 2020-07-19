#ifndef _PSP2_SHACCCG_H
#define _PSP2_SHACCCG_H

#ifdef	__cplusplus
extern "C" {
#endif	// def __cplusplus

typedef struct SceShaccCgCompileOptions SceShaccCgCompileOptions;
typedef struct SceShaccCgSourceFile SceShaccCgSourceFile;
typedef struct SceShaccCgSourceLocation SceShaccCgSourceLocation;
typedef void const *SceShaccCgParameter;

typedef SceShaccCgSourceFile* (*SceShaccCgCallbackOpenFile)(
	const char *fileName,
	const SceShaccCgSourceLocation *includedFrom,
	const SceShaccCgCompileOptions *compileOptions,
	const char **errorString);

typedef void (*SceShaccCgCallbackReleaseFile)(
	const SceShaccCgSourceFile *file,
	const SceShaccCgCompileOptions *compileOptions);

typedef const char* (*SceShaccCgCallbackLocateFile)(
	const char *fileName,
	const SceShaccCgSourceLocation *includedFrom,
	uint32_t searchPathCount,
	const char *const*searchPaths,
	const SceShaccCgCompileOptions *compileOptions,
	const char **errorString);

typedef const char* (*SceShaccCgCallbackAbsolutePath)(
	const char *fileName,
	const SceShaccCgSourceLocation *includedFrom,
	const SceShaccCgCompileOptions *compileOptions);

typedef void (*SceShaccCgCallbackReleaseFileName)(
	const char *fileName,
	const SceShaccCgCompileOptions *compileOptions);

typedef int32_t (*SceShaccCgCallbackFileDate)(
	const SceShaccCgSourceFile *file,
	const SceShaccCgSourceLocation *includedFrom,
	const SceShaccCgCompileOptions *compileOptions,
	int64_t *timeLastStatusChange,
	int64_t *timeLastModified);

typedef enum SceShaccCgDiagnosticLevel {
	SCE_SHACCCG_DIAGNOSTIC_LEVEL_INFO,
	SCE_SHACCCG_DIAGNOSTIC_LEVEL_WARNING,
	SCE_SHACCCG_DIAGNOSTIC_LEVEL_ERROR
} SceShaccCgDiagnosticLevel;

typedef enum SceShaccCgTargetProfile {
	SCE_SHACCCG_PROFILE_VP,
	SCE_SHACCCG_PROFILE_FP
} SceShaccCgTargetProfile;

typedef enum SceShaccCgCallbackDefaults {
	SCE_SHACCCG_SYSTEM_FILES,
	SCE_SHACCCG_TRIVIAL
} SceShaccCgCallbackDefaults;

typedef enum SceShaccCgLocale {
	SCE_SHACCCG_ENGLISH,
	SCE_SHACCCG_JAPANESE
} SceShaccCgLocale;

typedef struct SceShaccCgSourceFile {
	const char *fileName;
	const char *text;
	uint32_t size;
} SceShaccCgSourceFile;

typedef struct SceShaccCgSourceLocation {
	const SceShaccCgSourceFile *file;
	uint32_t lineNumber;
	uint32_t columnNumber;
} SceShaccCgSourceLocation;

typedef struct SceShaccCgCallbackList {
	SceShaccCgCallbackOpenFile openFile;
	SceShaccCgCallbackReleaseFile releaseFile;
	SceShaccCgCallbackLocateFile locateFile;
	SceShaccCgCallbackAbsolutePath absolutePath;
	SceShaccCgCallbackReleaseFileName releaseFileName;
	SceShaccCgCallbackFileDate fileDate;
} SceShaccCgCallbackList;

typedef struct SceShaccCgCompileOptions {
	const char *mainSourceFile;
	SceShaccCgTargetProfile targetProfile;
	const char *entryFunctionName;
	uint32_t searchPathCount;
	const char* const *searchPaths;
	uint32_t macroDefinitionCount;
	const char* const *macroDefinitions;
	uint32_t includeFileCount;
	const char* const *includeFiles;
	uint32_t suppressedWarningsCount;
	const uint32_t *suppressedWarnings;
	SceShaccCgLocale locale;
	int32_t useFx;
	int32_t noStdlib;
	int32_t optimizationLevel;
	int32_t useFastmath;
	int32_t useFastprecision;
	int32_t useFastint;
	int32_t warningsAsErrors;
	int32_t performanceWarnings;
	int32_t warningLevel;
	int32_t pedantic;
	int32_t pedanticError;
	int field_5C;
	int field_60;
	int field_64;
} SceShaccCgCompileOptions;

typedef struct SceShaccCgDiagnosticMessage {
	SceShaccCgDiagnosticLevel level;
	uint32_t code;
	const SceShaccCgSourceLocation *location;
	const char *message;
} SceShaccCgDiagnosticMessage;

typedef struct SceShaccCgCompileOutput {
	const uint8_t *programData;
	uint32_t programSize;
	int32_t diagnosticCount;
	const SceShaccCgDiagnosticMessage *diagnostics;
} SceShaccCgCompileOutput;


int SceShaccCg_0205DE96(int);
int SceShaccCg_07DDFC78(int);
int SceShaccCg_0E1285A6(int);
int SceShaccCg_152971B1(int);
int SceShaccCg_17223BEB(int);
int SceShaccCg_2654E73A(int);
int SceShaccCg_268FAEE9(int);

int sceShaccCgInitializeCompileOptions(
	SceShaccCgCompileOptions *options);

int SceShaccCg_4595A388(int);
int SceShaccCg_46FA0303(int);
int SceShaccCg_56BFA825(int);
int SceShaccCg_648739F3(int);

SceShaccCgCompileOutput const *sceShaccCgCompileProgram(
	const SceShaccCgCompileOptions *options,
	const SceShaccCgCallbackList *callbacks,
	int unk);

int SceShaccCg_6BB58825(int);
int sceShaccCgSetDefaultAllocator(void *(*malloc_cb)(unsigned int), void (*free_cb)(void *));
int SceShaccCg_6FB40CA9(int);
int SceShaccCg_7B2CF324(int);
int SceShaccCg_7BC25091(int);
int SceShaccCg_7F430CCD(int);
int SceShaccCg_95F57A23(int);
int SceShaccCg_A067C481(int);
int SceShaccCg_A13A8A1E(int);
int SceShaccCg_A56B1A5B(int);
int SceShaccCg_A7930FF6(int);

void sceShaccCgInitializeCallbackList(
	SceShaccCgCallbackList *callbacks,
	SceShaccCgCallbackDefaults defaults);

void sceShaccCgDestroyCompileOutput(
	SceShaccCgCompileOutput const *output);

int SceShaccCg_B4AC9943(int);
int SceShaccCg_BB703EE1(int);
int SceShaccCg_D4378DB1(int);
int SceShaccCg_DAD4AAE4(int);
int SceShaccCg_DF3DDCFD(int);
int SceShaccCg_EF8D59D6(int);
int SceShaccCg_F4BAB902(int);

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif /* _PSP2_SHACCCG_H */
