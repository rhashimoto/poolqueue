#include <cassert>
#include <iostream>
#include <thread>

#include <poolqueue/Delay.hpp>

using poolqueue::Delay;
using poolqueue::Promise;

int main() {
   // Delay::after() returns a Promise that fulfils no sooner than the
   // duration argument.
   auto bgnTime = std::chrono::steady_clock::now();
   Delay::after(std::chrono::milliseconds(100))
      .then(
         [=]() {
            auto endTime = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - bgnTime);
            assert(elapsed >= std::chrono::milliseconds(100));
            std::cout << "actual delay " << elapsed.count() << " seconds\n";
         },
         []() {
            // Would get here if cancelled.
         });
#if 0
   // ERROR - Don't attach long-running callbacks to Delay
   // Promises. The Delay implementation uses a single thread for
   // settling which means that callbacks that require significant
   // time to execute may cause other delays to be triggered later
   // than expected.
   Delay::after(std::chrono::milliseconds(100))
      .then(
         [=]() {
            std::this_thread::sleep_for(std::chrono::hours(24));
         });
       
#endif
   
   // Passing a Promise returned by Delay::after() to Delay::cancel()
   // returns true if it successfully rejects it. All pending Promises
   // in the Delay queue will be rejected when the program exits.
   Promise p = Delay::after(std::chrono::seconds(60));
   p.then(
      []() {
         // Would get here if not cancelled.
      },
      [](const std::exception_ptr& e) {
         // This output will likely (almost certainly) be seen before
         // the previous delay fulfils.
         std::cout << "delay cancelled\n";
      });

   Delay::cancel(p);

   // Just wait for everything to finish. This would be very sloppy
   // for a real program - a real program would likely block until
   // it had everything it expected before exiting - but the purpose
   // here is just to show how to use Delay.
   std::this_thread::sleep_for(std::chrono::seconds(1));
   return 0;
}
