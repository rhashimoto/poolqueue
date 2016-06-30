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

   // Fulfil with the first Promise to fulfil using Promise::any().
   // There are two overloads: iterators and initializer_list.
   Promise pA = Promise::any(promises.begin(), promises.end());
   Promise pB = Promise::any({promises[0], promises[1]});
   
   // A any Promise fulfils when any input Promise fulfils. The
   // result is propagated from the first input Promise to fulfil.
   assert(!pA.settled());
   promises[2].settle(std::make_exception_ptr(std::runtime_error("1st reject")));
   assert(!pA.settled());
   promises[4].settle(std::string("foo"));
   assert(pA.settled());

   // It doesn't matter what any remaining Promises do.
   promises[3].settle(std::string("bar"));
   pA.then([](const std::string& s) {
      std::cout << "pA fulfils with " << s << '\n';
      return nullptr;
   });

   // A any Promise rejects when all input Promises reject (in any
   // order).
   assert(!pB.settled());
   promises[1].settle(std::make_exception_ptr(std::runtime_error("2nd reject")));
   assert(!pB.settled());
   promises[0].settle(std::make_exception_ptr(std::runtime_error("3rd reject")));
   assert(pB.settled());
   pB.except([]() {
      std::cout << "pB rejects\n";
      return nullptr;
   });
   
   return 0;
}
