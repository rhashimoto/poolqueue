#include <atomic>
#include <cassert>
#include <iostream>

#include <poolqueue/Promise.hpp>

using poolqueue::Promise;

int main() {
   // Ordinarily when a Promise callback returns a value, the Promise
   // is fulfilled with that value. The exception is when a callback
   // returns a Promise. In that case, the original Promise (with the
   // callback) is settled with the result of the returned Promise
   // (which might occur at a later time.

   // This is the Promise that the callback will return.
   Promise inner;

   // We use this Promise to trigger the chain.
   Promise head;

   std::atomic<bool> outerCallbackDone(false);
   Promise outer = head.then([=, &outerCallbackDone](int i) {
         assert(i == 10);
         std::cout << "outer returning Promise\n";
         outerCallbackDone = true;
         return inner;
      });

   outer.then([](const std::string& s) {
         assert(s == "foo");
         std::cout << "outer fulfilled\n";
      });

   // At this point, nothing is settled.
   assert(!head.settled());
   assert(!inner.settled());
   assert(!outer.settled());
   assert(!outerCallbackDone);
   
   // When we settle the head Promise, the outer callback runs and
   // returns inner. Because inner is not settled, outer is also not
   // settled.
   head.settle(10);
   assert(!inner.settled());
   assert(!outer.settled());
   assert(outerCallbackDone);

   // When we settle the inner Promise, the outer Promise is also
   // settled.
   inner.settle(std::string("foo"));
   assert(inner.settled());
   assert(outer.settled());
   assert(outerCallbackDone);

   return 0;
}
