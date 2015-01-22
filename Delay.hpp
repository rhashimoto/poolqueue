/*
Copyright 2015 Shoestring Research, LLC.  All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#ifndef poolqueue_Delay_hpp
#define poolqueue_Delay_hpp

#include <chrono>

#include "Promise.hpp"

namespace poolqueue {

   // Enable timed callbacks.
   //
   // This class contains static member functions to create and cancel
   // asynchronous one-shot delays using Promises.
   //
   // Any outstanding Delay instances will be cancelled during static
   // object destruction. This can result in undelivered exceptions
   // if no reject handlers are attached.
   class Delay {
   public:
      struct cancelled : public std::exception {
      };
      
      // Instantiate a delay.
      // @duration Any std::chrono::duration, e.g. std::chrono::hours(2).
      //
      // This static function returns a Promise that will be resolved
      // no sooner than the duration argument.
      //
      // @return Promise resolved at expiration or rejected if cancelled.
      template<typename T>
      static Promise create(const T& duration) {
         Promise p;
         createImpl(std::chrono::duration_cast<std::chrono::steady_clock::duration>(duration), p);
         return p;
      }

      // Cancel a delay.
      // @p Promise previously returned by Delay::create().
      // @e Exception to reject the Promise.
      //
      // This function cancels the delayed Promise returned by
      // create(), returning true if successful. The Promise will be
      // rejected with the optional exception_ptr argument, or
      // Delay::cancelled if missing.
      //
      // @return true if cancel was successful.
      static bool cancel(const Promise& p, const std::exception_ptr& e = std::make_exception_ptr(cancelled()));
         
   private:
      static void createImpl(
         const std::chrono::steady_clock::duration& duration,
         const Promise& p);
   };

}

#endif // poolqueue_Delay_hpp
