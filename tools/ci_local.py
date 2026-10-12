#!/usr/bin/env python3
"""Run the GitHub CI workflows locally, and find the ones that cover a file.

A workflow's steps are its run: blocks, run here in order with the same
shell, environment, working directories and ${{ }} expressions GitHub
gives them, so a local result is the CI result rather than an
approximation of it. Nothing outside the Python standard library is
needed; the YAML the workflows use is read by the small parser below.

  tools/ci_local.py list
      Every workflow and job, with its runner and matrix size.

  tools/ci_local.py which <path>...
      The jobs that build, run or check the given files: those that name
      them (directly, through a script they call, or through a sample's
      Makefile), the full-tree builds, and the tree-wide checkers.

  tools/ci_local.py run <workflow> [--job ID] [--matrix KEY=VALUE]...
                        [--skip REGEX]... [--dry-run] [--clean-copy]
                        [--host] [--force]
      Runs a workflow's jobs, or one job; every matrix combination unless
      --matrix narrows them. <workflow> is a file name, with or without
      .yml, or the workflow's name.
        --dry-run     print each step's resolved script without running it
        --skip REGEX  skip steps whose name matches (e.g. "^Install")
        --clean-copy  run in a fresh worktree of HEAD plus the uncommitted
                      changes, leaving this checkout untouched
        --host        run a container job on this machine instead of in
                      its image (docker or podman is used otherwise)
        --force       run a job whose runner is another OS, or that uses
                      an action this script cannot stand in for

  tools/ci_local.py selftest
      Checks the parser against PyYAML (when installed) and resolves every
      job of every workflow in a dry run.

Actions are not run. actions/checkout is the checkout itself (another
repository is cloned instead), setup-python is the host's Python, and
cache and artifact steps are skipped. A job using any other action is
refused unless --force is given, which skips it.
"""
import fnmatch, json, os, re, shlex, shutil, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WF_DIR = os.path.join(ROOT, '.github', 'workflows')

# --------------------------------------------------------------------------
# YAML: the block subset the workflows use, resolved as PyYAML resolves it
# --------------------------------------------------------------------------

class YamlError(Exception):
   pass

_NULL = re.compile(r'^(?:~|null|Null|NULL|)$')
_BOOL = {'yes': True, 'Yes': True, 'YES': True, 'no': False, 'No': False,
         'NO': False, 'true': True, 'True': True, 'TRUE': True,
         'false': False, 'False': False, 'FALSE': False, 'on': True,
         'On': True, 'ON': True, 'off': False, 'Off': False, 'OFF': False}
_INT = re.compile(r'^(?:[-+]?0b[0-1_]+|[-+]?0[0-7_]+|[-+]?(?:0|[1-9][0-9_]*)'
                  r'|[-+]?0x[0-9a-fA-F_]+|[-+]?[1-9][0-9_]*(?::[0-5]?[0-9])+)$')
_FLOAT = re.compile(r'^(?:[-+]?(?:[0-9][0-9_]*)\.[0-9_]*(?:[eE][-+][0-9]+)?'
                    r'|\.[0-9][0-9_]*(?:[eE][-+][0-9]+)?'
                    r'|[-+]?[0-9][0-9_]*(?::[0-5]?[0-9])+\.[0-9_]*'
                    r'|[-+]?\.(?:inf|Inf|INF)|\.(?:nan|NaN|NAN))$')


def _resolve(s):
   if _NULL.match(s):
      return None
   if s in _BOOL:
      return _BOOL[s]
   if _INT.match(s):
      t = s.replace('_', '')
      sign = -1 if t[0] == '-' else 1
      if t[0] in '+-':
         t = t[1:]
      if t == '0':
         return 0
      if t.startswith('0b'):
         return sign * int(t[2:], 2)
      if t.startswith('0x'):
         return sign * int(t[2:], 16)
      if ':' in t:
         v = 0
         for part in t.split(':'):
            v = v * 60 + int(part)
         return sign * v
      if t[0] == '0':
         return sign * int(t, 8)
      return sign * int(t)
   if _FLOAT.match(s):
      t = s.replace('_', '').lower()
      if t.endswith('.inf'):
         return float('-inf') if t[0] == '-' else float('inf')
      if t.endswith('.nan'):
         return float('nan')
      if ':' in t:
         sign = -1 if t[0] == '-' else 1
         v = 0.0
         for part in t.lstrip('+-').split(':'):
            v = v * 60 + float(part)
         return sign * v
      return float(t)
   return s


def _strip_comment(s):
   """Cut a trailing ' #' comment, outside quotes."""
   q = None
   i = 0
   while i < len(s):
      c = s[i]
      if q:
         if c == '\\' and q == '"':
            i += 1
         elif c == q:
            q = None
      elif c in '"\'' and (i == 0 or s[i - 1] in ' \t[{,:'):
         q = c
      elif c == '#' and (i == 0 or s[i - 1] in ' \t'):
         return s[:i].rstrip()
      i += 1
   return s.rstrip()


def _indent(line):
   return len(line) - len(line.lstrip(' '))


_ESC = {'0': '\0', 'a': '\a', 'b': '\b', 't': '\t', '\t': '\t', 'n': '\n',
        'v': '\v', 'f': '\f', 'r': '\r', 'e': '\x1b', ' ': ' ', '"': '"',
        '/': '/', '\\': '\\', 'N': '\x85', '_': '\xa0', 'L': '\u2028',
        'P': '\u2029'}


def _fold(parts):
   """Join the lines of a multi-line flow scalar."""
   out = parts[0]
   pending = 0
   for p in parts[1:]:
      if p == '':
         pending += 1
         continue
      out += ('\n' * pending) if pending else ' '
      pending = 0
      out += p
   if pending:
      out += '\n' * pending
   return out


class _Parser:
   def __init__(self, text):
      self.lines = text.split('\n')

   def blank(self, i):
      s = self.lines[i].strip()
      return s == '' or s.startswith('#') or s == '---'

   def next_content(self, i):
      while i < len(self.lines) and self.blank(i):
         i += 1
      return i

   def parse(self):
      i = self.next_content(0)
      if i >= len(self.lines):
         return None
      node, i = self.node(i, -1)
      if self.next_content(i) < len(self.lines):
         raise YamlError('unparsed content at line %d' % (i + 1))
      return node

   def node(self, i, parent):
      i = self.next_content(i)
      if i >= len(self.lines):
         return None, i
      ind = _indent(self.lines[i])
      if ind <= parent:
         return None, i
      body = self.lines[i][ind:]
      if body == '-' or body.startswith('- '):
         return self.seq(i, ind)
      if self.key(body) is not None:
         return self.mapping(i, ind)
      return self.scalar(i, ind, body, parent)

   def key(self, body):
      """(key, rest) when the line is a mapping entry, else None."""
      if body[:1] in '"\'':
         q = body[0]
         j = 1
         while j < len(body):
            if q == '"' and body[j] == '\\':
               j += 2
               continue
            if body[j] == q:
               if q == "'" and body[j + 1:j + 2] == "'":
                  j += 2
                  continue
               break
            j += 1
         rest = body[j + 1:].lstrip(' ')
         if not rest.startswith(':') or rest[1:2] not in ('', ' '):
            return None
         return self.flow_scalar(body[:j + 1]), rest[1:].strip()
      if body[:1] in '[{#|>!&*%@`':
         return None
      m = re.match(r'^([^#]*?)\s*:(?:\s|$)', body)
      if not m:
         return None
      return _resolve(m.group(1)), body[m.end():].strip()

   def mapping(self, i, ind):
      out = {}
      while True:
         i = self.next_content(i)
         if i >= len(self.lines) or _indent(self.lines[i]) != ind:
            break
         body = self.lines[i][ind:]
         if body == '-' or body.startswith('- '):
            break
         kv = self.key(body)
         if kv is None:
            raise YamlError('expected a key at line %d' % (i + 1))
         k, rest = kv
         rest = _strip_comment(rest)
         if rest == '':
            j = self.next_content(i + 1)
            if j < len(self.lines) and _indent(self.lines[j]) == ind \
                  and (self.lines[j][ind:] == '-'
                       or self.lines[j][ind:].startswith('- ')):
               v, i = self.seq(j, ind)
            else:
               v, i = self.node(i + 1, ind)
         else:
            v, i = self.value(i, ind, rest)
         out[k] = v
      return out, i

   def seq(self, i, ind):
      out = []
      while True:
         i = self.next_content(i)
         if i >= len(self.lines) or _indent(self.lines[i]) != ind:
            break
         body = self.lines[i][ind:]
         if not (body == '-' or body.startswith('- ')):
            break
         rest = body[1:]
         if rest.strip() == '' or rest.strip().startswith('#'):
            v, i = self.node(i + 1, ind)
         else:
            col = ind + 1 + _indent(rest)
            self.lines[i] = ' ' * col + rest.lstrip(' ')
            v, i = self.node(i, ind)
         out.append(v)
      return out, i

   def scalar(self, i, ind, body, parent):
      return self.value(i, parent, _strip_comment(body))

   def value(self, i, ind, rest):
      """A value that starts on line i after a key (or a bare item)."""
      if rest[:1] in '|>':
         return self.block_scalar(i, ind, rest)
      if rest[:1] in '[{':
         text = rest
         j = i
         while not self.balanced(text):
            j += 1
            if j >= len(self.lines):
               raise YamlError('unclosed flow collection at line %d' % (i + 1))
            text += ' ' + _strip_comment(self.lines[j].strip())
         v, pos = self.flow(text, 0)
         return v, j + 1
      if rest[:1] in '"\'':
         text = rest
         j = i
         while not self.quote_closed(text):
            j += 1
            if j >= len(self.lines):
               raise YamlError('unclosed quote at line %d' % (i + 1))
            text += '\n' + self.lines[j]
         return self.flow_scalar(text), j + 1
      parts = [rest]
      j = i + 1
      while j < len(self.lines):
         line = self.lines[j]
         s = line.strip()
         if s == '':
            parts.append('')
            j += 1
            continue
         if _indent(line) <= ind or s.startswith('#'):
            break
         parts.append(_strip_comment(s))
         j += 1
      while len(parts) > 1 and parts[-1] == '':
         parts.pop()
         j -= 1
      while j > i + 1 and self.lines[j - 1].strip() == '':
         j -= 1
      return _resolve(_fold(parts)), j

   def quote_closed(self, text):
      q = text[0]
      j = 1
      while j < len(text):
         if q == '"' and text[j] == '\\':
            j += 2
            continue
         if text[j] == q:
            if q == "'" and text[j + 1:j + 2] == "'":
               j += 2
               continue
            return True
         j += 1
      return False

   def balanced(self, text):
      depth = 0
      q = None
      for k, c in enumerate(text):
         if q:
            if c == q:
               q = None
         elif c in '"\'' and (k == 0 or text[k - 1] in ' [{,:'):
            q = c
         elif c in '[{':
            depth += 1
         elif c in ']}':
            depth -= 1
            if depth == 0:
               return True
      return False

   def block_scalar(self, i, ind, header):
      m = re.match(r'^([|>])([+-]?)([1-9]?)([+-]?)\s*(?:#.*)?$', header)
      if not m:
         raise YamlError('bad block scalar header at line %d' % (i + 1))
      style, chomp, digit = m.group(1), m.group(2) or m.group(4), m.group(3)
      j = i + 1
      if digit:
         content = max(ind, 0) + int(digit)
      else:
         k = j
         while k < len(self.lines) and self.lines[k].strip() == '':
            k += 1
         content = _indent(self.lines[k]) if k < len(self.lines) else 0
         if content <= ind:
            content = ind + 1
      body = []
      while j < len(self.lines):
         line = self.lines[j]
         if line.strip() == '':
            body.append(line[content:] if len(line) > content else '')
            j += 1
            continue
         if _indent(line) < content:
            break
         body.append(line[content:])
         j += 1
      trailing = 0
      while body and body[-1].strip() == '':
         body.pop()
         trailing += 1
      end = j - trailing
      if style == '|':
         text = '\n'.join(body)
      else:
         text = ''
         prev_more = True
         for n, line in enumerate(body):
            more = line[:1] in (' ', '\t')
            if n == 0:
               text = line
            elif line == '':
               text += '\n'
            elif text.endswith('\n') or more or prev_more:
               text += ('\n' if not text.endswith('\n') else '') + line
            else:
               text += ' ' + line
            prev_more = more or line == ''
      if chomp == '-':
         out = text
      elif chomp == '+':
         out = (text + '\n' if body else '') + '\n' * trailing
      else:
         out = text + '\n' if body else ''
      return out, end

   def flow_scalar(self, text):
      text = text.strip()
      q = text[0]
      if q == "'":
         end = len(text) - 1
         parts = text[1:end].split('\n')
         if len(parts) > 1:
            parts = [parts[0].rstrip(' ')] + [p.strip(' ') for p in parts[1:-1]] \
                  + [parts[-1].lstrip(' ')]
         return _fold(parts).replace("''", "'")
      out = ''
      j = 1
      lines = []
      while j < len(text):
         c = text[j]
         if c == '\\':
            n = text[j + 1]
            if n == '\n':
               j += 2
               while j < len(text) and text[j] in ' \t':
                  j += 1
               out += '\x00JOIN'
               continue
            if n == 'x':
               out += chr(int(text[j + 2:j + 4], 16))
               j += 4
               continue
            if n == 'u':
               out += chr(int(text[j + 2:j + 6], 16))
               j += 6
               continue
            if n == 'U':
               out += chr(int(text[j + 2:j + 10], 16))
               j += 10
               continue
            out += _ESC[n]
            j += 2
            continue
         if c == '"':
            break
         out += c
         j += 1
      parts = out.split('\n')
      if len(parts) > 1:
         parts = [parts[0].rstrip(' ')] + [p.strip(' ') for p in parts[1:-1]] \
               + [parts[-1].lstrip(' ')]
         out = _fold(parts)
      return out.replace('\x00JOIN', '')

   def flow(self, s, p):
      p = self.ws(s, p)
      c = s[p]
      if c == '[':
         out = []
         p += 1
         while True:
            p = self.ws(s, p)
            if s[p] == ']':
               return out, p + 1
            v, p = self.flow(s, p)
            out.append(v)
            p = self.ws(s, p)
            if s[p] == ',':
               p += 1
      if c == '{':
         out = {}
         p += 1
         while True:
            p = self.ws(s, p)
            if s[p] == '}':
               return out, p + 1
            k, p = self.flow_atom(s, p, key=True)
            p = self.ws(s, p)
            v = None
            if s[p] == ':':
               p = self.ws(s, p + 1)
               if s[p] not in ',}':
                  v, p = self.flow(s, p)
            out[k] = v
            p = self.ws(s, p)
            if s[p] == ',':
               p += 1
      return self.flow_atom(s, p)

   def ws(self, s, p):
      while p < len(s) and s[p] in ' \t\n':
         p += 1
      return p

   def flow_atom(self, s, p, key=False):
      if s[p] in '"\'':
         q = s[p]
         j = p + 1
         while j < len(s):
            if q == '"' and s[j] == '\\':
               j += 2
               continue
            if s[j] == q:
               if q == "'" and s[j + 1:j + 2] == "'":
                  j += 2
                  continue
               break
            j += 1
         return self.flow_scalar(s[p:j + 1]), j + 1
      j = p
      while j < len(s):
         if s[j] in ',]}':
            break
         if s[j] == ':' and (j + 1 >= len(s) or s[j + 1] in ' ,]}'):
            break
         j += 1
      return _resolve(s[p:j].strip()), j


def yaml_load(text):
   return _Parser(text).parse()


# --------------------------------------------------------------------------
# ${{ }} expressions
# --------------------------------------------------------------------------

class ExprError(Exception):
   pass


class Unresolved(Exception):
   """A value that only exists while the job runs (a step's output)."""


_TOKEN = re.compile(r"\s*(?:(?P<num>-?(?:0x[0-9a-fA-F]+|[0-9]+(?:\.[0-9]+)?"
                    r"(?:[eE][-+]?[0-9]+)?))|(?P<str>'(?:[^']|'')*')"
                    r"|(?P<op>==|!=|<=|>=|&&|\|\||[<>!()\[\],.*])"
                    r"|(?P<id>[A-Za-z_][A-Za-z0-9_-]*))")


def _tokens(src):
   out = []
   p = 0
   src = src.strip()
   while p < len(src):
      m = _TOKEN.match(src, p)
      if not m or m.end() == p:
         raise ExprError('cannot read expression: %s' % src)
      for k in ('num', 'str', 'op', 'id'):
         if m.group(k) is not None:
            out.append((k, m.group(k)))
            break
      p = m.end()
      while p < len(src) and src[p] in ' \t\n':
         p += 1
   return out


def to_str(v):
   if v is None:
      return ''
   if v is True:
      return 'true'
   if v is False:
      return 'false'
   if isinstance(v, float):
      if v != v:
         return 'NaN'
      if v == int(v) and abs(v) < 1e15:
         return str(int(v))
      return repr(v)
   if isinstance(v, (dict, list)):
      return json.dumps(v, indent=2)
   return str(v)


def to_num(v):
   if v is None:
      return 0.0
   if isinstance(v, bool):
      return 1.0 if v else 0.0
   if isinstance(v, (int, float)):
      return float(v)
   if isinstance(v, str):
      s = v.strip()
      if s == '':
         return 0.0
      try:
         return float(int(s, 16)) if s.lower().startswith('0x') else float(s)
      except ValueError:
         return float('nan')
   return float('nan')


def truthy(v):
   if v is None or v is False:
      return False
   if isinstance(v, (int, float)) and not isinstance(v, bool):
      return v == v and v != 0
   if isinstance(v, str):
      return v != ''
   return True


def loose_eq(a, b):
   if isinstance(a, str) and isinstance(b, str):
      return a.lower() == b.lower()
   if isinstance(a, (dict, list)) or isinstance(b, (dict, list)):
      return a is b
   if type(a) is type(b) and not isinstance(a, str):
      return a == b
   return to_num(a) == to_num(b)


class Expr:
   """Evaluates one expression against the run's contexts."""

   FUNCS = ('success', 'failure', 'always', 'cancelled', 'startswith',
            'endswith', 'contains', 'format', 'join', 'tojson', 'fromjson',
            'hashfiles')

   def __init__(self, ctx, status):
      self.ctx = ctx
      self.status = status

   def eval(self, src):
      self.toks = _tokens(src)
      self.p = 0
      v = self.or_()
      if self.p != len(self.toks):
         raise ExprError('trailing input in: %s' % src)
      return v

   def peek(self):
      return self.toks[self.p] if self.p < len(self.toks) else (None, None)

   def take(self, val=None):
      t = self.peek()
      if val is not None and t[1] != val:
         raise ExprError('expected %s' % val)
      self.p += 1
      return t

   def or_(self):
      v = self.and_()
      while self.peek()[1] == '||':
         self.take()
         r = self.and_()
         v = v if truthy(v) else r
      return v

   def and_(self):
      v = self.cmp()
      while self.peek()[1] == '&&':
         self.take()
         r = self.cmp()
         v = r if truthy(v) else v
      return v

   def cmp(self):
      v = self.unary()
      while self.peek()[1] in ('==', '!=', '<', '>', '<=', '>='):
         op = self.take()[1]
         r = self.unary()
         if op == '==':
            v = loose_eq(v, r)
         elif op == '!=':
            v = not loose_eq(v, r)
         else:
            a, b = to_num(v), to_num(r)
            v = {'<': a < b, '>': a > b, '<=': a <= b, '>=': a >= b}[op]
      return v

   def unary(self):
      if self.peek()[1] == '!':
         self.take()
         return not truthy(self.unary())
      return self.postfix()

   def postfix(self):
      kind, val = self.peek()
      if val == '(':
         self.take()
         v = self.or_()
         self.take(')')
      elif kind == 'num':
         self.take()
         v = float(int(val, 16)) if 'x' in val.lower() else float(val)
      elif kind == 'str':
         self.take()
         v = val[1:-1].replace("''", "'")
      elif kind == 'id':
         self.take()
         low = val.lower()
         if low in ('true', 'false'):
            return low == 'true'
         if low == 'null':
            return None
         if self.peek()[1] == '(':
            v = self.call(low)
         elif val not in self.ctx:
            raise ExprError('unknown context: %s' % val)
         else:
            v = self.ctx[val]
      else:
         raise ExprError('unexpected %s' % val)
      while self.peek()[1] in ('.', '['):
         if self.take()[1] == '.':
            k = self.take()
            if k[0] != 'id' and k[1] != '*':
               raise ExprError('bad property %s' % k[1])
            v = self.prop(v, k[1])
         else:
            k = self.or_()
            self.take(']')
            v = self.prop(v, k if isinstance(k, str) else int(to_num(k)))
      return v

   def prop(self, v, k):
      if isinstance(v, Unresolved):
         return v
      if isinstance(v, dict):
         if isinstance(k, str):
            for key in v:
               if isinstance(key, str) and key.lower() == k.lower():
                  return v[key]
         return None
      if isinstance(v, list) and isinstance(k, int):
         return v[k] if 0 <= k < len(v) else None
      return None

   def call(self, name):
      self.take('(')
      args = []
      while self.peek()[1] != ')':
         args.append(self.or_())
         if self.peek()[1] == ',':
            self.take()
      self.take(')')
      if name not in self.FUNCS:
         raise ExprError('unknown function: %s' % name)
      if name == 'success':
         return self.status == 'success'
      if name == 'failure':
         return self.status == 'failure'
      if name == 'always':
         return True
      if name == 'cancelled':
         return False
      for a in args:
         if isinstance(a, Unresolved):
            raise a
      if name == 'startswith':
         return to_str(args[0]).lower().startswith(to_str(args[1]).lower())
      if name == 'endswith':
         return to_str(args[0]).lower().endswith(to_str(args[1]).lower())
      if name == 'contains':
         if isinstance(args[0], list):
            return any(loose_eq(x, args[1]) for x in args[0])
         return to_str(args[1]).lower() in to_str(args[0]).lower()
      if name == 'format':
         fmt = to_str(args[0])
         return re.sub(r'\{(\d+)\}', lambda m: to_str(args[int(m.group(1)) + 1]),
                       fmt).replace('{{', '{').replace('}}', '}')
      if name == 'join':
         sep = to_str(args[1]) if len(args) > 1 else ','
         if isinstance(args[0], list):
            return sep.join(to_str(x) for x in args[0])
         return to_str(args[0])
      if name == 'tojson':
         return json.dumps(args[0], indent=2)
      if name == 'fromjson':
         return json.loads(to_str(args[0]))
      return ''


_TEMPLATE = re.compile(r'\$\{\{((?:(?!\}\}).)*)\}\}', re.S)


def interpolate(text, ctx, status='success'):
   if not isinstance(text, str):
      return text
   def sub(m):
      v = Expr(ctx, status).eval(m.group(1))
      if isinstance(v, Unresolved):
         raise v
      return to_str(v)
   return _TEMPLATE.sub(sub, text)


def evaluate(text, ctx, status='success'):
   """A value that may be a whole ${{ }} expression keeps its type."""
   if isinstance(text, str):
      m = _TEMPLATE.fullmatch(text.strip())
      if m:
         v = Expr(ctx, status).eval(m.group(1))
         if isinstance(v, Unresolved):
            raise v
         return v
      return interpolate(text, ctx, status)
   if isinstance(text, list):
      return [evaluate(x, ctx, status) for x in text]
   if isinstance(text, dict):
      return dict((k, evaluate(v, ctx, status)) for k, v in text.items())
   return text


def condition(src, ctx, status):
   """A job's or step's if:, with success() when it has none."""
   if src is None:
      return status == 'success'
   if isinstance(src, bool):
      return src
   s = str(src).strip()
   m = _TEMPLATE.fullmatch(s)
   if m:
      s = m.group(1)
   if not re.search(r'\b(success|failure|always|cancelled)\s*\(', s):
      s = 'success() && (%s)' % s
   v = Expr(ctx, status).eval(s)
   if isinstance(v, Unresolved):
      raise v
   return truthy(v)


# --------------------------------------------------------------------------
# Workflows
# --------------------------------------------------------------------------

class Workflow:
   def __init__(self, path):
      self.path = path
      self.file = os.path.basename(path)
      with open(path, encoding='utf-8') as f:
         self.text = f.read()
      self.data = yaml_load(self.text) or {}
      self.name = self.data.get('name') or os.path.relpath(path, ROOT)
      self.jobs = self.data.get('jobs') or {}

   def triggers(self):
      on = self.data.get(True, self.data.get('on'))
      if isinstance(on, str):
         return {on: None}
      if isinstance(on, list):
         return dict((k, None) for k in on)
      return on or {}

   def inputs(self):
      wd = self.triggers().get('workflow_dispatch') or {}
      out = {}
      for k, v in (wd.get('inputs') or {}).items():
         out[k] = (v or {}).get('default')
      return out


def load_workflows():
   out = []
   for name in sorted(os.listdir(WF_DIR)):
      if name.endswith(('.yml', '.yaml')):
         out.append(Workflow(os.path.join(WF_DIR, name)))
   return out


def find_workflow(arg):
   wfs = load_workflows()
   for wf in wfs:
      if arg in (wf.file, os.path.splitext(wf.file)[0], wf.name) \
            or os.path.abspath(arg) == wf.path:
         return wf
   low = arg.lower()
   hits = [wf for wf in wfs if low in wf.file.lower() or low in str(wf.name).lower()]
   if len(hits) == 1:
      return hits[0]
   if hits:
      sys.exit('ci_local: "%s" matches several workflows: %s'
               % (arg, ', '.join(w.file for w in hits)))
   sys.exit('ci_local: no workflow named "%s"' % arg)


def git(*args, **kw):
   cwd = kw.pop('cwd', ROOT)
   return subprocess.run(('git',) + args, cwd=cwd, stdout=subprocess.PIPE,
                         stderr=subprocess.DEVNULL,
                         universal_newlines=True).stdout.strip()


def base_context(wf, job_id, workspace, temp):
   sha = git('rev-parse', 'HEAD') or '0' * 40
   branch = git('rev-parse', '--abbrev-ref', 'HEAD')
   ref = 'refs/heads/' + (branch if branch and branch != 'HEAD' else 'master')
   repo = 'libretro/RetroArch'
   url = git('remote', 'get-url', 'origin')
   m = re.search(r'github\.com[:/]+([^/]+/[^/]+?)(?:\.git)?/?$', url or '')
   if m:
      repo = m.group(1)
   github = {
      'workflow': wf.name, 'job': job_id, 'sha': sha, 'workflow_sha': sha,
      'ref': ref, 'ref_name': ref[len('refs/heads/'):], 'ref_type': 'branch',
      'event_name': 'workflow_dispatch', 'event': {}, 'workspace': workspace,
      'repository': repo, 'repository_owner': repo.split('/')[0],
      'server_url': 'https://github.com', 'actor': 'local', 'token': '',
      'run_id': '0', 'run_number': '0', 'run_attempt': '1',
      'action_path': '', 'action': '', 'base_ref': '', 'head_ref': '',
   }
   return {
      'github': github,
      'env': {},
      'vars': {},
      'secrets': {},
      'inputs': wf.inputs(),
      'runner': {'os': host_os(), 'arch': 'X64', 'temp': temp,
                 'tool_cache': temp, 'name': 'local'},
      'matrix': {},
      'strategy': {'job-index': 0, 'job-total': 1, 'fail-fast': True,
                   'max-parallel': 1},
      'steps': {},
      'needs': {},
      'job': {'status': 'success'},
   }


def host_os():
   if sys.platform.startswith('win') or os.environ.get('MSYSTEM'):
      return 'Windows'
   if sys.platform == 'darwin':
      return 'macOS'
   return 'Linux'


def matrix_combos(job, ctx):
   strat = job.get('strategy') or {}
   m = strat.get('matrix')
   if m is None:
      return [{}]
   m = evaluate(m, ctx)
   if not isinstance(m, dict):
      raise ExprError('matrix is not a mapping')
   include = m.get('include') or []
   exclude = m.get('exclude') or []
   axes = [(k, v if isinstance(v, list) else [v]) for k, v in m.items()
           if k not in ('include', 'exclude')]
   combos = [{}]
   for k, vals in axes:
      combos = [dict(c, **{k: v}) for c in combos for v in vals]
   if not axes:
      combos = []
   def matches(c, e):
      return all(k in c and loose_eq(c[k], v) for k, v in e.items())
   combos = [c for c in combos if not any(matches(c, e) for e in exclude)]
   originals = [dict(c) for c in combos]
   for inc in include:
      added = False
      for c, orig in zip(combos, originals):
         if all(k not in orig or loose_eq(orig[k], v) for k, v in inc.items()):
            c.update(inc)
            added = True
      if not added:
         combos.append(dict(inc))
         originals.append(dict(inc))
   return combos or [{}]


def job_order(wf):
   order = []
   seen = set()
   def visit(j, stack):
      if j in seen:
         return
      if j in stack:
         raise ExprError('needs: cycle at %s' % j)
      needs = wf.jobs[j].get('needs') or []
      for n in ([needs] if isinstance(needs, str) else needs):
         visit(n, stack + [j])
      seen.add(j)
      order.append(j)
   for j in wf.jobs:
      visit(j, [])
   return order


STAND_INS = {
   'actions/checkout': 'checkout',
   'taiki-e/checkout-action': 'checkout',
   'actions/setup-python': 'host',
   'actions/cache': 'skip',
   'actions/upload-artifact': 'skip',
   'actions/download-artifact': 'skip',
}


def action_kind(uses):
   return STAND_INS.get(uses.split('@')[0])


def runs_on_os(label):
   s = to_str(label).lower()
   if 'windows' in s:
      return 'Windows'
   if 'macos' in s:
      return 'macOS'
   if 'ubuntu' in s or 'linux' in s:
      return 'Linux'
   return None


def shell_command(shell, script, plat):
   if shell is None:
      shell = 'bash' if plat != 'Windows' else 'pwsh'
      explicit = False
   else:
      explicit = True
   s = shell.strip()
   if s == 'bash':
      return (['bash', '--noprofile', '--norc', '-eo', 'pipefail', script]
              if explicit else ['bash', '-e', script])
   if s == 'sh':
      return ['sh', '-e', script]
   if s == 'python':
      return ['python3', script]
   if '{0}' in s and not s.startswith(('msys2', 'pwsh', 'powershell', 'cmd')):
      return [script if a == '{0}' else a for a in shlex.split(s)]
   return None


# --------------------------------------------------------------------------
# run
# --------------------------------------------------------------------------

class Runner:
   def __init__(self, opts):
      self.opts = opts
      self.failed = []

   def say(self, msg):
      sys.stdout.write(msg + '\n')
      sys.stdout.flush()

   def run_workflow(self, wf, workspace):
      job_ids = job_order(wf)
      if self.opts.job:
         if self.opts.job not in wf.jobs:
            sys.exit('ci_local: %s has no job "%s" (jobs: %s)'
                     % (wf.file, self.opts.job, ', '.join(wf.jobs)))
         job_ids = [self.opts.job]
      results = {}
      for jid in job_ids:
         results[jid] = self.run_job(wf, jid, workspace, results)
      return not self.failed

   def run_job(self, wf, jid, workspace, results):
      job = wf.jobs[jid]
      if 'uses' in job:
         self.say('== %s / %s: calls workflow %s, not run' % (wf.file, jid, job['uses']))
         return {'result': 'skipped', 'outputs': {}}
      temp = tempfile.mkdtemp(prefix='ci_local.')
      try:
         return self.run_job_in(wf, jid, job, workspace, results, temp)
      finally:
         if not self.opts.dry_run:
            shutil.rmtree(temp, ignore_errors=True)

   def run_job_in(self, wf, jid, job, workspace, results, temp):
      ctx = base_context(wf, jid, workspace, temp)
      ctx['needs'] = dict((k, v) for k, v in results.items())
      ctx['env'] = dict((k, to_str(evaluate(v, ctx)))
                        for k, v in (wf.data.get('env') or {}).items())
      combos = matrix_combos(job, ctx)
      wanted = self.opts.matrix
      picked = [(n, c) for n, c in enumerate(combos)
                if all(k in c and loose_eq(c[k], v) for k, v in wanted.items())]
      if wanted and not picked:
         self.say('== %s / %s: no matrix combination matches %s'
                  % (wf.file, jid, ' '.join('%s=%s' % kv for kv in wanted.items())))
         self.failed.append(jid)
         return {'result': 'failure', 'outputs': {}}
      outputs = {}
      seen = set()
      for n, combo in picked:
         r, out = self.run_combo(wf, jid, job, workspace, temp, combo, n,
                                 len(combos), results)
         outputs.update(out)
         seen.add(r)
      result = 'failure' if 'failure' in seen else \
            'success' if 'success' in seen else 'skipped'
      return {'result': result, 'outputs': outputs}

   def run_combo(self, wf, jid, job, workspace, temp, combo, index, total,
                 results):
      ctx = base_context(wf, jid, workspace, temp)
      ctx['needs'] = results
      ctx['matrix'] = combo
      ctx['strategy']['job-index'] = index
      ctx['strategy']['job-total'] = total
      ctx['env'] = dict((k, to_str(evaluate(v, ctx)))
                        for k, v in (wf.data.get('env') or {}).items())
      label = '%s / %s' % (wf.file, jid)
      if combo:
         label += ' [%s]' % ', '.join('%s=%s' % (k, to_str(v)) for k, v in combo.items())
      needs = job.get('needs') or []
      needs = [needs] if isinstance(needs, str) else needs
      upstream = 'success' if all((results.get(n) or {}).get('result')
                                  in ('success', None) for n in needs) \
            else 'failure'
      if not condition(job.get('if'), ctx, upstream):
         self.say('== %s: skipped (%s)' % (label, 'a job it needs did not pass'
                  if upstream != 'success' else 'by its if:'))
         return 'skipped', {}
      plat = runs_on_os(evaluate(job.get('runs-on'), ctx)) or 'Linux'
      if plat != host_os() and not self.opts.force:
         self.say('== %s: runs on %s, not run here (use --force to try)'
                  % (label, plat))
         return 'skipped', {}
      steps = job.get('steps') or []
      blocked = [s['uses'] for s in steps if 'uses' in s
                 and action_kind(s['uses']) is None]
      if blocked and not self.opts.force:
         self.say('== %s: uses %s, not run here (--force skips those steps)'
                  % (label, ', '.join(sorted(set(blocked)))))
         return 'skipped', {}
      container = evaluate(job.get('container'), ctx)
      if isinstance(container, str):
         container = {'image': container}
      engine = None
      cid = None
      if container and not self.opts.host:
         engine = shutil.which('docker') or shutil.which('podman')
         if not engine and not self.opts.dry_run:
            self.say('== %s: needs container %s; install docker or podman, '
                     'or pass --host' % (label, container.get('image')))
            self.failed.append(label)
            return 'failure', {}
      for k, v in (job.get('env') or {}).items():
         ctx['env'][k] = to_str(evaluate(v, ctx))
      defaults = dict(((wf.data.get('defaults') or {}).get('run') or {}))
      defaults.update(((job.get('defaults') or {}).get('run') or {}))
      self.say('== %s%s' % (label, (' in ' + container['image']) if engine else ''))
      files = {}
      for name in ('env', 'path', 'output', 'step_summary'):
         files[name] = os.path.join(temp, 'github_' + name)
      extra_path = []
      status = 'success'
      if engine and not self.opts.dry_run:
         cid = self.start_container(engine, container, workspace, temp)
         if not cid:
            self.failed.append(label)
            self.say('== %s: FAILED' % label)
            return 'failure', {}
      try:
         for n, step in enumerate(steps):
            status = self.run_step(wf, job, step, n, ctx, status, plat,
                                   defaults, workspace, temp, files,
                                   extra_path, engine, cid)
      finally:
         if cid:
            subprocess.run([engine, 'rm', '-f', cid], stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL)
      outputs = {}
      if not self.opts.dry_run:
         for k, v in (job.get('outputs') or {}).items():
            try:
               outputs[k] = interpolate(v, ctx, status)
            except Unresolved:
               outputs[k] = ''
      if status == 'failure':
         self.failed.append(label)
         self.say('== %s: FAILED' % label)
      elif not self.opts.dry_run:
         self.say('== %s: passed' % label)
      return status, outputs

   def start_container(self, engine, container, workspace, temp):
      args = [engine, 'run', '-d', '--entrypoint', 'tail',
              '-v', '%s:%s' % (workspace, workspace),
              '-v', '%s:%s' % (temp, temp), '-w', workspace]
      args += shlex.split(to_str(container.get('options') or ''))
      for k, v in (container.get('env') or {}).items():
         args += ['-e', '%s=%s' % (k, to_str(v))]
      args += [container['image'], '-f', '/dev/null']
      r = subprocess.run(args, stdout=subprocess.PIPE, universal_newlines=True)
      if r.returncode:
         self.say('   could not start %s' % container['image'])
         return None
      return r.stdout.strip()

   def run_step(self, wf, job, step, n, ctx, status, plat, defaults,
                workspace, temp, files, extra_path, engine, cid):
      sid = step.get('id') or '__step%d' % n
      name = step.get('name') or step.get('uses') or \
            (step.get('run') or '').strip().split('\n')[0]
      try:
         name = interpolate(to_str(name), ctx, status)
      except (Unresolved, ExprError):
         pass
      record = {'outputs': {}, 'outcome': 'skipped', 'conclusion': 'skipped'}
      ctx['steps'][sid] = record
      for rx in self.opts.skip:
         if re.search(rx, name):
            self.say('-- %s: skipped (--skip)' % name)
            return status
      try:
         run_it = condition(step.get('if'), ctx, status)
      except Unresolved:
         if not self.opts.dry_run:
            raise
         self.say('-- %s: if: %s (decided while the job runs)' % (name, step.get('if')))
         run_it = True
      if not run_it:
         self.say('-- %s: skipped by its if:' % name)
         return status
      env = dict(ctx['env'])
      for k, v in (step.get('env') or {}).items():
         try:
            env[k] = to_str(evaluate(v, dict(ctx, env=env), status))
         except Unresolved:
            env[k] = ''
      if 'uses' in step:
         return self.run_action(step, name, ctx, env, status, workspace, record)
      script = step.get('run')
      if script is None:
         return status
      try:
         body = interpolate(to_str(script), dict(ctx, env=env), status)
      except Unresolved:
         body = None
      shell = step.get('shell') or defaults.get('shell')
      wd = step.get('working-directory') or defaults.get('working-directory')
      try:
         wd = interpolate(to_str(wd), dict(ctx, env=env), status) if wd else ''
      except Unresolved:
         wd = ''
      cwd = os.path.normpath(os.path.join(workspace, wd)) if wd else workspace
      if self.opts.dry_run:
         self.say('-- %s%s%s' % (name, ('  [shell: %s]' % shell) if shell else '',
                                  ('  [in %s]' % wd) if wd else ''))
         if body is None:
            self.say('   (script refers to a value only known while the job runs)')
         else:
            for line in body.rstrip('\n').split('\n'):
               self.say('   | ' + line)
         return status
      if body is None:
         raise ExprError('step "%s" refers to an output that was never set' % name)
      self.say('-- %s' % name)
      fd, path = tempfile.mkstemp(dir=temp, suffix='.sh' if shell != 'python' else '.py')
      with os.fdopen(fd, 'w', encoding='utf-8', newline='\n') as f:
         f.write(body)
      cmd = shell_command(shell, path.replace(os.sep, '/'), plat)
      if cmd is None:
         self.say('   shell "%s" is not available here' % shell)
         record['outcome'] = record['conclusion'] = 'failure'
         return 'failure' if not step.get('continue-on-error') else status
      for p in files.values():
         open(p, 'a').close()
      run_env = dict(os.environ)
      run_env.update(env)
      gh = ctx['github']
      run_env.update({
         'CI': 'true', 'GITHUB_ACTIONS': 'true', 'GITHUB_WORKSPACE': workspace,
         'GITHUB_SHA': gh['sha'], 'GITHUB_REF': gh['ref'],
         'GITHUB_REF_NAME': gh['ref_name'], 'GITHUB_EVENT_NAME': gh['event_name'],
         'GITHUB_JOB': gh['job'], 'GITHUB_WORKFLOW': to_str(gh['workflow']),
         'GITHUB_REPOSITORY': gh['repository'], 'GITHUB_RUN_ID': '0',
         'GITHUB_ENV': files['env'], 'GITHUB_PATH': files['path'],
         'GITHUB_OUTPUT': files['output'],
         'GITHUB_STEP_SUMMARY': files['step_summary'],
         'RUNNER_OS': ctx['runner']['os'], 'RUNNER_TEMP': temp,
         'RUNNER_ARCH': 'X64',
      })
      if extra_path:
         run_env['PATH'] = os.pathsep.join(extra_path + [run_env.get('PATH', '')])
      open(files['output'], 'w').close()
      if cid:
         exe = [shutil.which('docker') or shutil.which('podman'), 'exec', '-w', cwd]
         for k in env.keys() | set(k for k in run_env if k.startswith(('GITHUB_', 'RUNNER_'))) | {'CI'}:
            exe += ['-e', '%s=%s' % (k, run_env[k])]
         if extra_path:
            cmd = ['sh', '-c', 'PATH="$0:$PATH"; exec "$@"',
                   ':'.join(extra_path)] + cmd
         cmd = exe + [cid] + cmd
         proc = subprocess.run(cmd)
      else:
         if not os.path.isdir(cwd):
            self.say('   working directory %s does not exist' % wd)
            proc = None
         else:
            proc = subprocess.run(cmd, cwd=cwd, env=run_env)
      ok = proc is not None and proc.returncode == 0
      self.absorb(files, ctx, record, extra_path)
      record['outcome'] = 'success' if ok else 'failure'
      if ok or step.get('continue-on-error'):
         record['conclusion'] = 'success'
         if not ok:
            self.say('   failed (continue-on-error)')
         return status
      record['conclusion'] = 'failure'
      self.say('   step failed with exit status %s'
               % (proc.returncode if proc is not None else '?'))
      return 'failure'

   def run_action(self, step, name, ctx, env, status, workspace, record):
      kind = action_kind(step['uses'])
      record['outcome'] = record['conclusion'] = 'success'
      w = {}
      for k, v in (step.get('with') or {}).items():
         try:
            w[k] = to_str(evaluate(v, dict(ctx, env=env), status))
         except Unresolved:
            w[k] = ''
      if kind == 'checkout' and w.get('repository') \
            and w['repository'].lower() != ctx['github']['repository'].lower():
         dest = os.path.join(workspace, w.get('path') or '')
         if self.opts.dry_run:
            self.say('-- %s: clone %s into %s' % (name, w['repository'], dest))
            return status
         self.say('-- %s: cloning %s' % (name, w['repository']))
         if os.path.isdir(os.path.join(dest, '.git')):
            return status
         args = ['git', 'clone', '--depth', '1']
         ref = w.get('ref') or ''
         if ref and not re.fullmatch(r'[0-9a-f]{40}', ref):
            args += ['--branch', re.sub(r'^refs/(heads|tags)/', '', ref)]
         args += ['https://github.com/%s.git' % w['repository'], dest]
         if subprocess.run(args).returncode:
            record['outcome'] = record['conclusion'] = 'failure'
            return 'failure'
         return status
      if kind in ('checkout', 'host'):
         self.say('-- %s: this checkout / host tools stand in' % name)
      elif kind == 'skip':
         self.say('-- %s: skipped (no local equivalent needed)' % name)
      else:
         self.say('-- %s: action not run (--force)' % name)
      return status

   def absorb(self, files, ctx, record, extra_path):
      for k, v in parse_env_file(files['env']).items():
         ctx['env'][k] = v
      open(files['env'], 'w').close()
      record['outputs'].update(parse_env_file(files['output']))
      with open(files['path'], encoding='utf-8', errors='replace') as f:
         for line in f:
            line = line.strip()
            if line and line not in extra_path:
               extra_path.insert(0, line)
      open(files['path'], 'w').close()


def parse_env_file(path):
   out = {}
   try:
      with open(path, encoding='utf-8', errors='replace') as f:
         lines = f.read().split('\n')
   except OSError:
      return out
   i = 0
   while i < len(lines):
      line = lines[i]
      m = re.match(r'^([^=<]+)<<(.+)$', line)
      if m:
         key, delim = m.group(1), m.group(2)
         val = []
         i += 1
         while i < len(lines) and lines[i] != delim:
            val.append(lines[i])
            i += 1
         out[key] = '\n'.join(val)
      elif '=' in line:
         k, v = line.split('=', 1)
         out[k] = v
      i += 1
   return out


def clean_copy():
   dest = tempfile.mkdtemp(prefix='ci_local.tree.')
   os.rmdir(dest)
   r = subprocess.run(['git', 'worktree', 'add', '--detach', dest, 'HEAD'],
                      cwd=ROOT, stdout=subprocess.DEVNULL)
   if r.returncode:
      sys.exit('ci_local: git worktree add failed')
   diff = subprocess.run(['git', 'diff', 'HEAD', '--binary'], cwd=ROOT,
                         stdout=subprocess.PIPE).stdout
   if diff:
      subprocess.run(['git', 'apply', '--binary', '-'], cwd=dest, input=diff,
                     check=True)
   others = subprocess.run(['git', 'ls-files', '-z', '--others',
                            '--exclude-standard'], cwd=ROOT,
                           stdout=subprocess.PIPE).stdout.split(b'\0')
   for rel in others:
      if not rel:
         continue
      rel = rel.decode('utf-8', 'surrogateescape')
      src = os.path.join(ROOT, rel)
      dst = os.path.join(dest, rel)
      os.makedirs(os.path.dirname(dst), exist_ok=True)
      shutil.copy2(src, dst)
   return dest


# --------------------------------------------------------------------------
# which
# --------------------------------------------------------------------------

SRC_EXT = ('.c', '.cpp', '.cc', '.cxx', '.m', '.mm', '.h', '.hpp', '.S', '.s',
           '.glsl', '.slang', '.inc', '.def')
_PATHISH = re.compile(r'[A-Za-z0-9_.$(){}/+-]*[A-Za-z0-9_]/[A-Za-z0-9_.$(){}/+-]*'
                      r'|[A-Za-z0-9_+-]+\.(?:c|cpp|cc|m|mm|h|py|sh|mk|list|ref)\b')
_TREE_BUILD = re.compile(r'(?:^|[\s;&|(])(?:\./configure|\./bootstrap'
                         r'|make\b[^\n]*-f\s*Makefile\.[\w.]+|griffin'
                         r'|msbuild|cmake\b|xcodebuild|qb/)', re.M)
_CHECKER = re.compile(r'(?:^|[\s;&|(/])tools/[\w.-]+\.(?:py|sh)\b', re.M)


def _strip_vars(tok):
   tok = re.sub(r'\$[({][A-Za-z0-9_]+[)}]/?', '', tok)
   tok = re.sub(r'\$[A-Za-z_][A-Za-z0-9_]*/?', '', tok)
   while tok.startswith(('../', './')):
      tok = tok[3:] if tok.startswith('../') else tok[2:]
   return tok.strip('/')


class Coverage:
   """What a job reads: files and sample directories it names, and the
   sources the Makefiles in those directories list."""

   def __init__(self):
      self.files = {}
      self.dirs = {}
      self.suffixes = {}

   def add_text(self, text, base, via, depth=0):
      for tok in set(_PATHISH.findall(text)):
         t = tok.strip('"\'(),;:')
         if not t or t.startswith(('http', '-', '$')) and '/' not in t:
            continue
         for b in (ROOT, base):
            p = os.path.normpath(os.path.join(b, _strip_vars(t) if t.startswith('$') else t))
            if not p.startswith(ROOT + os.sep):
               continue
            rel = os.path.relpath(p, ROOT).replace(os.sep, '/')
            if os.path.isfile(p):
               if rel not in self.files:
                  self.files[rel] = via
                  if depth < 2 and rel.endswith(('.sh', '.py')) \
                        and not rel.startswith('.github/workflows/') or \
                        depth < 2 and rel.startswith('.github/workflows/scripts/'):
                     self.add_file_text(p, via + ' -> ' + rel, depth + 1)
               break
            if os.path.isdir(p) and rel.count('/') >= 1:
               if rel not in self.dirs:
                  self.dirs[rel] = via
                  self.add_makefiles(p, rel, via)
               break

   def add_file_text(self, path, via, depth):
      try:
         with open(path, encoding='utf-8', errors='replace') as f:
            text = f.read()
      except OSError:
         return
      self.add_text(text, os.path.dirname(path), via, depth)

   def add_makefiles(self, path, rel, via):
      for name in sorted(os.listdir(path)):
         if not (name.startswith('Makefile') or name.endswith('.mk')
                 or name == 'CMakeLists.txt'):
            continue
         mk = os.path.join(path, name)
         if not os.path.isfile(mk):
            continue
         with open(mk, encoding='utf-8', errors='replace') as f:
            text = f.read()
         for tok in re.findall(r'[A-Za-z0-9_.$(){}/+-]+\.(?:c|cpp|cc|m|mm|S)\b', text):
            s = _strip_vars(tok)
            if s:
               self.suffixes.setdefault(s, '%s -> %s/%s' % (via, rel, name))

   def covers(self, rel):
      if rel in self.files:
         return self.files[rel]
      for d, via in self.dirs.items():
         if rel.startswith(d + '/'):
            return '%s (in %s)' % (via, d)
      for s, via in self.suffixes.items():
         if rel == s or rel.endswith('/' + s):
            return '%s lists %s' % (via, s)
      return None


def job_texts(job):
   for step in job.get('steps') or []:
      if 'run' in step:
         yield step, to_str(step.get('run')), to_str(step.get('working-directory') or '')


def paths_filter_hit(wf, rel):
   trig = wf.triggers()
   for ev in ('push', 'pull_request'):
      spec = trig.get(ev) or {}
      if not isinstance(spec, dict) or not spec.get('paths'):
         continue
      for pat in spec['paths']:
         pat = to_str(pat)
         if pat.startswith('!'):
            continue
         rx = re.escape(pat).replace(r'\*\*', '.*').replace(r'\*', '[^/]*')
         if re.fullmatch(rx, rel) or fnmatch.fnmatch(rel, pat):
            return pat
   return None


def cmd_which(paths):
   rels = []
   for p in paths:
      a = os.path.abspath(p)
      if not a.startswith(ROOT):
         a = os.path.join(ROOT, p)
      rels.append(os.path.relpath(a, ROOT).replace(os.sep, '/'))
   direct, builds, checkers = [], [], []
   for wf in load_workflows():
      for jid, job in wf.jobs.items():
         if not isinstance(job, dict) or 'steps' not in job:
            continue
         cov = Coverage()
         texts = list(job_texts(job))
         for step, text, wd in texts:
            base = os.path.join(ROOT, wd) if wd and '${{' not in wd else ROOT
            cov.add_text(text + '\n' + wd, base, 'named in a run: step')
         plat = runs_on_os(job.get('runs-on')) or ('matrix' if '${{' in to_str(job.get('runs-on')) else '?')
         tag = '%s --job %s' % (wf.file, jid)
         hits = []
         for rel in rels:
            if rel == '.github/workflows/' + wf.file:
               hits.append('%s: is this workflow' % rel)
               continue
            pat = paths_filter_hit(wf, rel)
            if pat:
               hits.append('%s: paths filter %s' % (rel, pat))
               continue
            why = cov.covers(rel)
            if why:
               hits.append('%s: %s' % (rel, why))
         alltext = '\n'.join(t for _, t, _ in texts)
         if hits:
            direct.append((tag, plat, hits))
         elif any(r.endswith(SRC_EXT) for r in rels) and _TREE_BUILD.search(alltext):
            builds.append((tag, plat))
         elif _CHECKER.search(alltext) and not re.search(r'(?:^|\s)(?:make|gcc|cc|clang)\b', alltext, re.M):
            checkers.append((tag, plat))
   def show(title, rows):
      if not rows:
         return
      print(title)
      for row in rows:
         print('  %-58s %s' % (row[0], row[1]))
         for h in (row[2] if len(row) > 2 else []):
            print('      ' + h)
      print()
   show('Jobs that name these files (run all of them):', direct)
   show('Full-tree builds (run those for the platforms you can):', builds)
   show('Tree-wide checks driven by tools/ scripts (run all of them):', checkers)
   print('Run one with: tools/ci_local.py run <workflow> --job <job>')


def cmd_list():
   for wf in load_workflows():
      print('%s  (%s)' % (wf.file, wf.name))
      for jid, job in wf.jobs.items():
         if not isinstance(job, dict):
            continue
         ctx = base_context(wf, jid, ROOT, tempfile.gettempdir())
         try:
            n = len(matrix_combos(job, ctx))
         except (ExprError, Unresolved):
            n = '?'
         ro = to_str(job.get('runs-on'))
         extra = ' container' if job.get('container') else ''
         print('  %-40s %-28s matrix %s%s' % (jid, ro, n, extra))


# --------------------------------------------------------------------------
# selftest
# --------------------------------------------------------------------------

def _same(a, b):
   if isinstance(a, float) and isinstance(b, float) and a != a and b != b:
      return True
   if type(a) is not type(b):
      return False
   if isinstance(a, dict):
      return list(a.keys()) == list(b.keys()) and all(_same(a[k], b[k]) for k in a)
   if isinstance(a, list):
      return len(a) == len(b) and all(_same(x, y) for x, y in zip(a, b))
   return a == b


def _first_diff(a, b, path='$'):
   if isinstance(a, dict) and isinstance(b, dict):
      for k in list(a) + [k for k in b if k not in a]:
         if k not in a or k not in b:
            return '%s.%s present on one side only' % (path, k)
         d = _first_diff(a[k], b[k], '%s.%s' % (path, k))
         if d:
            return d
      return None
   if isinstance(a, list) and isinstance(b, list):
      for n, (x, y) in enumerate(zip(a, b)):
         d = _first_diff(x, y, '%s[%d]' % (path, n))
         if d:
            return d
      if len(a) != len(b):
         return '%s length %d != %d' % (path, len(a), len(b))
      return None
   if not _same(a, b):
      return '%s: %r != %r' % (path, a, b)
   return None


YAML_CASES = [
   ('a: 1\nb: [x, "y z", 3]\nc: { d: on, e: 10.4 }\n', None),
   ('k: |\n  one\n    two\n\n  three\n\nn: x\n', None),
   ('k: |-\n  a\n  b\n', None),
   ('s:\n- a\n- b: c\n  d: e\n-\n  - f\n', None),
   ("q: 'it''s'\nr: \"a\\tb\"\nu: plain # comment\n", None),
   ('m: >\n  folded\n  text\n\n  para\n', None),
   ('x: "multi\n  line"\ny: 0o7\nz: 017\nw: ~\n', None),
]

EXPR_CASES = [
   ("'a' == 'A'", True),
   ("1 == '1'", True),
   ("null == 0", True),
   ("matrix.x == ''", True),
   ("startsWith(matrix.name, 'aarch64') || matrix.name == 'armv7'", True),
   ("github.event.pull_request.number || github.ref", 'refs/heads/master'),
   ("github.event_name == 'release' && '0' || '1'", '1'),
   ("matrix.n == '' && 15 || 40", 40.0),
   ("!(1 == 2) && contains('abc', 'B')", True),
   ("format('{0}-{1}', 'a', 2)", 'a-2'),
   ("fromJSON('[1,2]')[1]", 2),
   ("strategy.job-index", 0),
]


FIXTURE = """\
name: fixture
on: [workflow_dispatch]
env:
  TOP: top-${{ github.event_name }}
jobs:
  first:
    runs-on: ubuntu-latest
    outputs:
      word: ${{ steps.make.outputs.word }}
    strategy:
      matrix:
        a: [x, y]
        include:
          - a: y
            extra: more
    steps:
      - uses: actions/checkout@v4
      - id: make
        run: |
          echo "word=hello-${{ matrix.a }}" >> "$GITHUB_OUTPUT"
          echo "FROMENV=set" >> "$GITHUB_ENV"
          mkdir -p "$RUNNER_TEMP/bin"
          printf '#!/bin/sh\\necho on-path\\n' > "$RUNNER_TEMP/bin/fixture_tool"
          chmod +x "$RUNNER_TEMP/bin/fixture_tool"
          echo "$RUNNER_TEMP/bin" >> "$GITHUB_PATH"
          mkdir -p sub
      - shell: bash
        working-directory: sub
        run: |
          test "$FROMENV" = set
          test "$TOP" = top-workflow_dispatch
          test "$(basename "$PWD")" = sub
          test "$(fixture_tool)" = on-path
          test "${{ matrix.a }}" = x || test "${{ matrix.extra }}" = more
      - if: steps.make.outputs.word == 'hello-y'
        run: touch "$GITHUB_WORKSPACE/only_y"
      - continue-on-error: true
        run: exit 3
  second:
    needs: first
    runs-on: ubuntu-latest
    steps:
      - run: test "${{ needs.first.outputs.word }}" = hello-y
  third:
    needs: second
    runs-on: ubuntu-latest
    steps:
      - shell: bash
        run: false | true
  fourth:
    needs: third
    runs-on: ubuntu-latest
    steps:
      - run: touch "$GITHUB_WORKSPACE/must_not_exist"
"""


def _run_fixture():
   """Runs FIXTURE on this machine: environment, path and output files,
   matrix include, needs, continue-on-error and bash's pipefail."""
   ws = tempfile.mkdtemp(prefix='ci_local.fixture.')
   try:
      path = os.path.join(ws, 'fixture.yml')
      with open(path, 'w', encoding='utf-8') as f:
         f.write(FIXTURE)
      wf = Workflow(path)

      class Opts:
         job = None
         matrix = {}
         skip = []
         dry_run = False
         host = True
         force = True
      out = []
      r = Runner(Opts())
      r.say = out.append
      ok = r.run_workflow(wf, ws)
      want_failed = ['fixture.yml / third']
      problems = []
      if ok or r.failed != want_failed:
         problems.append('failed jobs %r, expected %r' % (r.failed, want_failed))
      if not os.path.exists(os.path.join(ws, 'only_y')):
         problems.append('the matrix y step did not run')
      if os.path.exists(os.path.join(ws, 'must_not_exist')):
         problems.append('a job ran after the job it needs failed')
      for p in problems:
         print('FAIL run fixture: ' + p)
      if problems:
         print('\n'.join('  ' + l for l in out))
      return len(problems)
   finally:
      shutil.rmtree(ws, ignore_errors=True)


def cmd_selftest():
   bad = 0
   try:
      import yaml
   except ImportError:
      yaml = None
      print('PyYAML is not installed: parser not compared against it')
   for text, _ in YAML_CASES:
      if yaml:
         want = yaml.safe_load(text)
         got = yaml_load(text)
         if not _same(got, want):
            print('FAIL yaml case %r: %s' % (text, _first_diff(got, want)))
            bad += 1
   wfs = []
   for name in sorted(os.listdir(WF_DIR)):
      if not name.endswith(('.yml', '.yaml')):
         continue
      path = os.path.join(WF_DIR, name)
      try:
         wf = Workflow(path)
      except Exception as e:
         print('FAIL %s: does not parse: %s' % (name, e))
         bad += 1
         continue
      wfs.append(wf)
      if yaml:
         with open(path, encoding='utf-8') as f:
            want = yaml.safe_load(f)
         if not _same(wf.data, want):
            print('FAIL %s: parses differently from PyYAML: %s'
                  % (name, _first_diff(wf.data, want)))
            bad += 1
   ctx = {'matrix': {'x': '', 'name': 'aarch64-linux', 'n': 'x'},
          'github': {'event': {}, 'ref': 'refs/heads/master',
                     'event_name': 'workflow_dispatch'},
          'strategy': {'job-index': 0}}
   for src, want in EXPR_CASES:
      try:
         got = Expr(ctx, 'success').eval(src)
      except Exception as e:
         got = e
      if not (got == want and type(got) is type(want)):
         print('FAIL expression %s: %r, expected %r' % (src, got, want))
         bad += 1

   class Opts:
      job = None
      matrix = {}
      skip = []
      dry_run = True
      host = False
      force = True
   devnull = open(os.devnull, 'w')
   for wf in wfs:
      r = Runner(Opts())
      r.say = lambda msg: devnull.write(msg)
      try:
         r.run_workflow(wf, ROOT)
      except Exception as e:
         print('FAIL %s: dry run: %s: %s' % (wf.file, type(e).__name__, e))
         bad += 1
   devnull.close()
   if shutil.which('bash'):
      bad += _run_fixture()
   else:
      print('bash is not on PATH: run fixture skipped')
   print('%d workflows, %s' % (len(wfs), 'ok' if not bad else '%d failures' % bad))
   return 1 if bad else 0


# --------------------------------------------------------------------------

def main(argv):
   if not argv or argv[0] in ('-h', '--help', 'help'):
      print(__doc__.strip())
      return 0
   cmd, rest = argv[0], argv[1:]
   if cmd == 'list':
      cmd_list()
      return 0
   if cmd == 'which':
      if not rest:
         sys.exit('ci_local: which needs at least one path')
      cmd_which(rest)
      return 0
   if cmd == 'selftest':
      return cmd_selftest()
   if cmd != 'run':
      sys.exit('ci_local: unknown command "%s" (list, which, run, selftest)' % cmd)

   class Opts:
      job = None
      matrix = {}
      skip = []
      dry_run = False
      clean = False
      host = False
      force = False
   opts = Opts()
   opts.matrix = {}
   opts.skip = []
   target = None
   i = 0
   while i < len(rest):
      a = rest[i]
      if a in ('--job', '--matrix', '--skip'):
         if i + 1 >= len(rest):
            sys.exit('ci_local: %s needs a value' % a)
         v = rest[i + 1]
         i += 2
         if a == '--job':
            opts.job = v
         elif a == '--skip':
            opts.skip.append(v)
         else:
            if '=' not in v:
               sys.exit('ci_local: --matrix takes KEY=VALUE')
            k, val = v.split('=', 1)
            opts.matrix[k] = val
         continue
      if a == '--dry-run':
         opts.dry_run = True
      elif a == '--clean-copy':
         opts.clean = True
      elif a == '--host':
         opts.host = True
      elif a == '--force':
         opts.force = True
      elif a.startswith('-'):
         sys.exit('ci_local: unknown option %s' % a)
      elif target is None:
         target = a
      else:
         sys.exit('ci_local: one workflow at a time')
      i += 1
   if target is None:
      sys.exit('ci_local: run needs a workflow')
   wf = find_workflow(target)
   workspace = ROOT
   if opts.clean and not opts.dry_run:
      workspace = clean_copy()
      print('ci_local: running in %s' % workspace)
   try:
      ok = Runner(opts).run_workflow(wf, workspace)
   finally:
      if workspace != ROOT:
         subprocess.run(['git', 'worktree', 'remove', '--force', workspace],
                        cwd=ROOT)
   return 0 if ok else 1


if __name__ == '__main__':
   if hasattr(__import__('signal'), 'SIGPIPE'):
      import signal
      signal.signal(signal.SIGPIPE, signal.SIG_DFL)
   sys.exit(main(sys.argv[1:]))
