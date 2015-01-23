#include <cassert>
#include <iostream>

#include <poolqueue/Promise.hpp>

using poolqueue::Promise;

int main() {
   // A Promise is default-constructible.
   Promise p0;
   assert(!p0.settled());

   // The key Promise method is then(), which returns a new dependent
   // Promise.
   Promise p1 = p0.then(
      // onFulfil callback
      [](const std::string& s) {
         std::cout << "fulfilled with " << s << '\n';
      },
      // onReject callback
      [](const std::exception_ptr& e) {
         try {
            if (e)
               std::rethrow_exception(e);
         }
         catch (const std::exception& e) {
            std::cout << "rejected with " << e.what() << '\n';
         }
      });
   assert(!p1.settled());

   // Settling a Promise invokes the appropriate callback (if present)
   // and recursively settles dependent Promises.
   if (true)
      p0.settle(std::string("foo"));
   else {
      // Here is how you would settle with an exception. Change the if
      // condition from true to false to see what happens.
      p0.settle(std::make_exception_ptr(std::runtime_error("bad")));
   }
   assert(p0.settled());
   assert(p1.settled());

#if 0
   // ERROR - A Promise cannot be settled more than once.
   p0.settle(std::string("bar"));

   // ERROR - settle() can never be called on a dependent Promise
   // (i.e. one returned by then() or except() methods) even if it has
   // not been settled.
   p1.settle(std::string("baz"));
#endif
             
   return 0;
}
