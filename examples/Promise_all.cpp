#include <cassert>
#include <iostream>
#include <vector>

#include <poolqueue/Promise.hpp>

using poolqueue::Promise;

int main() {
   // Promises work with STL containers.
   std::vector<Promise> promises(5);
   for (auto& p : promises)
      assert(!p.settled());

   // Synchronize with multiple Promises using Promise::all().  There
   // are two overloads: initializer_list and iterators.
   Promise pA = Promise::all({promises[0], promises[1]});
   Promise pB = Promise::all(promises.begin(), promises.end());
   
   // An all Promise fulfils when all input Promises fulfil (in
   // any order).
   assert(!pA.settled());
   promises[1].settle(std::string("foo"));
   assert(!pA.settled());
   promises[0].settle(std::string("bar"));
   assert(pA.settled());
   pA.then([]() {
         std::cout << "pA fulfils\n";
      });

#if 0
   // DON'T DO THIS - onFulfil callbacks attached to a Promise::all()
   // Promise should not take an argument.
   pA.then([](const std::string& s) {
      });
#endif
   
   // An all Promise rejects when any input Promise rejects. The
   // result is propagated from the first input Promise to reject.
   assert(!pB.settled());
   promises[3].settle(std::make_exception_ptr(std::runtime_error("1st reject")));
   assert(pB.settled());

   // It doesn't matter what any remaining Promises do.
   promises[2].settle(std::make_exception_ptr(std::runtime_error("2nd reject")));
   assert(!promises[4].settled());
   pB.except([](const std::exception_ptr& e) {
         try {
            if (e)
               std::rethrow_exception(e);
         }
         catch (const std::exception& e) {
            std::cout << "pB rejects with " << e.what() << '\n';
         }
      });
   
   return 0;
}
