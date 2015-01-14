// Copyright 2014 Shoestring Research, LLC.  All rights reserved.
#ifndef poolqueue_Delay_hpp
#define poolqueue_Delay_hpp

#include <chrono>

#include "Promise.hpp"

namespace poolqueue {

   class Delay {
   public:
      // This function returns a Promise that will be resolved
      // after a minimum duration.
      template<typename T>
      static Promise create(const T& duration) {
         Promise p;
         createImpl(std::chrono::duration_cast<std::chrono::steady_clock::duration>(duration), p);
         return p;
      }

      // This function cancels the delayed Promise returned by
      // create(), returning true if successful. The Promise
      // will be rejected with the exception_ptr argument.
      static bool cancel(const Promise& p, const std::exception_ptr& e = std::exception_ptr());
         
   private:
      static void createImpl(
         const std::chrono::steady_clock::duration& duration,
         const Promise& p);
   };

}

#endif // poolqueue_Delay_hpp
