#include "rc_internal.h"

#include "rc_compat.h"

#include <ctype.h>

/* special formats only used by rc_richpresence_display_part_t.display_type. must not overlap other RC_FORMAT values */
enum {
  RC_FORMAT_STRING = 101,
  RC_FORMAT_LOOKUP = 102,
  RC_FORMAT_UNKNOWN_MACRO = 103
};

static rc_memref_value_t* rc_alloc_helper_variable_memref_value(const char* memaddr, int memaddr_len, rc_parse_state_t* parse) {
  const char* end;
  rc_value_t* variable;
  unsigned address;
  char size;

  /* single memory reference lookups without a modifier flag can be handled without a variable */
  end = memaddr;
  if (rc_parse_memref(&end, &size, &address) == RC_OK) {
    /* make sure the entire memaddr was consumed. if not, there's an operator and it's a comparison, not a memory reference */
    if (end == &memaddr[memaddr_len]) {
      /* if it's not a derived size, we can reference the memref directly */
      if (rc_memref_shared_size(size) == size)
        return &rc_alloc_memref(parse, address, size, 0)->value;
    }
  }

  /* not a simple memory reference, need to create a variable */
  variable = rc_alloc_helper_variable(memaddr, memaddr_len, parse);
  if (!variable)
    return NULL;

  return &variable->value;
}

static const char* rc_parse_line(const char* line, const char** end, rc_parse_state_t* parse) {
  const char* nextline;
  const char* endline;

  /* get a single line */
  nextline = line;
  while (*nextline && *nextline != '\n')
    ++nextline;

  /* if a trailing comment marker (//) exists, the line stops there */
  endline = line;
  while (endline < nextline && (endline[0] != '/' || endline[1] != '/' || (endline > line && endline[-1] == '\\')))
    ++endline;

  if (endline == nextline) {
    /* trailing whitespace on a line without a comment marker may be significant, just remove the line ending */
    if (endline > line && endline[-1] == '\r')
      --endline;
  } else {
    /* remove trailing whitespace before the comment marker */
    while (endline > line && isspace((int)((unsigned char*)endline)[-1]))
      --endline;
  }

  /* point end at the first character to ignore, it makes subtraction for length easier */
  *end = endline;

  /* tally the line */
  ++parse->lines_read;

  /* skip the newline character so we're pointing at the next line */
  if (*nextline == '\n')
    ++nextline;

  return nextline;
}

static rc_richpresence_display_t* rc_parse_richpresence_display_internal(const char* line, const char* endline, rc_parse_state_t* parse, rc_richpresence_lookup_t* first_lookup) {
  rc_richpresence_display_t* self;
  rc_richpresence_display_part_t* part;
  rc_richpresence_display_part_t** next;
  rc_richpresence_lookup_t* lookup;
  const char* ptr;
  const char* in;
  char* out;

  if (endline - line < 1) {
    parse->offset = RC_MISSING_DISPLAY_STRING;
    return 0;
  }

  {
    self = RC_ALLOC(rc_richpresence_display_t, parse);
    memset(self, 0, sizeof(rc_richpresence_display_t));
    next = &self->display;
  }

  /* break the string up on macros: text @macro() moretext */
  do {
    ptr = line;
    while (ptr < endline) {
      if (*ptr == '@' && (ptr == line || ptr[-1] != '\\')) /* ignore escaped @s */
        break;

      ++ptr;
    }

    if (ptr > line) {
      part = RC_ALLOC(rc_richpresence_display_part_t, parse);
      memset(part, 0, sizeof(rc_richpresence_display_part_t));
      *next = part;
      next = &part->next;

      /* handle string part */
      part->display_type = RC_FORMAT_STRING;
      part->text = rc_alloc_str(parse, line, (int)(ptr - line));
      if (part->text) {
        /* remove backslashes used for escaping */
        in = part->text;
        while (*in && *in != '\\')
          ++in;

        if (*in == '\\') {
          out = (char*)in++;
          while (*in) {
            *out++ = *in++;
            if (*in == '\\')
              ++in;
          }
          *out = '\0';
        }
      }
    }

    if (*ptr == '@') {
      /* handle macro part */
      line = ++ptr;
      while (ptr < endline && *ptr != '(')
        ++ptr;

      if (ptr == endline) {
        parse->offset = RC_MISSING_VALUE;
        return 0;
      }

      if (ptr > line) {
        /* find the lookup and hook it up */
        lookup = first_lookup;
        while (lookup) {
          if (strncmp(lookup->name, line, ptr - line) == 0 && lookup->name[ptr - line] == '\0') {
            part = RC_ALLOC(rc_richpresence_display_part_t, parse);
            *next = part;
            next = &part->next;

            part->text = lookup->name;
            part->lookup = lookup;
            part->display_type = lookup->format;

            in = line;
            line = ++ptr;
            while (ptr < endline && *ptr != ')')
              ++ptr;
            if (*ptr == ')') {
              part->value = rc_alloc_helper_variable_memref_value(line, (int)(ptr-line), parse);
              if (parse->offset < 0)
                return 0;
              ++ptr;
            }
            else {
              /* non-terminated macro, dump the macro and the remaining portion of the line */
              --in; /* already skipped over @ */
              part->display_type = RC_FORMAT_STRING;
              part->text = rc_alloc_str(parse, in, (int)(ptr - in));
            }

            break;
          }

          lookup = lookup->next;
        }

        if (!lookup) {
          part = RC_ALLOC(rc_richpresence_display_part_t, parse);
          memset(part, 0, sizeof(rc_richpresence_display_part_t));
          *next = part;
          next = &part->next;

          /* find the closing parenthesis */
          while (ptr < endline && *ptr != ')')
            ++ptr;
          if (*ptr == ')')
            ++ptr;

          /* assert: the allocated string is going to be smaller than the memory used for the parameter of the macro */
          part->display_type = RC_FORMAT_UNKNOWN_MACRO;
          part->text = rc_alloc_str(parse, line, (int)(ptr - line));
        }
      }
    }

    line = ptr;
  } while (line < endline);

  *next = 0;

  return self;
}

static int rc_richpresence_lookup_item_count(rc_richpresence_lookup_item_t* item)
{
  if (item == NULL)
    return 0;

  return (rc_richpresence_lookup_item_count(item->left) + rc_richpresence_lookup_item_count(item->right) + 1);
}

static void rc_rebalance_richpresence_lookup_get_items(rc_richpresence_lookup_item_t* root,
    rc_richpresence_lookup_item_t** items, int* index)
{
  if (root->left != NULL)
    rc_rebalance_richpresence_lookup_get_items(root->left, items, index);

  items[*index] = root;
  ++(*index);

  if (root->right != NULL)
    rc_rebalance_richpresence_lookup_get_items(root->right, items, index);
}

static void rc_rebalance_richpresence_lookup_rebuild(rc_richpresence_lookup_item_t** root,
    rc_richpresence_lookup_item_t** items, int first, int last)
{
  int mid = (first + last) / 2;
  rc_richpresence_lookup_item_t* item = items[mid];
  *root = item;

  if (mid == first)
    item->left = NULL;
  else
    rc_rebalance_richpresence_lookup_rebuild(&item->left, items, first, mid - 1);

  if (mid == last)
    item->right = NULL;
  else
    rc_rebalance_richpresence_lookup_rebuild(&item->right, items, mid + 1, last);
}

static void rc_rebalance_richpresence_lookup(rc_richpresence_lookup_item_t** root, rc_parse_state_t* parse)
{
  rc_richpresence_lookup_item_t** items;
  rc_scratch_buffer_t* buffer;
  const int alignment = sizeof(rc_richpresence_lookup_item_t*);
  int index;
  int size;

  /* don't bother rebalancing one or two items */
  int count = rc_richpresence_lookup_item_count(*root);
  if (count < 3)
    return;

  /* allocate space for the flattened list - prefer scratch memory if available */
  size = count * sizeof(rc_richpresence_lookup_item_t*);
  buffer = &parse->scratch.buffer;
  do {
    const int aligned_offset = (buffer->offset + alignment - 1) & ~(alignment - 1);
    const int remaining = sizeof(buffer->buffer) - aligned_offset;

    if (remaining >= size) {
      items = (rc_richpresence_lookup_item_t**)&buffer->buffer[aligned_offset];
      break;
    }

    buffer = buffer->next;
    if (buffer == NULL) {
      /* could not find large enough block of scratch memory; allocate. if allocation fails,
       * we can still use the unbalanced tree, so just bail out */
      items = (rc_richpresence_lookup_item_t**)malloc(size);
      if (items == NULL)
        return;

      break;
    }
  } while (1);

  /* flatten the list */
  index = 0;
  rc_rebalance_richpresence_lookup_get_items(*root, items, &index);

  /* and rebuild it as a balanced tree */
  rc_rebalance_richpresence_lookup_rebuild(root, items, 0, count - 1);

  if (buffer == NULL)
    free(items);
}

static void rc_insert_richpresence_lookup_item(rc_richpresence_lookup_t* lookup,
    unsigned first, unsigned last, const char* label, int label_len, rc_parse_state_t* parse)
{
  rc_richpresence_lookup_item_t** next;
  rc_richpresence_lookup_item_t* item;

  next = &lookup->root;
  while ((item = *next) != NULL) {
    if (first > item->last) {
      if (first == item->last + 1 &&
          strncmp(label, item->label, label_len) == 0 && item->label[label_len] == '\0') {
        item->last = last;
        return;
      }

      next = &item->right;
    }
    else if (last < item->first) {
      if (last == item->first - 1 &&
          strncmp(label, item->label, label_len) == 0 && item->label[label_len] == '\0') {
        item->first = first;
        return;
      }

      next = &item->left;
    }
    else {
      parse->offset = RC_DUPLICATED_VALUE;
      return;
    }
  }

  item = RC_ALLOC_SCRATCH(rc_richpresence_lookup_item_t, parse);
  item->first = first;
  item->last = last;
  item->label = rc_alloc_str(parse, label, label_len);
  item->left = item->right = NULL;

  *next = item;
}

static const char* rc_parse_richpresence_lookup(rc_richpresence_lookup_t* lookup, const char* nextline, rc_parse_state_t* parse)
{
  const char* line;
  const char* endline;
  const char* label;
  char* endptr = 0;
  unsigned first, last;
  int base;

  do
  {
    line = nextline;
    nextline = rc_parse_line(line, &endline, parse);

    if (endline - line < 2) {
      /* ignore full line comments inside a lookup */
      if (line[0] == '/' && line[1] == '/')
        continue;

      /* empty line indicates end of lookup */
      if (lookup->root)
        rc_rebalance_richpresence_lookup(&lookup->root, parse);
      break;
    }

    /* "*=XXX" specifies default label if lookup does not provide a mapping for the value */
    if (line[0] == '*' && line[1] == '=') {
      line += 2;
      lookup->default_label = rc_alloc_str(parse, line, (int)(endline - line));
      continue;
    }

    label = line;
    while (label < endline && *label != '=')
      ++label;

    if (label == endline) {
      parse->offset = RC_MISSING_VALUE;
      break;
    }
    ++label;

    do {
      /* get the value for the mapping */
      if (line[0] == '0' && line[1] == 'x') {
        line += 2;
        base = 16;
      } else {
        base = 10;
      }

      first = (unsigned)strtoul(line, &endptr, base);

      /* check for a range */
      if (*endptr != '-') {
        /* no range, just set last to first */
        last = first;
      }
      else {
        /* range, get last value */
        line = endptr + 1;

        if (line[0] == '0' && line[1] == 'x') {
          line += 2;
          base = 16;
        } else {
          base = 10;
        }

        last = (unsigned)strtoul(line, &endptr, base);
      }

      /* ignore spaces after the number - was previously ignored as string was split on equals */
      while (*endptr == ' ')
        ++endptr;

      /* if we've found the equal sign, this is the last item */
      if (*endptr == '=') {
        rc_insert_richpresence_lookup_item(lookup, first, last, label, (int)(endline - label), parse);
        break;
      }

      /* otherwise, if it's not a comma, it's an error */
      if (*endptr != ',') {
        parse->offset = RC_INVALID_CONST_OPERAND;
        break;
      }

      /* insert the current item and continue scanning the next one */
      rc_insert_richpresence_lookup_item(lookup, first, last, label, (int)(endline - label), parse);
      line = endptr + 1;
    } while (line < endline);

  } while (parse->offset > 0);

  return nextline;
}

void rc_parse_richpresence_internal(rc_richpresence_t* self, const char* script, rc_parse_state_t* parse) {
  rc_richpresence_display_t** nextdisplay;
  rc_richpresence_lookup_t* firstlookup = NULL;
  rc_richpresence_lookup_t** nextlookup = &firstlookup;
  rc_richpresence_lookup_t* lookup;
  rc_trigger_t* trigger;
  char format[64];
  const char* display = 0;
  const char* line;
  const char* nextline;
  const char* endline;
  const char* ptr;
  int hasdisplay = 0;
  int display_line = 0;
  int chars;

  /* special case for empty script to return 1 line read */
  if (!*script) {
     parse->lines_read = 1;
     parse->offset = RC_MISSING_DISPLAY_STRING;
     return;
  }

  /* first pass: process macro initializers */
  line = script;
  while (*line) {
    nextline = rc_parse_line(line, &endline, parse);
    if (strncmp(line, "Lookup:", 7) == 0) {
      line += 7;

      lookup = RC_ALLOC_SCRATCH(rc_richpresence_lookup_t, parse);
      lookup->name = rc_alloc_str(parse, line, (int)(endline - line));
      lookup->format = RC_FORMAT_LOOKUP;
      lookup->root = NULL;
      lookup->default_label = "";
      *nextlookup = lookup;
      nextlookup = &lookup->next;

      nextline = rc_parse_richpresence_lookup(lookup, nextline, parse);
      if (parse->offset < 0)
        return;

    } else if (strncmp(line, "Format:", 7) == 0) {
      line += 7;

      lookup = RC_ALLOC_SCRATCH(rc_richpresence_lookup_t, parse);
      lookup->name = rc_alloc_str(parse, line, (int)(endline - line));
      lookup->root = NULL;
      lookup->default_label = "";
      *nextlookup = lookup;
      nextlookup = &lookup->next;

      line = nextline;
      nextline = rc_parse_line(line, &endline, parse);
      if (parse->buffer && strncmp(line, "FormatType=", 11) == 0) {
        line += 11;

        chars = (int)(endline - line);
        if (chars > 63)
          chars = 63;
        memcpy(format, line, chars);
        format[chars] = '\0';

        lookup->format = (unsigned short)rc_parse_format(format);
      } else {
        lookup->format = RC_FORMAT_VALUE;
      }
    } else if (strncmp(line, "Display:", 8) == 0) {
      display = nextline;
      display_line = parse->lines_read;

      do {
        line = nextline;
        nextline = rc_parse_line(line, &endline, parse);
      } while (*line == '?');
    }

    line = nextline;
  }

  *nextlookup = 0;
  self->first_lookup = firstlookup;

  nextdisplay = &self->first_display;

  /* second pass, process display string*/
  if (display) {
    /* point the parser back at the display strings */
    int lines_read = parse->lines_read;
    parse->lines_read = display_line;
    line = display;

    nextline = rc_parse_line(line, &endline, parse);

    while (*line == '?') {
      /* conditional display: ?trigger?string */
      ptr = ++line;
      while (ptr < endline && *ptr != '?')
        ++ptr;

      if (ptr < endline) {
        *nextdisplay = rc_parse_richpresence_display_internal(ptr + 1, endline, parse, firstlookup);
        if (parse->offset < 0)
          return;
        trigger = &((*nextdisplay)->trigger);
        rc_parse_trigger_internal(trigger, &line, parse);
        trigger->memrefs = 0;
        if (parse->offset < 0)
          return;
        if (parse->buffer)
          nextdisplay = &((*nextdisplay)->next);
      }

      line = nextline;
      nextline = rc_parse_line(line, &endline, parse);
    }

    /* non-conditional display: string */
    *nextdisplay = rc_parse_richpresence_display_internal(line, endline, parse, firstlookup);
    if (*nextdisplay) {
      hasdisplay = 1;
      nextdisplay = &((*nextdisplay)->next);
    }

    /* restore the parser state */
    parse->lines_read = lines_read;
  }

  /* finalize */
  *nextdisplay = 0;

  if (!hasdisplay && parse->offset > 0) {
    parse->offset = RC_MISSING_DISPLAY_STRING;
  }
}

int rc_richpresence_size_lines(const char* script, int* lines_read) {
  rc_richpresence_t* self;
  rc_parse_state_t parse;
  rc_memref_t* first_memref;
  rc_value_t* variables;
  rc_init_parse_state(&parse, 0, 0, 0);
  rc_init_parse_state_memrefs(&parse, &first_memref);
  rc_init_parse_state_variables(&parse, &variables);

  self = RC_ALLOC(rc_richpresence_t, &parse);
  rc_parse_richpresence_internal(self, script, &parse);

  if (lines_read)
    *lines_read = parse.lines_read;

  rc_destroy_parse_state(&parse);
  return parse.offset;
}

int rc_richpresence_size(const char* script) {
  return rc_richpresence_size_lines(script, NULL);
}

rc_richpresence_t* rc_parse_richpresence(void* buffer, const char* script, lua_State* L, int funcs_ndx) {
  rc_richpresence_t* self;
  rc_parse_state_t parse;

  if (!buffer || !script)
    return NULL;

  rc_init_parse_state(&parse, buffer, L, funcs_ndx);

  self = RC_ALLOC(rc_richpresence_t, &parse);
  rc_init_parse_state_memrefs(&parse, &self->memrefs);
  rc_init_parse_state_variables(&parse, &self->variables);

  rc_parse_richpresence_internal(self, script, &parse);

  rc_destroy_parse_state(&parse);
  return (parse.offset >= 0) ? self : NULL;
}

void rc_update_richpresence(rc_richpresence_t* richpresence, rc_peek_t peek, void* peek_ud, lua_State* L) {
  rc_richpresence_display_t* display;

  rc_update_memref_values(richpresence->memrefs, peek, peek_ud);
  rc_update_variables(richpresence->variables, peek, peek_ud, L);

  for (display = richpresence->first_display; display; display = display->next) {
    if (display->trigger.has_required_hits)
      rc_test_trigger(&display->trigger, peek, peek_ud, L);
  }
}

static int rc_evaluate_richpresence_display(rc_richpresence_display_part_t* part, char* buffer, unsigned buffersize)
{
  rc_richpresence_lookup_item_t* item;
  char tmp[256];
  char* ptr = buffer;
  const char* text;
  size_t chars;
  unsigned value;

  *ptr = '\0';
  while (part) {
    switch (part->display_type) {
      case RC_FORMAT_STRING:
        text = part->text;
        chars = strlen(text);
        break;

      case RC_FORMAT_LOOKUP:
        value = part->value->value;
        text = part->lookup->default_label;
        item = part->lookup->root;
        while (item) {
          if (value > item->last) {
            item = item->right;
          }
          else if (value < item->first) {
            item = item->left;
          }
          else {
            text = item->label;
            break;
          }
        }

        chars = strlen(text);
        break;

      case RC_FORMAT_UNKNOWN_MACRO:
        chars = snprintf(tmp, sizeof(tmp), "[Unknown macro]%s", part->text);
        text = tmp;
        break;

      default:
        value = part->value->value;
        chars = rc_format_value(tmp, sizeof(tmp), value, part->display_type);
        text = tmp;
        break;
    }

    if (chars > 0 && buffersize > 0) {
      if ((unsigned)chars >= buffersize) {
        /* prevent write past end of buffer */
        memcpy(ptr, text, buffersize - 1);
        ptr[buffersize - 1] = '\0';
        buffersize = 0;
      }
      else {
        memcpy(ptr, text, chars);
        ptr[chars] = '\0';
        buffersize -= (unsigned)chars;
      }
    }

    ptr += chars;
    part = part->next;
  }

  return (int)(ptr - buffer);
}

int rc_get_richpresence_display_string(rc_richpresence_t* richpresence, char* buffer, unsigned buffersize, rc_peek_t peek, void* peek_ud, lua_State* L) {
  rc_richpresence_display_t* display;

  for (display = richpresence->first_display; display; display = display->next) {
    /* if we've reached the end of the condition list, process it */
    if (!display->next)
      return rc_evaluate_richpresence_display(display->display, buffer, buffersize);

    /* triggers with required hits will be updated in rc_update_richpresence */
    if (!display->trigger.has_required_hits)
      rc_test_trigger(&display->trigger, peek, peek_ud, L);

    /* if we've found a valid condition, process it */
    if (display->trigger.state == RC_TRIGGER_STATE_TRIGGERED)
      return rc_evaluate_richpresence_display(display->display, buffer, buffersize);
  }

  buffer[0] = '\0';
  return 0;
}

int rc_evaluate_richpresence(rc_richpresence_t* richpresence, char* buffer, unsigned buffersize, rc_peek_t peek, void* peek_ud, lua_State* L) {
  rc_update_richpresence(richpresence, peek, peek_ud, L);
  return rc_get_richpresence_display_string(richpresence, buffer, buffersize, peek, peek_ud, L);
}
