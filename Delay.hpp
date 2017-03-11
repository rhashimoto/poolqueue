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
   // This class contains static member functions to create a Promise
   // that is fulfilled after the specified delay.
   //
   // Any outstanding Delay instances will be cancelled when static
   // object are destroyed after main() exits. This can result in
   // undelivered exceptions if no reject handlers are attached.
   class Delay {
   public:
      struct cancelled : public std::exception {
         const char* what() const noexcept override {
            return "Delayed promise has been cancelled";
         }
      };

      // Instantiate a delay.
      // @duration Any std::chrono::duration, e.g. std::chrono::hours(2).
      //
      // This static function returns a Promise that will be fulfilled
      // no sooner than the duration argument.
      //
      // @return Promise fulfilled at expiration or rejected if cancelled.
      template<typename T>
      static Promise after(const T& duration) {
         Promise p;
         schedule(p, std::chrono::duration_cast<std::chrono::steady_clock::duration>(duration));
         return p;
      }

      // Cancel a delay.
      // @p Promise previously returned by Delay::after().
      // @e Optional exception to reject the Promise.
      //
      // This function cancels a Promise returned by after(),
      // returning true if successful. The Promise will be rejected
      // with e, or Delay::cancelled if omitted.
      //
      // @return true if cancel was successful.
      static bool cancel(const Promise& p, const std::exception_ptr& e = std::make_exception_ptr(cancelled()));
         
   private:
      static void schedule(
         const Promise& p,
         const std::chrono::steady_clock::duration& duration);
   };

}

#endif // poolqueue_Delay_hpp
