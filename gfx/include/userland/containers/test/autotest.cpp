/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <map>
#include <set>
#include <vector>
#include <list>
#include <algorithm>
#include <iomanip>

// MS compilers require __cdecl calling convention on some callbacks. Other compilers reject it.
#if (!defined(_MSC_VER) && !defined(__cdecl))
#define __cdecl
#endif

extern "C"
{
#include "containers/containers.h"
#include "containers/core/containers_common.h"
#include "containers/core/containers_logging.h"
#include "containers/core/containers_utils.h"
#include "containers/core/containers_io.h"

// Declare the CRC32 function from Snippets.org (obtained from the wayback machine, see crc_32.c)
   uint32_t crc32buf(const uint8_t *buf, size_t len);
}

// Error logger. It looks a little like std::cout, but it will stop the program if required on an end-of-line,
// returning an error to the caller. This is intended to allow automated tests to run to first fail,
// but interactive tests to run all the way through (using -k)
class ERROR_LOGGER_T
{
public:
   ERROR_LOGGER_T(): error_is_fatal(true), had_any_error(false)
   {}

   ~ERROR_LOGGER_T()
   {
      // Flush out any bits of message that have been assembled, but not yet flushed out.
      std::cerr << stream.str();

      if (had_any_error)
      {
         exit(VC_CONTAINER_ERROR_FAILED);
      }
   }

   // Tell the logger that we're going to keep going after an error if we possibly can
   void set_errors_not_fatal()
   {
      error_is_fatal = false;
   }

   // Tell the error logger to redirect its output to this stream instead of the default stderr.
   // This is used when dumping is enabled, so that errors become part of the stream description.
   void set_output(std::ostream* new_stream)
   {
      output_stream = new_stream;
   }

   // Generic inserter. Always calls through to the inserter for the base class.
   template <typename T> ERROR_LOGGER_T& operator<<(const T& object)
   {
      stream << object;
      had_any_error = true;
      return *this;
   }

   // specialised inserter for iomanip type objects. Ensures that this object, and not the contained stream,
   // is passed to the object.
   ERROR_LOGGER_T& operator<<(ERROR_LOGGER_T& (__cdecl *_Pfn)(ERROR_LOGGER_T&))
   {
      return _Pfn(*this);
   }

   // implementation of flush. This is called by endl, and if the flags are set the program will stop.
   ERROR_LOGGER_T& flush()
   {
      // If required send it to the quoted stream
      if (output_stream)
      {
         *output_stream << stream.str();
      }
      else
      {
         // just send it to stderr
         std::cerr << stream.str();
      }

      stream.clear();               // reset any odd flags
      stream.str("");               // empty the string

      if (error_is_fatal)
      {
         exit(VC_CONTAINER_ERROR_FAILED);
      }

      return *this;
   }

private:
   // set true if should stop on first error (usual for smoke testing)
   // or keep going (usual if you want to look at the logs). Controlled by config -k flag (as for make)
   bool error_is_fatal;

   // Set true if we've ever had an error. This way the app will return an error even if it kept going after it.
   bool had_any_error;

   // The current error message
   std::ostringstream stream;

   // The output stream, if not defaulted
   std::ostream* output_stream;
};

namespace std
{
   // GCC insists I say this twice:
   ERROR_LOGGER_T& ends(ERROR_LOGGER_T& logger);

   // implementation of std::ends for the error logger - flushes the message.
   ERROR_LOGGER_T& ends(ERROR_LOGGER_T& logger)
   {
      logger << "\n";
      logger.flush();
      return logger;
   }
}

// Internal to this file, PTS is stored in one of these. other code uses int64_t directly.
typedef int64_t PTS_T;

struct PACKET_DATA_T
{
   VC_CONTAINER_PACKET_T info;      // copy of the API's data
   std::vector<uint8_t> buffer;     // copy of the data contained, or empty if we have exceeded configuration.mem_max
   uint32_t crc;                    // CRC32 of the data.

   // Constructor. Zeroes the whole content.
   PACKET_DATA_T(): /* info((VC_CONTAINER_PACKET_T){0}), */ buffer(), crc(0)
   {
      // The syntax above for initialising info is probably correct according to C++ 0x.
      // Unfortunately the MS C++ compiler in VS2012 isn't really C++0x compliant.
      memset(&info, 0, sizeof(info));
   }
};

typedef uint32_t STREAM_T;

// Packets in a file in an arbitrary order, usually the order they were read.
typedef std::list<PACKET_DATA_T> ALL_PACKETS_T;

// Packets keyed by time (excludes packets with no PTS). This collection must be on a per-stream basis.
typedef std::map<PTS_T, ALL_PACKETS_T::const_iterator> TIMED_PACKETS_T;

// Packets with times. Must be split per-stream as multiple streams may have the same PTS.
// (It's actually unusual for more than one stream to have key frames)
typedef std::map<STREAM_T, TIMED_PACKETS_T> STREAM_TIMED_PACKETS_T;

// structure parsing and holding configuration information for the program.
struct CONFIGURATION_T
{
   // maximum size of in-memory buffers holding file data. Set by -m
   size_t mem_max;

   // The source file or URL being processed
   std::string source_name;

   // trace verbosity, to reflect the older test application. Set by -v.
   int32_t verbosity, verbosity_input, verbosity_output;

   // fatal errors flag set by -k
   bool errors_not_fatal;

   // dump-path for packet summary set by -d. May be std::cout.
   mutable std::ostream* dump_packets;

   // tolerance values set by -t.
   // Ideally when we seek to a time all the tracks would be right on it, but that's not always possible.
   // When we don't have seek_forwards set:
   // - The video must not be after the requested time
   // - The video must not be more than
   PTS_T tolerance_video_early;
   //   microseconds before the desired time. A 'good' container will hit it bang on if the supplied time is a video key frame.
   PTS_T tolerance_video_late;

   // - The other tracks must not be more than
   PTS_T tolerance_other_early;
   PTS_T tolerance_other_late;
   //   from the time of the video frame. If forcing is not supported we have to take what's in the file.

   // If set by -p  a test is performed re-reading the file using a packet buffer smaller than the expected packet:
   // If <1 it's a proportion (e.g 0.5 will read half the packet, then the other half on a subsequent read)
   // If 1 or more it's a size (1 will read 1 byte at a time; 100 not more than 100 bytes in each read)
   // Defaults to -ve value, which means don't do this test.
   double packet_buffer_size;

   // Constructor
   CONFIGURATION_T(int argc, char** argv)
      : mem_max(0x40000000)   // 1Gb for 32-bit system friendliness.
      , verbosity(0)          // no trace
      , verbosity_input(0)
      , verbosity_output(0)
      , errors_not_fatal(false)
      , dump_packets(nullptr)
      , tolerance_video_early(100000)   // 100k uS = 100mS
      , tolerance_video_late(0)
      , tolerance_other_early(100000)   // 100k uS = 100mS
      , tolerance_other_late(1000000)   // 1000k uS = 1S
      , packet_buffer_size(-1)
   {
      if (argc < 2)
      {
         std::cout <<
            "-d     produce packet dump (to file if specified)" << std::endl <<
            "-k     keep going on errors" << std::endl <<
            "-m     max memory buffer for packet data (default 1Gb). If the file is large not all will be validated properly." << std::endl <<
            "-p     re-read using a small packet buffer. If >= 1 this is a size; if < 1 a proportion of the packet" << std::endl <<
            "-v     verbose mode." << std::endl <<
            "         -vi input verbosity only" << std::endl <<
            "         -vo output verbosity only" << std::endl <<
            "         add more vvv to make it more verbose" << std::endl <<
            "-t     seek error tolerance in microseconds" << std::endl <<
            "         tv video streams" << std::endl <<
            "         to other streams" << std::endl <<
            "         te, tl all streams earliness, lateness" << std::endl <<
            "         tvl, tol video, other lateness" << std::endl <<
            "         toe, tve video, other earliness" << std::endl << std::endl <<

            "example: autotest -k 1-128.wmv -vvvvv -t1000000" << std::endl <<
            "  tests 1-128.wmv. Keeps going on errors. Very verbose. Tolerant of errors up to 1s in seeks." << std::endl;
         exit(0);
      }
      // Parse each argument
      for (int arg = 1; arg <argc; ++arg)
      {
         std::string argstr(argv[arg]);

         // We don't expect empty argument strings from either the Windows or Linux shells
         if (argstr.empty())
         {
            std::cerr << "Argument" << arg << " is a zero-length string";
            exit(VC_CONTAINER_ERROR_INVALID_ARGUMENT);
         }

         // If the string does not start with a - it must be the input file
         if (argstr.front() != '-')
         {
            if (source_name.empty())
            {
               source_name = argstr;
            }
            else
            {
               std::cerr << "Two source names supplied:" << std::endl << source_name << std::endl << argstr;
               exit(VC_CONTAINER_ERROR_INVALID_ARGUMENT);
            }
         }
         else
         {
            // throw away the hyphen
            argstr.erase(0,1);

            if (argstr.empty())
            {
               error(arg, argstr, "is too short");
            }

            // examine the char after the hyphen
            switch (argstr.at(0))
            {
            case 'd':
               // produce packet dump
               if (argstr.size() == 1)
               {
                  dump_packets = &std::cout;
               }
               else
               {
                  // Allocate a new ostream into it.
                  // Note: This new is not matched by a free - but this will be cleaned up at process end.
                  dump_packets = new std::ofstream(std::string(argstr.begin() + 1, argstr.end()), std::ios_base::out);
               }
               break;

            case 'k':
               // keep going on errors.
               if (argstr.size() == 1)
               {
                  errors_not_fatal = true;   // don't set the error logger's own flag yet. Command line parsing errors are ALWAYS fatal!
               }
               else
               {
                  error(arg, argstr, "-k has no valid following chars");
               }
               break;

               // memory size parameter
            case 'm':
               {
                  std::istringstream number(argstr);

                  // throw away the m
                  number.ignore(1);

                  if (number.eof())
                  {
                     error(arg, argstr, "Memory size not supplied");
                  }

                  // read the number
                  number >> mem_max;
                  if (!number.eof())
                  {
                     error(arg, argstr, "Size cannot be parsed");
                  }
               }
               break;

            case 'p':
               {
                  std::istringstream number(argstr);

                  // throw away the p
                  number.ignore(1);

                  if (number.eof())
                  {
                     error(arg, argstr, "Packet re-read size not supplied");
                  }

                  // read the number
                  number >> packet_buffer_size;
                  if (!number.eof())
                  {
                     error(arg, argstr, "Size cannot be parsed");
                  }
               }
               break;

            case 't':
               // error tolerance
               {
                  std::istringstream stream(argstr);

                  stream.ignore(1);    // throw away the t

                  // flags for which tolerance we are processing.
                  bool video = true, other = true, earliness=true, lateness = true;
                  int64_t tolerance;

                  // If there's a v or an o it's video or other only
                  if (!stream.eof())
                  {
                     switch (stream.peek())
                     {
                     case 'v':
                        other = false; // if he said video we don't touch the other params
                        stream.ignore(1);    // throw away the letter
                        break;

                     case 'o':
                        video = false;
                        stream.ignore(1);    // throw away the letter
                        break;

                        // do nothing on other chars.
                     }
                  }

                  // If there's an l or an e it's late or early only
                  if (!stream.eof())
                  {
                     switch (stream.peek())
                     {
                     case 'l':
                        earliness = false;
                        stream.ignore(1);    // throw away the letter
                        break;

                     case 'e':
                        lateness = false;
                        stream.ignore(1);    // throw away the letter
                        break;

                        // do nothing on other chars.
                     }
                  }

                  // read the number that follows
                  if (stream.eof())
                  {
                     error(arg, argstr, "tolerance not supplied");
                  }
                  else
                  {
                     // read the number
                     stream >> tolerance;
                     if (!stream.eof())
                     {
                        error(arg, argstr, "Number cannot be parsed");
                     }

                     if (video && earliness)
                     {
                        tolerance_video_early = tolerance;
                     }
                     if (video && lateness)
                     {
                        tolerance_video_late = tolerance;
                     }
                     if (other && earliness)
                     {
                        tolerance_other_early = tolerance;
                     }
                     if (other && lateness)
                     {
                        tolerance_other_late = tolerance;
                     }
                  }
               }
               break;

               // verbosity. v, vi or vo followed by zero or more extra v.
            case 'v':
               process_vees(arg, argstr);
               break;

               // anything else
            default:
               error(arg, argstr, "is not understood");
            }
         }
      }

      if (source_name.empty())
      {
         std::cerr << "No source name supplied";
         exit(VC_CONTAINER_ERROR_URI_NOT_FOUND);
      }

      if (verbosity != 0)
      {
         if (verbosity_input == 0)
         {
            verbosity_input = verbosity;
         }

         if (verbosity_output == 0)
         {
            verbosity_output = verbosity;
         }
      }

      std::cout << "Source: " << source_name << std::endl;
      std::cout << "Max buffer size:    " << mem_max << " 0x" << std::hex << mem_max << std::dec << std::endl;
      std::cout << "Verbosity:          " << verbosity                        << std::endl;
      std::cout << "Input verbosity:    " << verbosity_input                  << std::endl;
      std::cout << "Output verbosity:   " << verbosity_output                 << std::endl;
      std::cout << "Continue on errors: " << (errors_not_fatal ? 'Y' : 'N')   << std::endl;
      std::cout << "Seek tolerance (uS)"                                      << std::endl;
      std::cout << " Video Early:       " << tolerance_video_early            << std::endl;
      std::cout << " Video Late:        " << tolerance_video_late             << std::endl;
      std::cout << " Other Early:       " << tolerance_other_early            << std::endl;
      std::cout << " Other Late:        " << tolerance_other_late             << std::endl;
      std::cout << "Dump Summary:       " << (dump_packets ? 'Y' : 'N')       << std::endl;
   }
private:

   // processing for -v parameter.
   void process_vees(int arg, const std::string& argstr)
   {
      std::istringstream vees(argstr);

      // we know we have v, so we can drop it.
      vees.ignore(1);

      int32_t* which_param = &verbosity;
      int32_t value = VC_CONTAINER_LOG_ERROR|VC_CONTAINER_LOG_INFO;

      // process the rest of the characters, if any
      switch (vees.peek())
      {
      case 'v':
         // do nothing yet
         break;

      case 'i':
         which_param = &verbosity_input;
         vees.ignore(1);
         break;

      case 'o':
         which_param = &verbosity_output;
         vees.ignore(1);
         break;

      default:
         if (vees.peek() != std::char_traits<char>::eof())
         {
            error(arg, argstr, "verbosity is not understood");
         }
         break;
      }

      while (1)
      {
         int next = vees.get();
         if (next == 'v')
         {
            // add verbosity
            value = (value << 1) | 1;
         }
         else if (next == std::char_traits<char>::eof())
         {
            break;
         }
         else
         {
            error(arg, argstr, "verbosity is not understood");
         }
      }

      // store what we parsed.
      *which_param = value;
   }

   // error handling function. Does not return.
   void error(int arg, const std::string& argstr, const std::string& msg)
   {
      std::cerr << "Argument " << arg << " \"" << argstr << "\" " << msg;
      exit(VC_CONTAINER_ERROR_INVALID_ARGUMENT);
   }
};

// Most of the functionality of the program is in this class, or its subsidiaries.
class TESTER_T
{
public:
   // Construct, passing command line parameters
   TESTER_T(int argc, char**argv)
      : configuration(argc, argv)
      , video_stream(std::numeric_limits<STREAM_T>::max())
   {
      // If the configuration said keep going on errors tell the logger.
      if (configuration.errors_not_fatal)
      {
         error_logger.set_errors_not_fatal();
      }

      // Tell it where any -d routed the file summary
      error_logger.set_output(configuration.dump_packets);
   }

   // Run the tests
   VC_CONTAINER_STATUS_T run()
   {
      /* Set the verbosity */
      vc_container_log_set_verbosity(0, configuration.verbosity_input);

      // Open the container
      p_ctx = vc_container_open_reader(configuration.source_name.c_str(), &status, 0, 0);

      if(!p_ctx || (status != VC_CONTAINER_SUCCESS))
      {
        error_logger << "error opening file " << configuration.source_name << " Code " << status << std::ends;
      }

      // Find the video stream.
      for(size_t i = 0; i < p_ctx->tracks_num; i++)
      {
         if (p_ctx->tracks[i]->format->es_type == VC_CONTAINER_ES_TYPE_VIDEO)
         {
            if (video_stream == std::numeric_limits<STREAM_T>::max())
            {
               video_stream = i;
            }
            else
            {
               // we've found two video streams. This is not necessarily an error, but it's certainly odd - perhaps it's the angles
               // from a DVD. We don't expect to see it, but report it anyway. Don't stop, just assume that the first one is the one we want.
               error_logger << "Both track " << video_stream << " and " << i << " are marked as video streams" << std::ends;
            }
         }
      }

      if (video_stream == std::numeric_limits<STREAM_T>::max())
      {
         error_logger << "No video track found" << std::ends;
      }

      // Read all the packets sequentially. This will gve us all the metadata of the packets,
      // and (up to configuration.mem_max) will also store the packet data.
      read_sequential();

      // Now we have some packets we can initialise our internal RNG.
      init_crg();

      // Find all the keyframes in the collection we just read.
      find_key_packets();

      // Seeking tests. Only if the container supports it.
      // We do this first, because one of the checks is whether or not the container supports seeking,
      // and we want to be able to seek back between tests.
      if (p_ctx->capabilities & VC_CONTAINER_CAPS_CAN_SEEK)
      {
         /// Seek to PTS 0 (this ought to be the beginning)
         PTS_T beginning = 0;
         status = vc_container_seek(p_ctx, &beginning, VC_CONTAINER_SEEK_MODE_TIME, 0);
         if (status != VC_CONTAINER_SUCCESS)
         {
            error_logger << "Container failed to seek to the beginning - status " << status << std::ends;

            // This is fatal.
            exit(status);
         }

         // Ask for info about the first packet
         PACKET_DATA_T actual;
         status = vc_container_read(p_ctx, &actual.info, VC_CONTAINER_READ_FLAG_INFO);

         if (status != VC_CONTAINER_SUCCESS)
         {
            error_logger << "Read info failed after seek to the beginning - status " << status << std::ends;
         }
         else
         {
            PACKET_DATA_T& expected = all_packets.front();
            // Compare some selected data to check that this genuinely is the first packet.
            // We don't actually do a read.
            if ((expected.info.pts != actual.info.pts)
               || (expected.info.dts != actual.info.dts)
               || (expected.info.size != actual.info.size)
               || (expected.info.frame_size != actual.info.frame_size)
               || (expected.info.track != actual.info.track)
               || (expected.info.flags != actual.info.flags))
            {
               // Copy the CRC from the expected value. That will stop compare_packets whinging.
               actual.crc = expected.crc;

               // report the discrepancy
               error_logger << "Seek to time zero did not arrive at beginning: "
                  << compare_packets(expected, actual) << std::ends;
            }
         }

         // Perform seeks to find all the packets that a seek will go to.
         check_indices(0);
         check_indices(VC_CONTAINER_SEEK_FLAG_FORWARD);

         // If it supports forcing then seek to each of those locations, and force read all the tracks.
         if (p_ctx->capabilities & VC_CONTAINER_CAPS_FORCE_TRACK)
         {
            check_seek_then_force(0);
            check_seek_then_force(VC_CONTAINER_SEEK_FLAG_FORWARD);
         }

         seek_randomly(0);
         seek_randomly(VC_CONTAINER_SEEK_FLAG_FORWARD);

         // todo more?
      }
      else
      {
         // The file claims not to support seeking. Perform a seek, just to check this out.
         PTS_T beginning = 0;

         status = vc_container_seek(p_ctx, &beginning, VC_CONTAINER_SEEK_MODE_TIME, 0);

         if (status != VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION)
         {
            error_logger << "Container did not reject seek when its capabilities said it is not supported" << std::ends;
         }
      }

      // If the config didn't ask for this test don't do it.
      if (configuration.packet_buffer_size > 0)
      {
         re_seek_to_beginning();

         // Read through the file, reading the packet in several bites (bite size controlled by -p parameter)
         check_partial_reads();

         // Check forcing (this doesn't require seek)
         if (p_ctx->capabilities & VC_CONTAINER_CAPS_FORCE_TRACK)
         {
            re_seek_to_beginning();
            check_forcing();
         }
      }

      std::cout << "Test Complete" << std::endl;

      // If anything failed then the error logger will replace this error code. So always return 0 from here.
      return VC_CONTAINER_SUCCESS;
   }
private:
   // read packets from the file, and stash them away.
   void read_sequential()
   {
      // Use the same buffer for reading each of the packets. It's cheaper to copy the (usually small) data
      // from this buffer than to create a new one for each packet, then resize it.
      // 256k is the size used in the other program, and is bigger than any known packet. Increase it
      // if a bigger one is found.
      std::vector<uint8_t> buffer(256*1024, 0);

      std::map<uint32_t, size_t> packet_counts;

      // Count of memory used for packet buffers.
      size_t memory_buffered = 0;

      if (configuration.dump_packets)
      {
         *configuration.dump_packets << "pts" << ",\t" << "track" << ",\t" << "size" << ",\t" << "crc" << std::endl;
      }

      while (1)   //  We break out at EOF.
      {
         // Packet object
         PACKET_DATA_T packet;

         // Use the shared buffer for the moment,
         packet.info.data = &buffer[0];
         packet.info.buffer_size = buffer.size();

         // Read data.
         status = vc_container_read(p_ctx, &packet.info, 0);

         if (packet.info.size >= buffer.size())
         {
            std::cerr << "Packet size limit exceeeded. Increase internal buffer size!";
            exit(VC_CONTAINER_ERROR_FAILED);
         }

         if (status == VC_CONTAINER_SUCCESS)
         {
            // Calculate the CRC of the packet
            packet.crc = crc32buf(&buffer[0], packet.info.size);

            // If there is any data, and we haven't exceeded our size limit...
            if ((packet.info.size > 0) && (memory_buffered < configuration.mem_max))
            {
               // Copy the data we read across from the big buffer to our local one
               packet.buffer.assign(buffer.begin(), buffer.begin() + packet.info.size);

               // count how much we have buffered
               memory_buffered += packet.info.size;

               // wipe the bit of the big buffer we used
               memset(&buffer[0], 0, packet.info.size);
            }
            else
            {
               // not storing any data, either because there is none or we have no space.
               packet.buffer.clear();
            }

            // Clear the pointer in our data. It won't be valid later.
            packet.info.data = 0;

            // Set the size of the buffer to match what is really there.
            packet.info.buffer_size = packet.info.size;

            // count the packet
            ++packet_counts[packet.info.track];

            // store it
            all_packets.push_back(packet);

            // perhaps dump it
            if (configuration.dump_packets)
            {
               if (packet.info.pts < 0)
               {
                  *configuration.dump_packets << "-";
               }
               else
               {
                  *configuration.dump_packets << packet.info.pts;
               }

               *configuration.dump_packets
                  << ",\t" << packet.info.track << ",\t" << packet.info.size << ",\t" << packet.crc << std::endl;
            }
         }
         else if (status == VC_CONTAINER_ERROR_EOS)
         {
            break;
         }
         else
         {
            error_logger << "error reading file " << configuration.source_name << " Code " << status << std::ends;
         }
      }

      std::cout << std::dec << "File has " << packet_counts.size() << " tracks." << std::endl;

      // Print the packet count for each stream. Also, while iterating, save all the stream numbers.
      for(std::map<uint32_t, size_t>::const_iterator stream = packet_counts.begin(); stream != packet_counts.end(); ++stream)
      {
         // Print the packet count
         std::cout << "Stream " << stream->first << " has " << stream->second << " packets." << std::endl;

         // Note that we have a stream with this number.
         all_streams.insert(stream->first);
      };

      if (p_ctx->tracks_num != packet_counts.size())
      {
         error_logger << "The file header claims " << p_ctx->tracks_num
            << " but " << packet_counts.size() << " streams were found" << std::ends;
      }
   }

   // Search the all_packets collection for key frames, and store them in all_key_packets
   void find_key_packets()
   {
      // The last known PTS for each stream.
      std::map<STREAM_T, PTS_T> last_pts_in_stream;

      for (ALL_PACKETS_T::const_iterator packet = all_packets.begin(); packet != all_packets.end(); ++packet)
      {
         PTS_T pts = packet->info.pts;
         STREAM_T stream = packet->info.track;

         // If it has a PTS check they are an ascending sequence.
         if (pts >= 0)
         {
            // Find the last PTS, if any, for this stream
            std::map<STREAM_T, PTS_T>::const_iterator last_known_pts = last_pts_in_stream.find(stream);

            if ((last_known_pts != last_pts_in_stream.end()) && (pts <= last_known_pts->second))
            {
               // the PTS isn't bigger than the previous best. This is an error.
               error_logger << "Out of sequence PTS " << pts << " found. Previous largest was "
                  << last_pts_in_stream[stream] << std::ends;
            }

            // store it (even if bad)
            last_pts_in_stream[stream] = pts;

            // Store in the collection of packets with PTSes
            all_pts_packets[stream].insert(std::make_pair(pts, packet));

            // if it is also a keyframe
            if (packet->info.flags & VC_CONTAINER_PACKET_FLAG_KEYFRAME)
            {
               // Put it into the collection for its track.
               all_key_packets[stream].insert(std::make_pair(pts, packet));
            }
         }
      }
   }

   // Check which locations can be accessed by a seek, with or without VC_CONTAINER_SEEK_FLAG_FORWARD.
   void check_indices(VC_CONTAINER_SEEK_FLAGS_T direction)
   {
      STREAM_TIMED_PACKETS_T& index_positions = direction == 0 ? reverse_index_positions : forward_index_positions;
      // Go through each stream that contains key packets (usually only one)
      for (STREAM_TIMED_PACKETS_T::const_iterator stream_keys = all_key_packets.begin();
         stream_keys != all_key_packets.end();
         ++stream_keys)
      {
         // Start with a PTS just after the last key packet, and repeat until we fall off the beginning.
         for (PTS_T target_pts = stream_keys->second.rbegin()->first + 1; target_pts >= 0; /* no decrement */)
         {
            // copy the PTS value to pass to vc_container_seek
            PTS_T actual_pts = target_pts;

            // Seek to that position in the file.
            status = vc_container_seek(p_ctx, &actual_pts, VC_CONTAINER_SEEK_MODE_TIME, direction);

            if (status != VC_CONTAINER_SUCCESS)
            {
               error_logger << "Error " << status << " seeking to PTS " << target_pts << std::ends;
               continue;   // if errors are not fatal we'll try again 1uS earlier.
            }

            // Check whether this seek reported that it went somewhere sensible
            check_correct_seek(direction, actual_pts, target_pts);

            // Validate that the place it said we arrived at is correct - read info for the first packet in any stream
            {
               PACKET_DATA_T packet;
               status = vc_container_read(p_ctx, &packet.info, VC_CONTAINER_READ_FLAG_INFO);

               if (status != VC_CONTAINER_SUCCESS)
               {
                  error_logger << "Error " << status << " reading info for packet at PTS " << target_pts << std::ends;
                  continue;   // stop trying to read at this PTS.
               }

               if (packet.info.pts != actual_pts)
               {
                  error_logger << "Incorrect seek. Container claimed to have arrived at "
                     << actual_pts << " but first packet ";

                  if (packet.info.pts < 0)
                  {
                     error_logger << " had no PTS";
                  }
                  else
                  {
                        error_logger << "was at " << packet.info.pts;
                  }

                  error_logger << std::ends;
               }
            }

            // We'll perform reads until we've had a packet with PTS from every stream.
            for (std::set<STREAM_T> unfound_streams = all_streams;
               !unfound_streams.empty();
               /* unfound_streams.erase(packet.info.track) */ )
            {
               // Read a packet. We can't be sure what track it will be from, so first read the info...
               PACKET_DATA_T packet;
               status = vc_container_read(p_ctx, &packet.info, VC_CONTAINER_READ_FLAG_INFO);

               if (status != VC_CONTAINER_SUCCESS)
               {
                  error_logger << "Error " << status << " reading info for packet following PTS " << target_pts << std::ends;
                  break;   // stop trying to read at this PTS.
               }

               // allocate a buffer.
               packet.buffer.resize(packet.info.size);
               packet.info.data = &packet.buffer[0];
               packet.info.buffer_size = packet.info.size;

               // perform the read
               status = vc_container_read(p_ctx, &packet.info, 0);

               if (status != VC_CONTAINER_SUCCESS)
               {
                  error_logger << "Error " << status << " reading packet following PTS " << target_pts << std::ends;
                  break;   // stop trying to read at this PTS.
               }

               STREAM_T stream = packet.info.track;

               // If it has no PTS we can't use it. Read another one.
               if (packet.info.pts < 0)
               {
                  // The first packet we found for a stream has no PTS.
                  // That's odd, because we wouldn't be able to play it. However,
                  // if the stream doesn't permit forcing it may be inevitable.
                  if ((unfound_streams.find(stream) != unfound_streams.end())
                     && ((p_ctx->capabilities & VC_CONTAINER_CAPS_FORCE_TRACK) != 0))
                  {
                     error_logger << "Packet in stream " << stream
                        << " has no PTS after seeking to PTS " << target_pts << std::ends;
                  }
                  continue;
               }

               // Search for the PTS collection for this stream
               STREAM_TIMED_PACKETS_T::iterator stream_packets = all_pts_packets.find(stream);

               if (stream_packets == all_pts_packets.end())
               {
                  error_logger << "Packet from unknown stream " << stream
                     << "found when reading packet at PTS " << target_pts << std::ends;
                  continue;      // try reading another packet.
               }

               // Look for the packet for this PTS in this stream.
               TIMED_PACKETS_T::const_iterator expected_packet = stream_packets->second.find(packet.info.pts);

               if (expected_packet == stream_packets->second.end())
               {
                  error_logger << "Read packet from stream " << stream << " has unknown PTS " << packet.info.pts << std::ends;
                  continue;
               }

               // calculate the CRC
               packet.crc = crc32buf(&packet.buffer[0], packet.info.size);

               // Validate that the data is the data we found when we did the sequential reads.
               std::string anyerror = compare_packets(*expected_packet->second, packet);

               if (!anyerror.empty())
               {
                  error_logger << "Incorrect data found at PTS " << actual_pts << anyerror << std::ends;
               }

               // If this is the first packet we found for this stream
               if (unfound_streams.find(stream) != unfound_streams.end())
               {
                  // Store the packet we found. Note we key it by the PTS we used for the seek,
                  // not its own PTS. This should mean next time we seek to that PTS we'll get this packet.
                  index_positions[stream].insert(std::make_pair(target_pts, expected_packet->second));

                  // Check whether the time on the packet is reasonable - it ought to be close to the time achieved.
                  check_seek_tolerance(packet, actual_pts);

                  // Now we've found a packet from this stream we're done with it, so note we don't care about it any more.
                  unfound_streams.erase(stream);
               }
            }  // repeat until we've had a packet for every track

            // Adjust the target location
            if (actual_pts > target_pts)
            {
               // Find the next packet down from where we looked, and try there.
               TIMED_PACKETS_T this_stream_keys = stream_keys->second;
               TIMED_PACKETS_T::const_iterator next_packet = this_stream_keys.lower_bound(target_pts);

               // If there is such a packet that it has more than this PTS, and it isn't the first packet
               if ((next_packet != this_stream_keys.begin()) &&
                  (next_packet != this_stream_keys.end()))
               {
                  // pull out the PTS from the one before, and ask for a microsecond past it.
                  PTS_T new_target_pts = (--next_packet)->first + 1;
                  if (new_target_pts >= target_pts)
                  {
                     --target_pts;
                  }
                  else
                  {
                     target_pts = new_target_pts;
                  }
               }
               else
               {
                  // There's no packet earlier than where we are looking.
                  // First time try 1 next time try zero.
                  target_pts = target_pts != 1 ? 1 : 0;
               }
            }
            else if (actual_pts < (target_pts - 1))
            {
               // The place we arrived at is well before where we wanted.
               // Next time go just after where we arrived.
               target_pts = actual_pts + 1;
            }
            else
            {
               // If the place we asked for is where we arrived, next time try one microsecond down.
               --target_pts;
            }
         }
      }
   }

   // Seek to all feasible locations and perform force reads on all possible tracks
   void check_seek_then_force(VC_CONTAINER_SEEK_FLAGS_T direction)
   {
      // Depending on whether we are doing forward or reverse seeks pick a collection of places a seek can go to.
      STREAM_TIMED_PACKETS_T index_positions = direction == 0 ? reverse_index_positions : forward_index_positions;

      // Go through all the streams
      for (STREAM_TIMED_PACKETS_T::const_iterator index_stream = index_positions.begin();
         index_stream != index_positions.end();
          ++index_stream)
      {
         // Go through all the packets in each stream that can be indexed
         for (TIMED_PACKETS_T::const_iterator location = index_stream->second.begin();
            location != index_stream->second.end();
            ++location
            )
         {
            // This is the time we expect
            PTS_T actual_pts = location->first;

            // Seek to that position in the file.
            status = vc_container_seek(p_ctx, &actual_pts, VC_CONTAINER_SEEK_MODE_TIME, direction);

            if (status != VC_CONTAINER_SUCCESS)
            {
               error_logger << "Error " << status << " seeking to PTS " << location->first << std::ends;
               continue;   // if errors are not fatal we'll try the next position.
            }

            // We'll perform force-reads until we've had a packet from every stream that has any PTS in it.
            for (STREAM_TIMED_PACKETS_T::const_iterator stream = all_pts_packets.begin(); stream != all_pts_packets.end(); ++stream)
            {
               // Read a packet.
               PACKET_DATA_T packet;
               packet.info.track = stream->first;

               // This loop repeats occasionally with a continue, but usually stops on the break at the end first time around.
               while(true)
               {
                  status = vc_container_read(p_ctx, &packet.info, VC_CONTAINER_READ_FLAG_INFO | VC_CONTAINER_READ_FLAG_FORCE_TRACK);

                  if (status != VC_CONTAINER_SUCCESS)
                  {
                     error_logger << "Error " << status << " force-reading info for stream "
                        << location->first << " after seeking to PTS " << stream->first << std::ends;

                     // If we can't read the info there's no point in anything else on this stream
                     break;
                  }

                  // allocate a buffer.
                  packet.buffer.resize(packet.info.size);
                  packet.info.data = &packet.buffer[0];
                  packet.info.buffer_size = packet.info.size;

                  // perform the actual read
                  status = vc_container_read(p_ctx, &packet.info, VC_CONTAINER_READ_FLAG_FORCE_TRACK);

                  if (status != VC_CONTAINER_SUCCESS)
                  {
                     error_logger << "Error " << status << " force-reading stream "
                        << location->first << " after seeking to PTS " << stream->first << std::ends;

                     // If we didn't read successfully we can't check the data. Try the next stream.
                     break;
                  }

                  // If it has no PTS we can't use it - and it can't be played either.
                  if (packet.info.pts < 0)
                  {
                     error_logger << "Packet force-read on stream " << stream->first
                        << " has no PTS after seeking to PTS " << location->first << std::ends;

                     // Try force-reading another.
                     continue;
                  }

                  // Make sure that the packet is near the time we wanted.
                  check_seek_tolerance(packet, actual_pts);

                  // Look for the packet for this PTS in this stream.
                  TIMED_PACKETS_T::const_iterator expected_packet = stream->second.find(packet.info.pts);

                  if (expected_packet == stream->second.end())
                  {
                     error_logger << "Packet force-read on stream " << stream->first
                        << " has unknown PTS " << packet.info.pts << std::ends;

                     // Try force-reading another.
                     continue;
                  }

                  packet.crc = crc32buf(packet.info.data, packet.info.size);

                  // Validate that the data is the data we found when we did the sequential reads.
                  std::string anyerror = compare_packets(*expected_packet->second, packet);

                  if (!anyerror.empty())
                  {
                     error_logger << "Incorrect data found at PTS " << actual_pts << anyerror << std::ends;
                  }

                  // If we arrive here we're done for this stream.
                  break;
               }
            }
            // repeat until we've had a packet for every stream at this index position
         }
         // repeat for every index position in the stream
      }
      // repeat for every stream that has index positions
   }

   // seek to a random selection of places, and see if we get the correct data
   void seek_randomly(VC_CONTAINER_SEEK_FLAGS_T direction)
   {
      // Depending on whether we are doing forward or reverse seeks pick a collection of places a seek can go to.
      const STREAM_TIMED_PACKETS_T& index_positions = direction == 0 ? reverse_index_positions : forward_index_positions;

      // Go through each stream in that collection
      for (STREAM_TIMED_PACKETS_T::const_iterator i_all_index_packets = index_positions.begin();
         i_all_index_packets != index_positions.end();
         ++i_all_index_packets)
      {
         // These are the index packets in the stream.
         const TIMED_PACKETS_T& all_index_packets = i_all_index_packets->second;

         // this is the number of seeks we'll perform.
         size_t seek_count = all_index_packets.size();

         // If there are more than 100 locations limit it to 100.
         if (seek_count > 100)
         {
            seek_count = 100;
         }

         // We want an unsorted list of PTSes.
         std::list<TIMED_PACKETS_T::value_type> selected_index_packets;
         {
            // Picking 100 at random out of a potentially large collection of timestamps in a set is going
            // to be an expensive operation - search by key is really fast, but search by position is not.
            // This is an attempt to come up with something reasonably efficient even if the collection is large.
            std::vector<PTS_T> all_ptses;

            // Reserve enough memory to hold every PTS. This collection should be several orders of magnitude smaller than the file.
            all_ptses.reserve(all_index_packets.size());

            // Copy all the PTS values into the vector
            std::transform(
               all_index_packets.begin(),
               all_index_packets.end(),
               std::back_inserter(all_ptses),
               [](const TIMED_PACKETS_T::value_type& source){ return source.first; }
            );

            // Based roughly on the Knuth shuffle, get a random subset of the PTS to the front of the collection.
            std::vector<PTS_T>::iterator some_pts;
            size_t count;
            for (count = 0, some_pts = all_ptses.begin(); count < seek_count; ++count, ++some_pts)
            {
               // Swap some random PTS into one of the first seek_count locations.
               // Note this may well be another of the first seek_count locations, especially if this is most of them
               std::iter_swap(some_pts, (all_ptses.begin() + rand() % seek_count));
            }

            // Throw away the ones we don't want
            all_ptses.resize(seek_count);

            // Copy the iterators for each packet with a selected PTS into the list.
            std::transform(
               all_ptses.begin(),
               all_ptses.end(),
               std::back_inserter(selected_index_packets),
               [&all_index_packets](PTS_T time){ return *all_index_packets.find(time); });

            // End scope. That will free the rest of all_ptses.
         }

         // Loop through the selected packets.
         for (std::list<TIMED_PACKETS_T::value_type>::iterator expected = selected_index_packets.begin();
            expected != selected_index_packets.end();
            ++expected)
         {
            // Seek to their position & check we got there
            const PACKET_DATA_T& target = *expected->second;
            PTS_T target_pts = expected->first;

            status = vc_container_seek(p_ctx, &target_pts, VC_CONTAINER_SEEK_MODE_TIME, direction);
            check_correct_seek(direction, target_pts, expected->first);

            // Start by initialising the new packet object to match the old one - it's mostly right.
            PACKET_DATA_T found_packet = target;

            // If forcing is supported, read the first packet & check it's OK.
            if (p_ctx->capabilities & VC_CONTAINER_CAPS_FORCE_TRACK)
            {
               // There might not be a buffer if we ran out of memory
               found_packet.buffer.resize(found_packet.info.size);

               // Set the address
               found_packet.info.data = &found_packet.buffer[0];

               // Perform the read
               status = vc_container_read(p_ctx, &found_packet.info, VC_CONTAINER_READ_FLAG_FORCE_TRACK);
            }
            else
            {
               // as forcing is not supported repeat reads until a packet is found for this track.
               do
               {
                  status = vc_container_read(p_ctx, &found_packet.info, VC_CONTAINER_READ_FLAG_INFO);

                  // Give up on any error
                  if (status != VC_CONTAINER_SUCCESS) break;

                  // Set the buffer to match the actual packet
                  found_packet.info.buffer_size = found_packet.info.size;
                  found_packet.buffer.resize(found_packet.info.size);
                  found_packet.info.data = &found_packet.buffer[0];
                  status = vc_container_read(p_ctx, &found_packet.info, 0);

                  if (status != VC_CONTAINER_SUCCESS) break;
               }
               while (found_packet.info.track != target.info.track);
            }

            if (status != VC_CONTAINER_SUCCESS)
            {
               error_logger << "Error " << status << " after random seek to PTS " << target_pts << std::ends;
            }
            else
            {
               found_packet.crc = crc32buf(&found_packet.buffer[0], found_packet.info.size);

               // We now have a packet from the right stream - it ought to be the one we asked for.
               const std::string& compare_result = compare_packets(found_packet, target);

               if (!compare_result.empty())
               {
                  error_logger << "Incorrect result from reading packet after random seek: " << compare_result << std::ends;
               }
            }
         }
      }
   }

   // Seek back to the beginning of the file, either with a seek or by closing and re-opening it
   void re_seek_to_beginning()
   {
      // If the container supports seek do it.
      if (p_ctx->capabilities & VC_CONTAINER_CAPS_CAN_SEEK)
      {
         PTS_T beginning = 0;

         status = vc_container_seek(p_ctx, &beginning, VC_CONTAINER_SEEK_MODE_TIME, 0);

         if (status != VC_CONTAINER_SUCCESS)
         {
            error_logger << "Failed to seek back to the beginning" << std::ends;
         }
      }
      else
      {
         // The file claims not to support seeking. Close it, and re-open - this should do it.
         status = vc_container_close(p_ctx);

         if (status != VC_CONTAINER_SUCCESS)
         {
            error_logger << "Error " << status << " Failed to close the container." << std::ends;
         }

         p_ctx = vc_container_open_reader(configuration.source_name.c_str(), &status, 0, 0);
         if (status != VC_CONTAINER_SUCCESS)
         {
            error_logger << "Error " << status << " Failed to re-open the container." << std::ends;

            // Even with -k this is a fatal error. Give up.
            exit(status);
         }
      }
   }

   // Read through the file, checking to see that the packets we get match the ones we read first time around -
   // given that we're going to read with a buffer of a different size, either a fixed value or a proportion
   // of the actual packet size.
   void check_partial_reads()
   {
      // This is used for some reads, and for compares.
      PACKET_DATA_T actual;

      // Repeat until we meet EOF, which will be in the middle of the loop somewhere.
      ALL_PACKETS_T::const_iterator expected;
      for (expected = all_packets.begin(); expected != all_packets.end(); ++expected)
      {
         // Ask for info about the first packet
         status = vc_container_read(p_ctx, &actual.info, VC_CONTAINER_READ_FLAG_INFO);

         if (status != VC_CONTAINER_SUCCESS)
         {
            error_logger << "Error " << status << " reading info for partial packet" << std::ends;
            // Not much point in carrying on if we can't read - but usually this is the end-of-file break.
            break;
         }

         size_t whole_packet = actual.info.size;

         // Work out how big a read we want to do.
         size_t wanted_read_size = (configuration.packet_buffer_size >= 1.0)
            ? (size_t)configuration.packet_buffer_size
            : (size_t)(configuration.packet_buffer_size * (double)whole_packet);

         // Make sure it's at least 1 byte (a small proportion of a small packet might round down to zero)
         if (wanted_read_size == 0)
         {
            wanted_read_size = 1;
         }

         // Ensure our buffer is at least big enough to contain the _whole_ packet.
         if (whole_packet > actual.buffer.size())
         {
            actual.buffer.resize(whole_packet);
         }

         // We'll need to collect some data from the bits of the packet as they go by.
         PTS_T first_valid_pts = std::numeric_limits<PTS_T>::min();
         PTS_T first_valid_dts = std::numeric_limits<PTS_T>::min();
         uint32_t total_flags = 0;

         size_t amount_read;

         // Loop around several times, reading part of the packet each time
         for (amount_read = 0; amount_read < whole_packet; /* increment elsewhere */)
         {
            // Somewhere in the buffer
            actual.info.data = &actual.buffer[amount_read];

            // Not more than our calculated size
            actual.info.buffer_size = wanted_read_size;

            // read some data.
            status = vc_container_read(p_ctx, &actual.info, 0);

            if (status != VC_CONTAINER_SUCCESS)
            {
               error_logger << "Unable to read " << wanted_read_size << " bytes from packet of size "
                  << expected->info.size << " error " << status << std::ends;

               // Guess we're at end of packet. We _might_ be able to recover from this.
               break;
            }

            amount_read += actual.info.size;

            // validate the amount read is less than the request
            if (actual.info.size > wanted_read_size)
            {
               error_logger << "Too much data read from packet. Request size was " << whole_packet
                  << " but " << actual.info.size << "read in" << std::ends;
            }

            // and that the total amount read for this packet is not too much
            if (amount_read > whole_packet)
            {
               error_logger << "Too much data read from packet. Total size is " << whole_packet
                  << " but " << amount_read << "read in" << std::ends;
            }

            // OR all the flags together
            total_flags |= actual.info.flags;

            // Save the PTS if we don't have one yet
            if (first_valid_pts < 0)
            {
               first_valid_pts = actual.info.pts;
            }

            // Ditto the DTS
            if (first_valid_dts < 0)
            {
               first_valid_dts = actual.info.dts;
            }
         }

         // The buffer should now contain all the correct data. However, the size field
         // reflects the last read only - so correct it before the compare.
         actual.info.size = amount_read;

         // store back the other stuff we got in the loop
         actual.info.flags = total_flags;
         actual.info.pts = first_valid_pts;
         actual.info.dts = first_valid_dts;

         // Calculate the CRC of the whole lot of data
         actual.crc = crc32buf(&actual.buffer[0], amount_read);

         // It's possible that the packet read isn't the one we expected.
         // (This happens with the Supremecy3_20sec_WMV_MainProfile_VGA@29.97fps_2mbps_WMA9.2.wmv sample,
         // where the first packet has no PTS and is not a key frame - seek won't go there)
         if ((expected->info.pts != actual.info.pts)
            && (actual.info.pts >= 0))
         {
            ALL_PACKETS_T::const_iterator candidate = std::find_if
               (all_packets.begin(),
               all_packets.end(),
               [&actual](ALL_PACKETS_T::value_type& expected) -> bool
            {
               // If we find one with the correct track and PTS, that will do.
               return (expected.info.pts == actual.info.pts)
                  && (expected.info.track == actual.info.track);
            });

            if (candidate != all_packets.end())
            {
               // We've seen the packet before
               error_logger << "Partial read returned an unexpected packet. Expect PTS " << expected->info.pts
                  << " but got PTS " << actual.info.pts << std::ends;

               // switch to expecting this packet
               expected = candidate;
            }

            // If we didn't find the packet anywhere in the expected list do nothing, and let the compare fail.
         }

         const std::string& compare_result = compare_packets(*expected, actual);

         if (!compare_result.empty())
         {
            error_logger << "Incorrect result from reading packet in parts: " << compare_result << std::ends;
         }
      }

      // check end reached if we were OK this far
      if (status == VC_CONTAINER_SUCCESS)
      {
         // We should be at the end of the expected collection
         if (expected != all_packets.end())
         {
            error_logger << "Failed to read entire file when reading partial packets" << std::ends;
         }

         // We should not be able to read any more.
         status = vc_container_read(p_ctx, &actual.info, VC_CONTAINER_READ_FLAG_INFO);

         if (status == VC_CONTAINER_ERROR_EOS)
         {
            status = VC_CONTAINER_SUCCESS;
         }
         else
         {
            error_logger << "Should have reached end of stream, but got status "
               << status << " while reading partial packets." << std::ends;
         }
      }
   }

   // Information used in the next function about where we are in each stream.
   struct FORCING_INFO : public PACKET_DATA_T
   {
      void new_packet()
      {
         first_valid_pts = std::numeric_limits<PTS_T>::min();
         first_valid_dts = std::numeric_limits<PTS_T>::min();
         total_flags = 0;

         // Adjust the buffer to hold the amount we want.
         // (note - resize smaller does not re-allocate,
         // it'll grow until it reaches the biggest packet then stay at that size)
         buffer.resize(info.buffer_size);

         info.data = &buffer[0];
      }

      PTS_T first_valid_pts;
      PTS_T first_valid_dts;
      uint32_t total_flags;
      ALL_PACKETS_T::const_iterator position;
   };

   // Check forcing. Read bits of the streams so that they get out of sync, and make sure the data is correct.
   void check_forcing()
   {
      // Data for each stream
      std::map<STREAM_T, FORCING_INFO> stream_positions;
      std::map<STREAM_T, FORCING_INFO>::iterator stream_pos;

      // Set stream_positions to the first packet for each stream
      for (std::set<STREAM_T>::const_iterator stream = all_streams.begin();
         stream != all_streams.end();
         ++stream)
      {
         const STREAM_T stream_number = *stream;
         FORCING_INFO stream_data;
         stream_data.position = all_packets.begin();

         // Insert an entry for this stream, starting at the beginning
         stream_pos = stream_positions.insert(std::make_pair(stream_number, stream_data)).first;

         // Then search for a packet in this stream.
         next_in_stream(stream_pos);

         stream_pos->second.info.track = stream_number;
      }

      // Repeat until explicit break when all have reached last packet
      bool alldone = false;
      while (!alldone)
      {
         // go through each stream in turn and get some data. At end of packet check it.
         for (stream_pos = stream_positions.begin(); stream_pos != stream_positions.end(); ++stream_pos)
         {
            FORCING_INFO& stream_data = stream_pos->second;
            if (stream_data.position == all_packets.end())
            {
               // This stream has reached the end of the expected packets. Force-read an info,
               // to make sure it really is at the end.
               status = vc_container_read(p_ctx, &stream_data.info,
                  VC_CONTAINER_READ_FLAG_INFO | VC_CONTAINER_READ_FLAG_FORCE_TRACK);

               if (status != VC_CONTAINER_ERROR_EOS)
               {
                  error_logger << "Reading from stream " << stream_pos->first
                     << " after end gave error " << status << "instead of EOS" << std::ends;

                  // Erase the buffer so we don't repeat the check. We'd just end up with loads of errors.
                  stream_positions.erase(stream_pos);

                  // Then reset the iterator, as that made it invalid.
                  stream_pos = stream_positions.begin();
               }
            }
            else
            {
               // This stream has not reached the end. Read some more, maybe check it.

               // If we haven't read anything in yet
               if (stream_data.info.size == 0)
               {
                  if (configuration.packet_buffer_size <= 0)
                  {
                     // if unspecified read whole packets.
                     stream_data.info.buffer_size = stream_data.position->info.size;
                  }
                  else if (configuration.packet_buffer_size < 1.0)
                  {
                     // If a proportion work it out
                     stream_data.info.buffer_size
                        = (uint32_t)(configuration.packet_buffer_size * stream_data.position->info.size);

                     if (stream_data.info.buffer_size < 1)
                     {
                        // but never less than 1 byte
                        stream_data.info.buffer_size = 1;
                     }
                  }
                  else
                  {
                     // Use the amount specified
                     stream_data.info.buffer_size = (uint32_t)configuration.packet_buffer_size;
                  }

                  stream_data.new_packet();
               }
               else
               {
                  // We've already read part of the packet. Read some more.
                  size_t read_so_far = stream_data.buffer.size();

                  // expand the buffer to hold another lump of data
                  stream_data.buffer.resize(read_so_far + stream_data.info.buffer_size);

                  // point the read at the new part.
                  stream_data.info.data = &stream_data.buffer[read_so_far];
               }

               // Read some data
               status = vc_container_read(p_ctx, &stream_data.info, VC_CONTAINER_READ_FLAG_FORCE_TRACK);

               stream_data.total_flags |= stream_data.info.flags;
               // Save the PTS if we don't have one yet
               if (stream_data.first_valid_pts < 0)
               {
                  stream_data.first_valid_pts = stream_data.info.pts;
               }

               // Ditto the DTS
               if (stream_data.first_valid_dts < 0)
               {
                  stream_data.first_valid_dts = stream_data.info.dts;
               }

               // work out how much we have.
               uint32_t total_read = (stream_data.info.data + stream_data.info.size) - &stream_data.buffer[0];

               if (total_read > stream_data.position->info.size)
               {
                  // we've read more than a packet. That's an error.
                  error_logger << "Read more data than expected from a packet - read " << total_read
                     << " Expect " << stream_data.position->info.size;
               }

               if (total_read < stream_data.position->info.size)
               {
                  // more to read. Just go back around the loop.
                  continue;
               }

               stream_data.info.size = total_read;

               stream_data.crc = crc32buf(&stream_data.buffer[0], total_read);

               stream_data.info.flags = stream_data.total_flags;
               stream_data.info.pts = stream_data.first_valid_pts;
               stream_data.info.dts = stream_data.first_valid_dts;

               // Validate that the data is the data we found when we did the sequential reads.
               std::string anyerror = compare_packets(*stream_data.position, stream_data);

               if (!anyerror.empty())
               {
                  error_logger << "Incorrect data found at PTS " << stream_data.info.pts << anyerror << std::ends;
               }

               // Move to the next packet
               ++stream_data.position;

               // Then search until we get one for this stream, or end.
               next_in_stream(stream_pos);

               // Note we've read nothing of the new packet yet.
               stream_data.info.size = 0;
            }
         }

         // go through each stream in turn. If they have all reached the end we are complete.
         for (alldone = true, stream_pos = stream_positions.begin(); alldone && stream_pos != stream_positions.end(); ++stream_pos)
         {
            if (stream_pos->second.position != all_packets.end())
            {
               // If any stream has not reached the end we have more to do
               alldone = false;
            }
         }
      }
   }

   // Helper for the above: Advance the supplied FORCING_INFO to the next packet in the stream
   void next_in_stream(std::map<STREAM_T, FORCING_INFO>::iterator& pos)
   {
      // We're searching for this stream
      const STREAM_T track = pos->first;

      ALL_PACKETS_T::const_iterator& position = pos->second.position;

      // Loop until no more packets, or found one for this stream
      while ((position != all_packets.end())
         && (position->info.track != track))
      {
         // advance this iterator to the next packet.
         ++position;
      }
   };

   // Compare two packets, and return a non-empty string if they don't match
   std::string compare_packets(const PACKET_DATA_T& expected, const PACKET_DATA_T& actual)
   {
      std::ostringstream errors;

      // Check the fields in the structure
      if (expected.info.size != actual.info.size)
      {
         errors << "size mismatch. Expect " << expected.info.size << " actual " << actual.info.size << std::endl;
      }

      if (expected.info.frame_size != actual.info.frame_size)
      {
         errors << "frame_size mismatch. Expect " << expected.info.frame_size << " actual " << actual.info.frame_size << std::endl;
      }

      if (expected.info.pts != actual.info.pts)
      {
         errors << "pts mismatch. Expect " << expected.info.pts << " actual " << actual.info.pts << std::endl;
      }

      if (expected.info.dts != actual.info.dts)
      {
         errors << "dts mismatch. Expect " << expected.info.dts << " actual " << actual.info.dts << std::endl;
      }

      if (expected.info.track != actual.info.track)
      {
         errors << "track mismatch. Expect " << expected.info.track << " actual " << actual.info.track << std::endl;
      }

      if (expected.info.flags != actual.info.flags)
      {
         errors << "flags mismatch. Expect " << expected.info.flags << " actual " << actual.info.flags << std::endl;
      }

      // check the buffer. We won't bother if there isn't one in either - if the expected hasn't got one,
      // it's probably because we hit our memory limit. If the actual hasn't, the there should be a size error.
      for (size_t i = 0, err_count = 0; i < expected.buffer.size() && i < actual.buffer.size(); ++i)
      {
         if (expected.buffer[i] != actual.buffer[i])
         {
            errors << "Data mismatch at "
               << std::setw(8) << std::hex << i
               << std::setw(3) << (unsigned)expected.buffer[i]
               << std::setw(3)<< (unsigned)actual.buffer[i] << std::endl;

            if (++err_count > 20)
            {
               errors << "Too many errors to report" << std::endl;
               break;
            }
         }
      }

      if (expected.crc != actual.crc)
      {
         errors << "CRC mismatch. Expect " << expected.crc << " actual " << actual.crc << std::endl;
      }

      // If there were errors put a new line on the front. This aids formatting.
      const std::string& error_list = errors.str();

      if (error_list.empty())
      {
         // There were no errors
         return error_list;
      }
      else
      {
         return std::string("\n") + error_list;
      }
   }

   void check_correct_seek(VC_CONTAINER_SEEK_FLAGS_T direction, PTS_T actual_pts, PTS_T target_pts)
   {
      if (status != VC_CONTAINER_SUCCESS)
      {
         error_logger << "Error " << status << " returned by seek" << std::ends;
      }
      if (direction)
      {
         if (actual_pts < target_pts)
         {
            // We shouldn't normally arrive at a time earlier than we requested.
            // However there's a special case - when the time requested is after the last keyframe
            PTS_T highest_pts_in_video = all_key_packets.at(video_stream).rbegin()->first;

            if (actual_pts < highest_pts_in_video)
            {
               error_logger << "Incorrect forward seek. Should not be before "
                  << target_pts << " but arrived at " << actual_pts << std::ends;
            }
         }
      }
      else
      {
         if (actual_pts > target_pts)
         {
            // We shouldn't normally arrive at a time later than we requested.
            // However there's a special case - when the time requested is before the first keyframe in the file.
            PTS_T lowest_pts_in_video = all_key_packets.at(video_stream).begin()->first;

            if (actual_pts > lowest_pts_in_video)
            {
               error_logger << "Incorrect reverse seek. Should not be after "
                  << target_pts << " but arrived at " << actual_pts << std::ends;
            }
         }
      }
   }

   // Check whether a packet is close enough to the PTS supplied. Used after a seek.
   void check_seek_tolerance(const PACKET_DATA_T& packet, PTS_T actual_pts)
   {
      // Check whether the time on the packet is reasonable - it ought to be close to the time achieved.
      if (packet.info.track == video_stream)
      {
         if ((packet.info.pts + configuration.tolerance_video_early) < actual_pts)
         {
            error_logger << "Video stream seek location is bad " << actual_pts - packet.info.pts << "uS early" << std::ends;
         }

         if ((packet.info.pts - configuration.tolerance_video_late) > actual_pts)
         {
            // We shouldn't normally arrive at a time later than we requested.
            // However there's a special case - when the time requested is before the first PTS in the file.
            PTS_T lowest_pts_in_video = all_pts_packets.at(video_stream).begin()->first;

            if (actual_pts > lowest_pts_in_video)
            {
               error_logger << "Video stream seek location is bad " <<  packet.info.pts - actual_pts << "uS late" << std::ends;
            }
         }
      }
      else
      {
         if ((packet.info.pts + configuration.tolerance_other_early) < actual_pts)
         {
            error_logger << "Non-video stream seek location is bad " << actual_pts - packet.info.pts << "uS early" << std::ends;
         }

         if ((packet.info.pts - configuration.tolerance_other_late) > actual_pts)
         {
            error_logger << "Non-video stream seek location is bad " <<  packet.info.pts - actual_pts << "uS late" << std::ends;
         }
      }
   }

   // Initialise a Congruential Random Number Generator from the file contents.
   // Used rather than rand() for the complete control that this offers - this app
   // should behave the same regardless of the platform and word size.
   void init_crg()
   {
      if (all_packets.empty())
      {
         static const char* msg = "You must read some packets before initialising the RNG";
         assert(!msg);
         std::cerr << msg;
         exit(1);
      }

      rng_value = 0;

      // XOR all the CRCs together to act as a file-specific seed.
      for (ALL_PACKETS_T::const_iterator packet = all_packets.begin(); packet != all_packets.end(); ++packet)
      {
         rng_value ^= packet->crc;
      }
   }

   // Get a pseudo-random number.
   uint32_t rand()
   {
      // Constants from "Numerical recipes in 'C'"
      return rng_value = 1664525 * rng_value + 1013904223;
   }

   // configuration
   const CONFIGURATION_T configuration;

   // Error logger.
   class ERROR_LOGGER_T error_logger;

   // The status from the last call made
   VC_CONTAINER_STATUS_T status;

   // Pointer to the file being processed
   VC_CONTAINER_T *p_ctx;

   // All the packets in the file
   ALL_PACKETS_T all_packets;

   // All the streams
   std::set<STREAM_T> all_streams;

   // All the streams and all the packets in them with a PTS, whether or not they are keyframes.
   STREAM_TIMED_PACKETS_T all_pts_packets;

   // Subset of the above that hold key frames.
   STREAM_TIMED_PACKETS_T all_key_packets;

   // Places where a seek without VC_CONTAINER_SEEK_FLAG_FORWARD can go to. A subset of all_key_packets.
   STREAM_TIMED_PACKETS_T reverse_index_positions;

   // Places where a seek with VC_CONTAINER_SEEK_FLAG_FORWARD can go to.
   STREAM_TIMED_PACKETS_T forward_index_positions;

   // The first stream containing video packets
   STREAM_T video_stream;

   // The stored value of the RNG
   uint32_t rng_value;
};

int main(int argc, char** argv)
{
   // Read and parse the configuration information from the command line.
   TESTER_T test(argc, argv);

   // Run the tests and return their error code
   return test.run();
}
