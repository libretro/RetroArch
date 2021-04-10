#include "internal.h"

#include "compat.h"

#include <ctype.h>

/* special formats only used by rc_richpresence_display_part_t.display_type. must not overlap other RC_FORMAT values */
enum {
  RC_FORMAT_STRING = 101,
  RC_FORMAT_LOOKUP = 102,
  RC_FORMAT_UNKNOWN_MACRO = 103
};

static const char* rc_parse_line(const char* line, const char** end) {
  const char* nextline;
  const char* endline;

  /* get a single line */
  nextline = line;
  while (*nextline && *nextline != '\n')
    ++nextline;

  /* find a trailing comment marker (//) */
  endline = line;
  while (endline < nextline && (endline[0] != '/' || endline[1] != '/' || (endline > line && endline[-1] == '\\')))
    ++endline;

  /* remove trailing whitespace */
  if (endline == nextline) {
    if (endline > line && endline[-1] == '\r')
      --endline;
  } else {
    while (endline > line && isspace((unsigned char)endline[-1]))
      --endline;
  }

  /* end is pointing at the first character to ignore - makes subtraction for length easier */
  *end = endline;

  if (*nextline == '\n')
    ++nextline;
  return nextline;
}

static rc_richpresence_display_t* rc_parse_richpresence_display_internal(const char* line, const char* endline, rc_parse_state_t* parse, rc_richpresence_t* richpresence) {
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
        if (!parse->buffer) {
          /* just calculating size, can't confirm lookup exists */
          part = RC_ALLOC(rc_richpresence_display_part_t, parse);

          in = line;
          line = ++ptr;
          while (ptr < endline && *ptr != ')')
            ++ptr;
          if (*ptr == ')') {
            rc_parse_value_internal(&part->value, &line, parse);
            if (parse->offset < 0)
              return 0;
            ++ptr;
          } else {
            /* no closing parenthesis - allocate space for the invalid string */
            --in; /* already skipped over @ */
            rc_alloc_str(parse, line, (int)(ptr - in));
          }

        } else {
          /* find the lookup and hook it up */
          lookup = richpresence->first_lookup;
          while (lookup) {
            if (strncmp(lookup->name, line, ptr - line) == 0 && lookup->name[ptr - line] == '\0') {
              part = RC_ALLOC(rc_richpresence_display_part_t, parse);
              *next = part;
              next = &part->next;

              part->text = lookup->name;
              part->first_lookup_item = lookup->first_item;
              part->display_type = lookup->format;

              in = line;
              line = ++ptr;
              while (ptr < endline && *ptr != ')')
                ++ptr;
              if (*ptr == ')') {
                rc_parse_value_internal(&part->value, &line, parse);
                part->value.memrefs = 0;
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
    }

    line = ptr;
  } while (line < endline);

  *next = 0;

  return self;
}

static const char* rc_parse_richpresence_lookup(rc_richpresence_lookup_t* lookup, const char* nextline, rc_parse_state_t* parse)
{
  rc_richpresence_lookup_item_t** next;
  rc_richpresence_lookup_item_t* item;
  char number[64];
  const char* line;
  const char* endline;
  const char* defaultlabel = 0;
  char* endptr = 0;
  unsigned key;
  unsigned chars;

  next = &lookup->first_item;

  do
  {
    line = nextline;
    nextline = rc_parse_line(line, &endline);

    if (endline - line < 2)
      break;

    chars = 0;
    while (chars < (sizeof(number) - 1) && line + chars < endline && line[chars] != '=') {
      number[chars] = line[chars];
      ++chars;
    }
    number[chars] = '\0';

    if (line[chars] == '=') {
      line += chars + 1;

      if (chars == 1 && number[0] == '*') {
        defaultlabel = rc_alloc_str(parse, line, (int)(endline - line));
        continue;
      }

      if (number[0] == '0' && number[1] == 'x')
        key = strtoul(&number[2], &endptr, 16);
      else
        key = strtoul(&number[0], &endptr, 10);

      if (*endptr && !isspace((unsigned char)*endptr)) {
        parse->offset = RC_INVALID_CONST_OPERAND;
        return nextline;
      }

      item = RC_ALLOC(rc_richpresence_lookup_item_t, parse);
      item->value = key;
      item->label = rc_alloc_str(parse, line, (int)(endline - line));
      *next = item;
      next = &item->next_item;
    }
  } while (1);

  if (!defaultlabel)
    defaultlabel = rc_alloc_str(parse, "", 0);

  item = RC_ALLOC(rc_richpresence_lookup_item_t, parse);
  item->value = 0;
  item->label = defaultlabel;
  item->next_item = 0;
  *next = item;

  return nextline;
}

void rc_parse_richpresence_internal(rc_richpresence_t* self, const char* script, rc_parse_state_t* parse) {
  rc_richpresence_display_t** nextdisplay;
  rc_richpresence_lookup_t** nextlookup;
  rc_richpresence_lookup_t* lookup;
  rc_trigger_t* trigger;
  char format[64];
  const char* display = 0;
  const char* line;
  const char* nextline;
  const char* endline;
  const char* ptr;
  int hasdisplay = 0;
  int chars;

  nextlookup = &self->first_lookup;

  /* first pass: process macro initializers */
  line = script;
  while (*line)
  {
    nextline = rc_parse_line(line, &endline);
    if (strncmp(line, "Lookup:", 7) == 0) {
      line += 7;

      lookup = RC_ALLOC(rc_richpresence_lookup_t, parse);
      lookup->name = rc_alloc_str(parse, line, (int)(endline - line));
      lookup->format = RC_FORMAT_LOOKUP;
      *nextlookup = lookup;
      nextlookup = &lookup->next;

      nextline = rc_parse_richpresence_lookup(lookup, nextline, parse);
      if (parse->offset < 0)
        return;

    } else if (strncmp(line, "Format:", 7) == 0) {
      line += 7;

      lookup = RC_ALLOC(rc_richpresence_lookup_t, parse);
      lookup->name = rc_alloc_str(parse, line, (int)(endline - line));
      lookup->first_item = 0;
      *nextlookup = lookup;
      nextlookup = &lookup->next;

      line = nextline;
      nextline = rc_parse_line(line, &endline);
      if (parse->buffer && strncmp(line, "FormatType=", 11) == 0) {
        line += 11;

        chars = (int)(endline - line);
        if (chars > 63)
          chars = 63;
        memcpy(format, line, chars);
        format[chars] = '\0';

        lookup->format = rc_parse_format(format);
      } else {
        lookup->format = RC_FORMAT_VALUE;
      }
    } else if (strncmp(line, "Display:", 8) == 0) {
      display = nextline;

      do {
        line = nextline;
        nextline = rc_parse_line(line, &endline);
      } while (*line == '?');
    }

    line = nextline;
  }

  *nextlookup = 0;
  nextdisplay = &self->first_display;

  /* second pass, process display string*/
  if (display) {
    line = display;
    nextline = rc_parse_line(line, &endline);

    while (*line == '?') {
      /* conditional display: ?trigger?string */
      ptr = ++line;
      while (ptr < endline && *ptr != '?')
        ++ptr;

      if (ptr < endline) {
        *nextdisplay = rc_parse_richpresence_display_internal(ptr + 1, endline, parse, self);
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
      nextline = rc_parse_line(line, &endline);
    }

    /* non-conditional display: string */
    *nextdisplay = rc_parse_richpresence_display_internal(line, endline, parse, self);
    if (*nextdisplay) {
      hasdisplay = 1;
      nextdisplay = &((*nextdisplay)->next);
    }
  }

  /* finalize */
  *nextdisplay = 0;

  if (!hasdisplay && parse->offset > 0) {
    parse->offset = RC_MISSING_DISPLAY_STRING;
  }
}

int rc_richpresence_size(const char* script) {
  rc_richpresence_t* self;
  rc_parse_state_t parse;
  rc_init_parse_state(&parse, 0, 0, 0);

  self = RC_ALLOC(rc_richpresence_t, &parse);
  rc_parse_richpresence_internal(self, script, &parse);

  rc_destroy_parse_state(&parse);
  return parse.offset;
}

rc_richpresence_t* rc_parse_richpresence(void* buffer, const char* script, lua_State* L, int funcs_ndx) {
  rc_richpresence_t* self;
  rc_parse_state_t parse;

  if (!buffer || !script)
    return 0;

  rc_init_parse_state(&parse, buffer, L, funcs_ndx);

  self = RC_ALLOC(rc_richpresence_t, &parse);
  rc_init_parse_state_memrefs(&parse, &self->memrefs);

  rc_parse_richpresence_internal(self, script, &parse);

  rc_destroy_parse_state(&parse);
  return (parse.offset >= 0) ? self : 0;
}

int rc_evaluate_richpresence(rc_richpresence_t* richpresence, char* buffer, unsigned buffersize, rc_peek_t peek, void* peek_ud, lua_State* L) {
  rc_richpresence_display_t* display;
  rc_richpresence_display_part_t* part;
  rc_richpresence_lookup_item_t* item;
  char tmp[256];
  char* ptr;
  const char* text;
  size_t chars;
  unsigned value;

  rc_update_memref_values(richpresence->memrefs, peek, peek_ud);

  ptr = buffer;
  display = richpresence->first_display;
  while (display) {
    if (!display->next || rc_test_trigger(&display->trigger, peek, peek_ud, L)) {
      part = display->display;
      while (part) {
        switch (part->display_type) {
          case RC_FORMAT_STRING:
            text = part->text;
            chars = strlen(text);
            break;

          case RC_FORMAT_LOOKUP:
            value = rc_evaluate_value(&part->value, peek, peek_ud, L);
            item = part->first_lookup_item;
            if (!item) {
              text = "";
              chars = 0;
            } else {
              while (item->next_item && item->value != value)
                item = item->next_item;

              text = item->label;
              chars = strlen(text);
            }
            break;

          case RC_FORMAT_UNKNOWN_MACRO:
            chars = snprintf(tmp, sizeof(tmp), "[Unknown macro]%s", part->text);
            text = tmp;
            break;

          default:
            value = rc_evaluate_value(&part->value, peek, peek_ud, L);
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

    display = display->next;
  }

  buffer[0] = '\0';
  return 0;
}
