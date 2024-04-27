#pragma once

#ifdef _MSC_VER
/* avoid deprecated warning about strdup */
#define _CRT_NONSTDC_NO_DEPRECATE
#endif /* _MSC_VER */

#include <stdint.h>
#include <string.h> /* memcpy/strlen/strcmp/strdup */
#include <stdlib.h> /* malloc/realloc/free */

#include <formats/rjson.h>

/* if only there was a standard library function for this */
template <size_t Len>
inline size_t StringCopy(char (&dest)[Len], const char* src)
{
    size_t copied;
    char* out = dest;
    if (!src || !Len)
        return 0;
    for (copied = 1; *src && copied < Len; ++copied)
        *out++ = *src++;
    *out = 0;
    return copied - 1;
}

size_t JsonWriteHandshakeObj(char* dest, size_t maxLen,
      int version, const char* applicationId);

/* Commands */
struct DiscordRichPresence;
size_t JsonWriteRichPresenceObj(char* dest,
                                size_t maxLen,
                                int nonce,
                                int pid,
                                const DiscordRichPresence* presence);
size_t JsonWriteSubscribeCommand(char* dest, size_t maxLen, int nonce, const char* evtName);

size_t JsonWriteUnsubscribeCommand(char* dest, size_t maxLen, int nonce, const char* evtName);

size_t JsonWriteJoinReply(char* dest, size_t maxLen, const char* userId, int reply, int nonce);

class JsonWriter
{
   char* buf;
   size_t buf_len;
   size_t buf_cap;

   rjsonwriter_t* writer;
   bool need_comma;

   static int writer_io(const void* inbuf, int inlen, void *user_data)
   {
       JsonWriter* self  = (JsonWriter*)user_data;
       size_t buf_remain = (self->buf_cap - self->buf_len);
       if ((size_t)inlen > buf_remain)
          inlen = (int)buf_remain;
       memcpy(self->buf + self->buf_len, inbuf, inlen);
       self->buf_len += inlen;
       self->buf[self->buf_len - (self->buf_len == self->buf_cap ? 1 : 0)] = '\0';
       return inlen;
   }

   public:
      JsonWriter(char* dest, size_t maxLen)
         : buf(dest), buf_len(0), buf_cap(maxLen),
           need_comma(false)
      {
          writer = rjsonwriter_open_user(writer_io, this);
      }

      ~JsonWriter()
      {
          rjsonwriter_free(writer);
      }

      size_t Size()
      {
          rjsonwriter_flush(writer);
          return buf_len;
      }

      void WriteComma()
      {
          if (!need_comma)
             return;
          rjsonwriter_raw(writer, ",", 1);
          need_comma = false;
      }

      void StartObject()
      {
          WriteComma();
          rjsonwriter_raw(writer, "{", 1);
      }

      void StartArray()
      {
         WriteComma();
         rjsonwriter_raw(writer, "[", 1);
      }

      void EndObject()
      {
         rjsonwriter_raw(writer, "}", 1);
         need_comma = true;
      }

      void EndArray()
      {
         rjsonwriter_raw(writer, "]", 1);
         need_comma = true;
      }

      void Key(const char* key)
      {
          WriteComma();
          rjsonwriter_add_string(writer, key);
          rjsonwriter_raw(writer, ":", 1);
      }

      void String(const char* val)
      {
          WriteComma();
          rjsonwriter_add_string(writer, val);
          need_comma = true;
      }

      void Int(int value)
      {
          WriteComma();
          rjsonwriter_rawf(writer, "%d", value);
          need_comma = true;
      }

      void Int64(int64_t val)
      {
          WriteComma();
          char num[24], *pEnd = num + 24, *p = pEnd;
          if (!val)
          {
              *(--p) = '0';
          }
          else if (val < 0)
          {
              for (; val; val /= 10)
                  *(--p) = '0' - (char)(val % 10);
              *(--p) = '-';
          }
          else
          {
              for (; val; val /= 10)
                  *(--p) = '0' + (char)(val % 10);
          }
          rjsonwriter_raw(writer, p, (int)(pEnd - p));
          need_comma = true;
      }

      void Bool(bool value)
      {
          WriteComma();
          rjsonwriter_raw(writer, (value ? "true" : "false"), (value ? 4 : 5));
          need_comma = true;
      }
};

class JsonDocument
{
      size_t json_cap;
      enum { kDefaultChunkCapacity = 32 * 1024 };

   public:
      size_t json_length;
      char* json_data;

      JsonDocument()
         : json_cap(kDefaultChunkCapacity), json_length(0),
           json_data((char*)malloc(kDefaultChunkCapacity))
      { }

      ~JsonDocument()
      {
          free(json_data);
      }

      void ParseInsitu(const char* input)
      {
          size_t input_len = strlen(input);
          while (json_length + input_len >= json_cap)
          {
              json_cap += kDefaultChunkCapacity;
              json_data = (char*)realloc(json_data, json_cap);
          }
          memcpy(json_data + json_length, input, input_len);
          json_length += input_len;
          json_data[json_length] = '\0';
      }
};

class JsonReader
{
      rjson_t* reader;

   public:
      const char* key;
      unsigned int depth;

      JsonReader(JsonDocument& doc)
         : reader(rjson_open_buffer(doc.json_data, doc.json_length))
      { }

      ~JsonReader()
      {
          rjson_free(reader);
      }

      bool NextKey()
      {
          for (;;)
          {
              enum rjson_type json_type = rjson_next(reader);
              if (json_type == RJSON_DONE || json_type == RJSON_ERROR) return false;
              if (json_type != RJSON_STRING) continue;
              if (rjson_get_context_type(reader) != RJSON_OBJECT) continue;
              if ((rjson_get_context_count(reader) & 1) != 1) continue;
              depth = rjson_get_context_depth(reader);
              key = rjson_get_string(reader, NULL);
              return true;
          }
      }

      const char* NextString(const char* default_val)
      {
          return (rjson_next(reader) != RJSON_STRING ? default_val :
                       rjson_get_string(reader, NULL));
      }

      void NextStrDup(char** val)
      {
          if (rjson_next(reader) == RJSON_STRING)
              *val = strdup(rjson_get_string(reader, NULL));
      }

      void NextInt(int* val)
      {
          if (rjson_next(reader) == RJSON_NUMBER)
              *val = rjson_get_int(reader);
      }
};
