#pragma once

#include <algorithm>
#include <random>
#include <stdint.h>
#include <time.h>

struct Backoff
{
   int64_t minAmount;
   int64_t maxAmount;
   int64_t current;
   std::mt19937_64 randGenerator;
   std::uniform_real_distribution<> randDistribution;

   Backoff(int64_t min, int64_t max)
      : minAmount(min)
        , maxAmount(max)
        , current(min)
        , randGenerator((uint64_t)time(0))
   {
   }

   void reset()
   {
      current = minAmount;
   }

   int64_t nextDelay()
   {
      int64_t delay = (int64_t)((double)current * 2.0 * 
            randDistribution(randGenerator));
      current = std::min(current + delay, maxAmount);
      return current;
   }
};
