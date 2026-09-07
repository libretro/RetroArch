#!/usr/bin/env python3
"""Overlapping-buffer copy check.

Flags calls to copy functions whose source and destination arguments
share the same base buffer expression.  Two passes:

1. Direct: strlcpy/strlcat/strcpy/strncpy/strcat/strncat/memcpy calls
   whose dst and src arguments have the same normalized base
   expression (exact alias, or same base at different offsets), and
   sprintf/snprintf calls whose destination reappears as a format
   argument.  memmove is the sanctioned fix and is never flagged.

2. Helper-mediated: function definitions whose body copies from one
   pointer parameter to another via the functions above WITHOUT an
   alias guard (no `dst == src` / `dst != src` comparison between the
   two parameters anywhere in the body), cross-referenced against
   every call site passing the same base expression for both.

Rationale: strlcpy on macOS resolves to the fortified __strlcpy_chk,
which aborts via __chk_fail_overlap on ANY overlap including exact
aliasing (bd79aee: EXC_BREAKPOINT typing into the OSK), and
overlapping memcpy is undefined everywhere and flagged by ASan
(e942ee2: leaderboard widget element shift).  The portable
strlcpy_retro__ fallback tolerating aliasing on Linux/Windows means
these defects ship silently everywhere except macOS; this check is
the only thing that notices before a Mac user does.

Intentional same-buffer copies (overlap-guarded LZ match runs,
disjoint slice copies inside one array) are recorded in
tools/overlap_copy_allow.list as `file|func|dstbase|srcbase` entries;
see the header of that file.  Exact-alias findings are never
allowlisted.

Usage:
  overlap_copy_check.py [--root DIR] [--allow FILE]   # tree check
  overlap_copy_check.py --selftest                    # fixture check

Exit status: 0 clean, 1 findings (or self-test failure).
"""

import argparse
import os
import re
import sys
import tempfile

COPY_FUNCS = ("strlcpy", "strlcat", "strcpy", "strncpy", "strcat",
              "strncat", "memcpy")
PRINTF_FUNCS = ("sprintf", "snprintf")
CALL_RE = re.compile(
    r"\b(" + "|".join(COPY_FUNCS + PRINTF_FUNCS) + r")\s*\(")
DEF_RE = re.compile(
    r"(?:^|\n)[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w*)\s*\(([^;{}()]*)\)\s*\n?\s*\{",
    re.S)
SKIP_DIRS = {".git", "deps", "obj-unix", "libogc", "pkg", "media"}
SKIP_PREFIXES = ("gfx/include/", "wii/", "gx/",
                 "libretro-common/samples/", "libretro-common/test/")
EXTS = (".c", ".h", ".m", ".mm", ".cpp", ".cc")


def split_args(seg):
    """Args of a call, given text after '('. None if unbalanced."""
    depth, args, cur = 1, [], []
    for ch in seg:
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
            if depth == 0:
                args.append("".join(cur).strip())
                return args
        if ch == "," and depth == 1:
            args.append("".join(cur).strip())
            cur = []
        else:
            cur.append(ch)
    return None


def base_expr(e):
    """Normalize an argument to its base buffer expression: strip
    casts, parens, address-of, and a trailing top-level +/- offset."""
    e = e.strip()
    e = re.sub(r"^\((?:const\s+)?[\w ]+\*+\s*\)", "", e).strip()
    while e.startswith("(") and e.endswith(")"):
        inner, depth, balanced = e[1:-1], 0, True
        for ch in inner:
            if ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth < 0:
                    balanced = False
                    break
        if balanced and depth == 0:
            e = inner.strip()
        else:
            break
    if e.startswith("&"):
        e = e[1:].strip()
    depth = 0
    for i, ch in enumerate(e):
        if ch in "([":
            depth += 1
        elif ch in ")]":
            depth -= 1
        elif depth == 0 and i > 0 and (
                ch == "+"
                or (ch == "-" and (i + 1 >= len(e) or e[i + 1] != ">")
                    and e[i - 1] != "-")):
            e = e[:i].strip()
            break
    # strip a trailing top-level [index] so &a[i] and &a[i+1]
    # normalize to the same base
    depth = 0
    for i, ch in enumerate(e):
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        elif ch == "[" and depth == 0:
            return e[:i].strip()
    return e


def base_ident(expr):
    """Leading identifier of an expression, for parameter matching."""
    e = expr.strip()
    e = re.sub(r"^\((?:const\s+)?[\w ]+\*+\s*\)", "", e).strip()
    while e.startswith("("):
        e = e[1:].strip()
    if e.startswith("&"):
        e = e[1:].strip()
    m = re.match(r"^([A-Za-z_]\w*)", e)
    return m.group(1) if m else None


def param_names(paramstr):
    names = []
    depth, cur, parts = 0, [], []
    for ch in paramstr:
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        if ch == "," and depth == 0:
            parts.append("".join(cur))
            cur = []
        else:
            cur.append(ch)
    if cur:
        parts.append("".join(cur))
    for p in parts:
        p = p.strip()
        if not p or p == "void":
            names.append(None)
            continue
        m = re.search(r"([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?\s*$", p)
        names.append(m.group(1) if m else None)
    return names


def body_of(text, open_brace_idx):
    depth, i = 1, open_brace_idx + 1
    while i < len(text) and depth:
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
        i += 1
    return text[open_brace_idx + 1:i - 1]


def iter_files(root):
    for r, dirs, files in os.walk(root):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        for fn in files:
            if not fn.endswith(EXTS):
                continue
            path = os.path.join(r, fn)
            rel = os.path.relpath(path, root).replace(os.sep, "/")
            if any(rel.startswith(p) for p in SKIP_PREFIXES):
                continue
            yield path, rel


def read(path):
    try:
        with open(path, errors="replace") as f:
            return f.read()
    except OSError:
        return ""


def direct_findings(text, rel):
    """Yield (rel, line, func, dst, src, kind)."""
    for m in CALL_RE.finditer(text):
        func = m.group(1)
        args = split_args(text[m.end():m.end() + 2000])
        if not args or len(args) < 2:
            continue
        # skip prototypes/definitions: first arg starting with a type
        if re.match(r"^(const\s|char\b|void\b|unsigned\b|signed\b|"
                    r"size_t\b|uint\d+_t\b|int\d*_t\b|struct\b)", args[0]):
            continue
        line = text.count("\n", 0, m.start()) + 1
        dst = base_expr(args[0])
        if not dst:
            continue
        if func in PRINTF_FUNCS:
            fmt_and_rest = args[2:] if func == "snprintf" else args[1:]
            for a in fmt_and_rest[1:]:
                if base_expr(a) == dst:
                    yield (rel, line, func, args[0], a, "dst-reused-as-arg")
        else:
            src = base_expr(args[1])
            if dst in ("NULL", "0", "nullptr"):
                continue
            if src and dst == src:
                kind = ("exact-alias"
                        if args[0].strip() == args[1].strip()
                        else "same-base")
                yield (rel, line, func, args[0], args[1], kind)


def collect_helpers(files_text):
    """{name: set((dst_idx, src_idx))} for UNGUARDED param->param
    copy helpers."""
    helpers = {}
    for rel, text in files_text.items():
        for dm in DEF_RE.finditer(text):
            fname, params = dm.group(1), dm.group(2)
            if fname in COPY_FUNCS + PRINTF_FUNCS or fname == "memmove":
                continue
            pnames = param_names(params)
            if sum(1 for p in pnames if p) < 2:
                continue
            body = body_of(text, dm.end() - 1)
            if not CALL_RE.search(body):
                continue
            for cm in CALL_RE.finditer(body):
                fn = cm.group(1)
                args = split_args(body[cm.end():cm.end() + 600])
                if not args or len(args) < 2:
                    continue
                if fn in PRINTF_FUNCS:
                    d = base_ident(args[0])
                    srcs = (args[3:] if fn == "snprintf" else args[2:])
                    cand = [(d, base_ident(a)) for a in srcs]
                else:
                    cand = [(base_ident(args[0]), base_ident(args[1]))]
                for d, s in cand:
                    if (d in pnames and s in pnames and d != s
                            and d is not None and s is not None):
                        # alias guard: any comparison between the two
                        # parameter names in the body
                        guard = re.search(
                            r"\b(?:%s\s*[!=]=\s*%s|%s\s*[!=]=\s*%s)\b"
                            % (re.escape(d), re.escape(s),
                               re.escape(s), re.escape(d)), body)
                        if guard:
                            continue
                        helpers.setdefault(fname, set()).add(
                            (pnames.index(d), pnames.index(s)))
    return helpers


def helper_findings(files_text, helpers):
    if not helpers:
        return
    # also match function-pointer call spellings such as
    # (ozone->word_wrap)(...) - the exact form of the bd79aee OSK bug
    combined = re.compile(
        r"\b(" + "|".join(re.escape(f) for f in helpers)
        + r")\s*\)?\s*\(")
    for rel, text in files_text.items():
        for cm in combined.finditer(text):
            fname = cm.group(1)
            args = split_args(text[cm.end():cm.end() + 1500])
            if not args:
                continue
            if args and re.match(
                    r"^(const\s|char\b|void\b|unsigned\b|size_t\b)",
                    args[0]):
                continue  # definition/prototype
            for di, si in helpers[fname]:
                if di >= len(args) or si >= len(args):
                    continue
                a, b = args[di], args[si]
                ba = base_expr(a)
                if ba in ("NULL", "0", "nullptr"):
                    continue
                if ba and ba == base_expr(b):
                    line = text.count("\n", 0, cm.start()) + 1
                    yield (rel, line, fname, a, b, "helper-aliased")


def load_allow(path):
    allow = set()
    if not path or not os.path.exists(path):
        return allow
    with open(path) as f:
        for raw in f:
            raw = raw.strip()
            if not raw or raw.startswith("#"):
                continue
            allow.add(tuple(raw.split("|")))
    return allow


def allow_key(rel, func, dst, src):
    return (rel, func, base_expr(dst), base_expr(src))


def run_check(root, allow_path, emit_allow=False):
    files_text = {rel: read(path) for path, rel in iter_files(root)}
    allow = load_allow(allow_path)
    findings, used, entries = [], set(), []
    for rel, text in sorted(files_text.items()):
        for f in direct_findings(text, rel):
            rel_, line, func, dst, src, kind = f
            key = allow_key(rel_, func, dst, src)
            entries.append("|".join(key))
            if kind != "exact-alias" and key in allow:
                used.add(key)
                continue
            findings.append(f)
    helpers = collect_helpers(files_text)
    for f in helper_findings(files_text, helpers):
        rel_, line, func, dst, src, kind = f
        key = allow_key(rel_, func, dst, src)
        entries.append("|".join(key))
        if key in allow:
            used.add(key)
            continue
        findings.append(f)
    if emit_allow:
        for e in sorted(set(entries)):
            print(e)
        return 0
    for rel, line, func, dst, src, kind in findings:
        print("%s:%d: %s(%s, %s)  [%s]" % (rel, line, func, dst, src, kind))
    stale = allow - used
    for s in sorted(stale):
        sys.stderr.write("stale allowlist entry: %s\n" % "|".join(s))
    if findings:
        sys.stderr.write(
            "%d overlapping-copy finding(s). Overlapping copies are UB\n"
            "and abort under macOS fortified libc; use memmove, a\n"
            "separate staging buffer, or an alias-guarded helper. For\n"
            "audited-intentional same-buffer copies add the printed\n"
            "file|func|dst|src key to tools/overlap_copy_allow.list.\n"
            % len(findings))
        return 1
    sys.stderr.write("overlap copy check: clean (%d file(s), "
                     "%d helper(s) tracked, %d allowlisted)\n"
                     % (len(files_text), len(helpers), len(used)))
    return 0


# --------------------------------------------------------------------
# self-test

FIXTURE_BAD_EXACT = """
void f(char *buf, const char *in, unsigned long n)
{
   strlcpy(buf, buf, n);
}
"""

FIXTURE_BAD_SHIFT = """
void g(struct info *a, int i, int count)
{
   memcpy(&a[i], &a[i + 1], (count - i) * sizeof(a[i]));
}
"""

FIXTURE_OK_MEMMOVE = """
void g(struct info *a, int i, int count)
{
   memmove(&a[i], &a[i + 1], (count - i) * sizeof(a[i]));
}
"""

FIXTURE_HELPER = """
unsigned long wrap_like(char *s, unsigned long len, const char *src,
      unsigned long src_len)
{
   if (src_len < 10)
      return strlcpy(s, src, len);
   return 0;
}

unsigned long join_guarded(char *s, const char *dir, unsigned long len)
{
   unsigned long _len = 0;
   if (s != dir)
      _len = strlcpy(s, dir, len);
   return _len;
}

void caller(void)
{
   char msg[64];
   char other[64];
   wrap_like(msg, sizeof(msg), msg, 3);      /* BAD: helper-aliased */
   wrap_like(msg, sizeof(msg), other, 3);    /* ok */
   join_guarded(msg, msg, sizeof(msg));      /* ok: guarded helper */
}
"""

FIXTURE_ALLOWED = """
void lz(unsigned char *to, unsigned long have)
{
   /* deliberate disjoint doubling copy */
   memcpy(to + have, to, have);
}
"""


def selftest():
    def check(name, sources, allow_lines, expect_substrings,
              forbid_substrings):
        with tempfile.TemporaryDirectory() as td:
            for fn, content in sources.items():
                with open(os.path.join(td, fn), "w") as f:
                    f.write(content)
            allow_path = None
            if allow_lines:
                allow_path = os.path.join(td, "allow.list")
                with open(allow_path, "w") as f:
                    f.write("\n".join(allow_lines) + "\n")
            import io
            out, err = io.StringIO(), io.StringIO()
            oldout, olderr = sys.stdout, sys.stderr
            sys.stdout, sys.stderr = out, err
            try:
                rc = run_check(td, allow_path)
            finally:
                sys.stdout, sys.stderr = oldout, olderr
            text = out.getvalue() + err.getvalue()
            for s in expect_substrings:
                if s not in text:
                    print("SELFTEST FAIL [%s]: missing %r in:\n%s"
                          % (name, s, text))
                    return False
            for s in forbid_substrings:
                if s in text:
                    print("SELFTEST FAIL [%s]: unexpected %r in:\n%s"
                          % (name, s, text))
                    return False
            expected_rc = 1 if expect_substrings and any(
                "[" in s for s in expect_substrings) else 0
            if rc != expected_rc:
                print("SELFTEST FAIL [%s]: rc=%d expected %d"
                      % (name, rc, expected_rc))
                return False
            return True

    ok = True
    ok &= check("exact-alias flagged",
                {"a.c": FIXTURE_BAD_EXACT}, None,
                ["[exact-alias]"], [])
    ok &= check("overlapping shift flagged",
                {"a.c": FIXTURE_BAD_SHIFT}, None,
                ["[same-base]"], [])
    ok &= check("memmove not flagged",
                {"a.c": FIXTURE_OK_MEMMOVE}, None,
                ["clean"], ["memmove"])
    ok &= check("unguarded helper aliased call flagged, guarded not",
                {"a.c": FIXTURE_HELPER}, None,
                ["wrap_like", "[helper-aliased]"], ["join_guarded"])
    ok &= check("allowlist suppresses same-base",
                {"a.c": FIXTURE_ALLOWED},
                ["a.c|memcpy|to|to"],
                ["clean"], ["[same-base]"])
    ok &= check("exact-alias cannot be allowlisted",
                {"a.c": FIXTURE_BAD_EXACT},
                ["a.c|strlcpy|buf|buf"],
                ["[exact-alias]"], [])
    ok &= check("stale allowlist entry reported",
                {"a.c": FIXTURE_OK_MEMMOVE},
                ["gone.c|memcpy|x|x"],
                ["stale allowlist entry"], [])
    print("selftest: %s" % ("OK" if ok else "FAILED"))
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    ap.add_argument("--allow", default="tools/overlap_copy_allow.list")
    ap.add_argument("--emit-allow", action="store_true",
                    help="print allowlist keys for all current findings")
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args()
    if args.selftest:
        return selftest()
    return run_check(args.root, args.allow, emit_allow=args.emit_allow)


if __name__ == "__main__":
    sys.exit(main())
