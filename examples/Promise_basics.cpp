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
         std::cout << "p1 fulfilled with " << s << '\n';
      },
      // onReject callback
      [](const std::exception_ptr& e) {
         try {
            if (e)
               std::rethrow_exception(e);
         }
         catch (const std::exception& e) {
            std::cout << "p1 rejected with " << e.what() << '\n';
         }
      });
   assert(!p1.settled());

   // Settling a Promise invokes the appropriate callback (if present)
   // and recursively settles dependent Promises.
   p0.settle(std::string("foo"));
   assert(p0.settled());
   assert(p1.settled());

#if 0
   // DON'T DO THIS - A Promise cannot be settled more than once.
   p0.settle(std::string("bar"));

   // DON'T DO THIS - settle() can never be called on a dependent
   // Promise (i.e. one returned by then() or except() methods) even
   // if it has not been settled.
   p1.settle(std::string("baz"));

   // DON'T DO THIS - The onResolve argument type must match the
   // upstream type.
   p0.then(
      // onFulfil callback
      [](int i) { // <-- type mismatch
      },
      // onReject callback
      [](const std::exception_ptr& e) {
      });
#endif

   // then() can be called more than once on a Promise. It is also okay
   // to call then() on a settled Promise - in that case the dependent
   // Promise settles immediately.
   Promise p2 = p0.then(
      // onFulfil callback
      [](const std::string& s) {
         std::cout << "p2 fulfilled with " << s << '\n';
         return 0;
      },
      // onReject callback
      [](const std::exception_ptr& e) {
         try {
            if (e)
               std::rethrow_exception(e);
         }
         catch (const std::exception& e) {
            std::cout << "p2 rejected with " << e.what() << '\n';
         }
         return -1;
      });
   assert(p0.settled());
   assert(p2.settled());

#if 0
   // DON'T DO THIS - onFulfil and onReject cannot have different
   // return types.
   p0.then(
      // onFulfil callback
      [](const std::string& s) {
         return 0;
      },
      // onReject callback
      [](const std::exception_ptr& e) {
         return std::string("abc");
      });
#endif
   
   // A non-dependent Promise can be created with callbacks.
   Promise p3(
      // onFulfil callback
      [](const std::string& s) {
         std::cout << "p3 fulfilled with " << s << '\n';
         return 0;
      },
      // onReject callback
      [](const std::exception_ptr& e) {
         try {
            if (e)
               std::rethrow_exception(e);
         }
         catch (const std::exception& e) {
            std::cout << "p3 rejected with " << e.what() << '\n';
         }
         return -1;
      });
   assert(!p3.settled());

   // Here is how you would settle with an exception.
   p3.settle(std::make_exception_ptr(std::runtime_error("bad")));
   assert(p3.settled());

   // However, the way an exception is typically created is a
   // callback throws it.
   Promise p4 = p0.then(
      // onFulfil callback
      [](const std::string& s) {
         std::cout << "p4 fulfilled with " << s << '\n';

         if ("some error occurs")
            throw std::runtime_error("sample error");
         return 0;
      },
      // onReject callback
      [](const std::exception_ptr& e) {
         // This callback won't get exceptions from the same Promise,
         // only from upstream Promises. Only one callback on a
         // Promise can execute.
         try {
            if (e)
               std::rethrow_exception(e);
         }
         catch (const std::exception& e) {
            std::cout << "p4 rejected with " << e.what() << '\n';
         }
         return -1;
      });
   assert(p4.settled());
   
   // Dependent Promises will get the exception.
   Promise p5 = p4.then(
      // onFulfil callback
      [](int i) {
         std::cout << "p5 fulfilled with " << i << '\n';
         return 0;
      },
      // onReject callback
      [](const std::exception_ptr& e) {
         try {
            if (e)
               std::rethrow_exception(e);
         }
         catch (const std::exception& e) {
            std::cout << "p5 rejected with " << e.what() << '\n';
         }
         return -1;
      });
   assert(p5.settled());
   
   // A Promise can have just one callback. The onReject argument
   // to then() can be omitted...
   Promise p6 = p4.then(
      // onFulfil callback
      [](int i) {
         // In this case the callback will not be invoked because
         // p4's callback threw an exception.
         std::cout << "p6 fulfilled with " << i << '\n';
         std::unexpected();
         return 0;
      });
   assert(p4.settled());
   assert(p6.settled());

   // ...and except() works the same as then() but does not take
   // an onFulfil argument.
   //
   // Because p6 was missing the callback that would have been
   // invoked, it simply carries forward the result from its
   // upstream Promise p4.
   Promise p7 = p6.except(
      // onReject callback
      [](const std::exception_ptr& e) {
         try {
            if (e)
               std::rethrow_exception(e);
         }
         catch (const std::exception& e) {
            std::cout << "p7 rejected with " << e.what() << '\n';
         }
      });
   assert(p6.settled());
   assert(p7.settled());

   // Callbacks can always omit their arguments, even if
   // the upstream type is non-void.
   Promise p8 = p0.then(
      // onFulfil callback
      []() {
         std::cout << "p8 fulfilled (argument omitted)\n";
      },
      // onReject callback
      []() {
         std::cout << "p8 rejected (argument omitted)\n";
      });

   // Here's what happens if a callback returns a Promise.
   Promise p9;
   Promise p10 = Promise().settle().then(
      [=]() {
         return p9;
      });
   assert(!p9.settled());
   assert(!p10.settled());
#if 0
   // DON'T DO THIS - A returned Promise does not propagate like other
   // types. A callback should never take a Promise argument.
   p10.then([](const Promise& value) {
      });
#endif

   // Dependent Promises will be settled with the result
   // of the returned Promise, whenever the returned
   // Promise settles. That could be either immediately
   // (i.e. if the returned Promise is already settled)
   // or sometime in the future.
   p9.settle(std::string("bar"));
   assert(p9.settled());
   assert(p10.settled());
   
   Promise p11 = p10.then(
      [](const std::string& s) {
         std::cout << "p11 fulfilled with " << s << '\n';
      });
   
   return 0;
}
