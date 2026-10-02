#!/usr/bin/env python3
# Copyright (C) 2026 buu420
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile the actual RetroArch speech environment case with a fake frontend.

This checks API validation and dispatch, not a full RetroArch build, platform
speech engines, speech completion, audio output, threading, or ABI compatibility.
Production sources are read only; generated files use a temporary directory.
"""

import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


def mask_c_literals(source):
    """Preserve character offsets while hiding comments and string literals."""
    pattern = r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\''
    return re.sub(pattern, lambda m: ''.join('\n' if c == '\n' else ' ' for c in m[0]),
                  source, flags=re.S)


def extract_block(source, pattern, label):
    masked = mask_c_literals(source)
    match = re.search(pattern, masked, flags=re.M)
    if not match:
        raise ValueError('Missing ' + label)
    opening = masked.find('{', match.end())
    if opening < 0:
        raise ValueError('Missing body for ' + label)
    depth = 0
    for index in range(opening, len(masked)):
        if masked[index] == '{':
            depth += 1
        elif masked[index] == '}':
            depth -= 1
            if depth == 0:
                return source[match.start():index + 1]
    raise ValueError('Unbalanced body for ' + label)


PREAMBLE = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <libretro.h>

typedef struct {
   struct { bool accessibility_enable; } bools;
   struct { unsigned accessibility_narrator_speech_speed; } uints;
} settings_t;
typedef struct { bool enabled; } access_state_t;
typedef struct {
   bool (*accessibility_speak)(int, const char *, int);
} frontend_ctx_driver_t;
typedef struct {
   frontend_ctx_driver_t *current_frontend_ctx;
} frontend_state_t;

static settings_t test_settings;
static access_state_t test_access;
static frontend_state_t test_frontend_state;
static unsigned checks;

settings_t *config_get_ptr(void) { return &test_settings; }
access_state_t *access_state_get_ptr(void) { return &test_access; }
frontend_state_t *frontend_state_get_ptr(void) { return &test_frontend_state; }
bool string_is_empty(const char *value) { return !value || !*value; }
void RARCH_DBG(const char *format, ...) { (void)format; }

static void check(bool condition, const char *description)
{
   ++checks;
   if (!condition)
   {
      fprintf(stderr, "FAIL: %s\n", description);
      exit(EXIT_FAILURE);
   }
}
'''

TESTS = r'''
#ifdef HAVE_ACCESSIBILITY
static unsigned calls;
static bool backend_result;
static int received_speed;
static int received_priority;
static char received_text[256];

static bool recording_backend(int speed, const char *text, int priority)
{
   ++calls;
   received_speed = speed;
   received_priority = priority;
   if (strlen(text) >= sizeof(received_text))
      return false;
   strcpy(received_text, text);
   return backend_result;
}

static frontend_ctx_driver_t recording_frontend;

static void reset(void)
{
   memset(&test_settings, 0, sizeof(test_settings));
   memset(&test_access, 0, sizeof(test_access));
   test_settings.bools.accessibility_enable = true;
   test_settings.uints.accessibility_narrator_speech_speed = 7;
   recording_frontend.accessibility_speak = recording_backend;
   test_frontend_state.current_frontend_ctx = &recording_frontend;
   calls = 0;
   backend_result = true;
   received_speed = 0;
   received_priority = 0;
   received_text[0] = '\0';
}

static void expect_rejected(struct retro_accessibility_speech *request,
      const char *description)
{
   check(!test_environment(RETRO_ENVIRONMENT_ACCESSIBILITY_SPEAK, request),
         description);
   check(calls == 0, "rejected request must not reach speech backend");
}

int main(void)
{
   struct retro_accessibility_speech request;
   const char utf8_text[] = "Caf\303\251 \346\227\245\346\234\254 \360\237\216\256";
   reset();
   request.text = "Inventory: key";
   request.priority = 10;
   request.channel = NULL;
   request.flags = 0;

   expect_rejected(NULL, "null request is rejected");
   request.text = NULL;
   expect_rejected(&request, "null text is rejected");
   request.text = "";
   expect_rejected(&request, "empty text is rejected");
   request.text = "Inventory: key";
   request.flags = 1;
   expect_rejected(&request, "nonzero reserved flags are rejected");
   request.flags = UINT_MAX;
   expect_rejected(&request, "all reserved flag bits are rejected");
   request.flags = 0;

   test_settings.bools.accessibility_enable = false;
   expect_rejected(&request, "disabled accessibility is rejected");
   test_access.enabled = true;
   check(test_environment(RETRO_ENVIRONMENT_ACCESSIBILITY_SPEAK, &request),
         "command line override enables speech");
   check(calls == 1, "command line override dispatches once");

   reset();
   test_frontend_state.current_frontend_ctx = NULL;
   expect_rejected(&request, "missing frontend is rejected");
   test_frontend_state.current_frontend_ctx = &recording_frontend;
   recording_frontend.accessibility_speak = NULL;
   expect_rejected(&request, "missing speech hook is rejected");

   reset();
   backend_result = false;
   check(!test_environment(RETRO_ENVIRONMENT_ACCESSIBILITY_SPEAK, &request),
         "backend failure propagates");
   check(calls == 1, "failed backend was called once");

   reset();
   request.text = utf8_text;
   request.priority = 13;
   request.channel = "inventory";
   check(test_environment(RETRO_ENVIRONMENT_ACCESSIBILITY_SPEAK, &request),
         "backend success propagates");
   check(calls == 1, "successful request dispatches once");
   check(strcmp(received_text, utf8_text) == 0, "UTF-8 bytes are forwarded exactly");
   check(received_speed == 7, "configured speech speed is forwarded exactly");
   check(received_priority == 13, "priority is forwarded exactly");

   reset();
   request.text = "-10 <HP> & \"quoted\"";
   request.priority = 0;
   request.channel = NULL;
   check(test_environment(RETRO_ENVIRONMENT_ACCESSIBILITY_SPEAK, &request),
         "null channel is accepted");
   check(strcmp(received_text, request.text) == 0,
         "punctuation reaches the backend unchanged");
   check(received_priority == 0, "zero priority reaches the backend unchanged");

   reset();
   request.channel = "";
   request.priority = INT_MAX;
   test_settings.uints.accessibility_narrator_speech_speed = 3;
   check(test_environment(RETRO_ENVIRONMENT_ACCESSIBILITY_SPEAK, &request),
         "empty channel is accepted");
   check(received_priority == INT_MAX, "large priority reaches the backend unchanged");
   check(received_speed == 3, "changed configured speed is forwarded");

   printf("PASS accessibility-enabled: %u assertions\n", checks);
   return EXIT_SUCCESS;
}
#else
int main(void)
{
   struct retro_accessibility_speech request;
   request.text = "Inventory: key";
   request.priority = 10;
   request.channel = NULL;
   request.flags = 0;
   check(!test_environment(RETRO_ENVIRONMENT_ACCESSIBILITY_SPEAK, &request),
         "feature-disabled build rejects valid speech");
   check(!test_environment(RETRO_ENVIRONMENT_ACCESSIBILITY_SPEAK, NULL),
         "feature-disabled build rejects null request");
   printf("PASS accessibility-disabled: %u assertions\n", checks);
   return EXIT_SUCCESS;
}
#endif
'''


def extract_argv_constructions(function_source, label):
    """Extract declarations, assignments and execvp calls without rewriting them."""
    masked = mask_c_literals(function_source)
    starts = list(re.finditer(r'\bchar\s*\*\s*cmd\s*\[\s*\]\s*=', masked))
    if len(starts) != 2:
        raise ValueError('Expected two argv constructions in ' + label)
    snippets = []
    for start in starts:
        execute = re.search(r'\bexecvp\s*\(', masked[start.end():])
        if not execute:
            raise ValueError('Missing execvp after argv construction in ' + label)
        end = masked.find(';', start.end() + execute.end())
        if end < 0:
            raise ValueError('Missing execvp terminator in ' + label)
        snippets.append(function_source[start.start():end + 1])
    return snippets


ARGV_PREAMBLE = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
static unsigned calls;
static const char *const *expected_argv;
static void check(int condition, const char *description)
{
   ++checks;
   if (!condition)
   {
      fprintf(stderr, "FAIL argv: %s\n", description);
      exit(EXIT_FAILURE);
   }
}
static int recording_execvp(const char *program, char *const *arguments,
      size_t array_count)
{
   size_t count = 0;
   size_t index;
   ++calls;
   while (expected_argv[count])
      ++count;
   check(strcmp(program, expected_argv[0]) == 0, "selected executable");
   check(array_count == count + 1, "argument array size including terminator");
   for (index = 0; index < count; ++index)
      check(arguments[index] && strcmp(arguments[index], expected_argv[index]) == 0,
            "argument byte content and position");
   check(arguments[count] == NULL, "NULL terminator");
   return 0;
}
#define execvp(program, arguments) recording_execvp(program, arguments, sizeof(arguments) / sizeof(arguments[0]))
'''


ARGV_TESTS = r'''
int main(void)
{
   const char *payloads[] = {
      "-10", "--help", "-fshould-stay-text",
      "A & B; $(literal) \"quoted\"", "<HP> 30/50",
      "Caf\303\251\nNext line"
   };
   size_t index;
   for (index = 0; index < sizeof(payloads) / sizeof(payloads[0]); ++index)
   {
      const char *spd_expected[] = { "spd-say", "-l", "en", "-r",
         "rate-fixture", "-w", "--", NULL, NULL };
      const char *espeak_expected[] = { "espeak", "-ven", "-s170", "--", NULL, NULL };
      const char *say_voice_expected[] = { "say", "-v", "voice-fixture", "-r",
         "speed-fixture", "--", NULL, NULL };
      const char *say_default_expected[] = { "say", "-r", "speed-fixture", "--", NULL, NULL };
      spd_expected[7] = payloads[index];
      espeak_expected[4] = payloads[index];
      say_voice_expected[6] = payloads[index];
      say_default_expected[4] = payloads[index];

      expected_argv = spd_expected;
      calls = 0;
      test_spd(payloads[index]);
      check(calls == 1, "spd-say called once");
      expected_argv = espeak_expected;
      calls = 0;
      test_espeak(payloads[index]);
      check(calls == 1, "espeak called once");
      expected_argv = say_voice_expected;
      calls = 0;
      test_say_voice(payloads[index]);
      check(calls == 1, "say with voice called once");
      expected_argv = say_default_expected;
      calls = 0;
      test_say_default(payloads[index]);
      check(calls == 1, "say with default voice called once");
   }
   printf("PASS argv-construction: 24 recorded calls, %u assertions\n", checks);
   return EXIT_SUCCESS;
}
'''


def argv_test_source(repository):
    unix_source = (repository / 'frontend/drivers/platform_unix.c').read_text(encoding='utf-8')
    mac_source = (repository / 'frontend/drivers/platform_darwin.m').read_text(encoding='utf-8')
    unix_function = extract_block(unix_source,
        r'^static\s+bool\s+accessibility_speak_unix\s*\(', 'accessibility_speak_unix')
    mac_function = extract_block(mac_source,
        r'^static\s+bool\s+accessibility_speak_macos\s*\(', 'accessibility_speak_macos')
    unix_cmds = extract_argv_constructions(unix_function, 'accessibility_speak_unix')
    mac_cmds = extract_argv_constructions(mac_function, 'accessibility_speak_macos')
    fixtures = [
        ('test_spd', '   int speed = 1;\n   const char *language = "en";\n'
         '   const char *spd_rates[] = { "rate-fixture" };\n', unix_cmds[0]),
        ('test_espeak', '   char voice_out[] = "-ven";\n'
         '   char speed_out[] = "-s170";\n', unix_cmds[1]),
        ('test_say_voice', '   int speed = 1;\n'
         '   char *language_speaker = "voice-fixture";\n'
         '   char *speeds[] = { "speed-fixture" };\n', mac_cmds[0]),
        ('test_say_default', '   int speed = 1;\n'
         '   char *speeds[] = { "speed-fixture" };\n', mac_cmds[1]),
    ]
    source = ARGV_PREAMBLE
    for name, declarations, construction in fixtures:
        source += '\nstatic void ' + name + '(const char *speak_text)\n{\n'
        source += declarations + construction + '\n}\n'
    return source + ARGV_TESTS


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', '--repo', dest='source', type=Path, required=True,
                        help='RetroArch source checkout to verify (--repo is an alias)')
    parser.add_argument('--cc', default=shutil.which('gcc') or 'gcc')
    args = parser.parse_args()
    repository = args.source.resolve()
    temporary = tempfile.TemporaryDirectory(prefix='retroarch-speech-test-')
    output = Path(temporary.name)
    try:
        runloop = (repository / 'runloop.c').read_text(encoding='utf-8')
        speech_case = extract_block(runloop,
            r'\bcase\s+RETRO_ENVIRONMENT_ACCESSIBILITY_SPEAK\s*:',
            'RETRO_ENVIRONMENT_ACCESSIBILITY_SPEAK switch case')
        retroarch = (repository / 'retroarch.c').read_text(encoding='utf-8')
        enabled_function = extract_block(retroarch,
            r'^bool\s+is_accessibility_enabled\s*\(', 'is_accessibility_enabled')
        speech_function = extract_block(retroarch,
            r'^bool\s+accessibility_speak_priority\s*\(', 'accessibility_speak_priority')
        source = PREAMBLE + '\n#ifdef HAVE_ACCESSIBILITY\n' + enabled_function + '\n'
        source += speech_function + '\n#endif\n'
        source += '\nstatic bool test_environment(unsigned cmd, void *data)\n{\n'
        source += '   settings_t *settings = config_get_ptr();\n'
        source += '   (void)settings;\n   (void)data;\n   switch (cmd)\n   {\n'
        source += speech_case + '\n      default: return false;\n   }\n}\n'
        source += TESTS
        output.mkdir(parents=True, exist_ok=True)
        compiler_environment = os.environ.copy()
        compiler = Path(shutil.which(args.cc) or args.cc).resolve()
        compiler_environment['PATH'] = str(compiler.parent) + os.pathsep + compiler_environment.get('PATH', '')
        for variable in ('TEMP', 'TMP', 'TMPDIR'):
            compiler_environment[variable] = str(output)
        generated = output / 'actual_speech_case_test.c'
        generated.write_text(source, encoding='utf-8')
        for enabled in (True, False):
            mode = 'enabled' if enabled else 'disabled'
            executable = output / ('speech_case_' + mode + ('.exe' if sys.platform == 'win32' else ''))
            command = [str(compiler), '-std=c89', '-Wall', '-Wextra', '-Werror',
                       '-I', str(repository / 'libretro-common' / 'include')]
            if enabled:
                command.append('-DHAVE_ACCESSIBILITY')
            command += [str(generated), '-o', str(executable)]
            print('Compiling actual-source harness (' + mode + ')', flush=True)
            subprocess.run(command, check=True, env=compiler_environment)
            subprocess.run([str(executable)], check=True, env=compiler_environment)
        argv_source = output / 'actual_argv_construction_test.c'
        argv_source.write_text(argv_test_source(repository), encoding='utf-8')
        argv_executable = output / ('argv_construction' + ('.exe' if sys.platform == 'win32' else ''))
        print('Compiling actual-source argv-construction harness', flush=True)
        subprocess.run([str(compiler), '-std=c89', '-Wall', '-Wextra', '-Werror',
                        str(argv_source), '-o', str(argv_executable)], check=True, env=compiler_environment)
        subprocess.run([str(argv_executable)], check=True, env=compiler_environment)
        print('LIMITS: focused source extraction + recording frontend; no full build, '
              'native speech, audible-output, threading, or cross-platform runtime verification.')
        return 0
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print('FAIL: ' + str(error), file=sys.stderr)
        return 1
    finally:
        temporary.cleanup()


if __name__ == '__main__':
    sys.exit(main())
