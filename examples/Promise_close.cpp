#include <atomic>
#include <cassert>
#include <exception>
#include <iostream>

#include <poolqueue/Promise.hpp>

using poolqueue::Promise;

// Movable but not copyable class.
struct Uncopyable {
   Uncopyable() {}
   Uncopyable(Uncopyable&&) {
      std::cout << "move constructor\n";
   }

   Uncopyable(const Uncopyable&) = delete;
   Uncopyable& operator=(const Uncopyable&) = delete;
};

int main() {
   // A closed Promise cannot have then() or except() called.
   // A Promise may be closed explicitly with the close() method:
   Promise p0;
   p0.close();
   assert(p0.closed());

#if 0
   // DON'T DO THIS - Cannot call then() or except() after a
   // Promise is closed.
   p0.then([]() { });
   p0.except([]() { });
#endif

   // A Promise may be closed implicitly by calling then() with an
   // onResolve callback that takes an rvalue reference argument:
   Promise p1;
   p1.then([](Uncopyable&& s) {
         Uncopyable local(std::move(s));
      });
   assert(p1.closed());

#if 0
   // DON'T DO THIS - Cannot call then() or except() after a
   // Promise is closed.
   p1.then([]() { });
   p1.except([]() { });
#endif

   // Rvalue references can be used to avoid copies, though copying is
   // necessary in certain scenarios. A closed Promise may settle
   // slightly faster because it can avoid locking a mutex.
   Uncopyable foo;
   p1.settle(std::move(foo));
   
   return 0;
}
