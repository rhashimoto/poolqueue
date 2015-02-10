#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>

#include <poolqueue/ThreadPool.hpp>

using poolqueue::ThreadPool;
using poolqueue::Promise;

int main() {
   ThreadPool tp;
   
   // The default size of the pool is the number of hardware threads.
   std::cout << tp.getThreadCount() << " default threads\n";

   // The size of the pool can be changed if desired.
   tp.setThreadCount(5);
   assert(tp.getThreadCount() == 5);
   std::cout << "Changed to " << tp.getThreadCount() << " threads\n";
#if 0
   // DON'T DO THIS - Don't change the thread count from within a pool
   // thread.
   tp.post([&tp]() {
         tp.setThreadCount(1);
      });
#endif

   // Use post() to queue a function to execute in the pool.
   std::cout << "main() thread " << std::this_thread::get_id() << '\n';
   tp.post([]() {
         std::cout << "function running in thread " << std::this_thread::get_id() << '\n';
      });

#if 0
   // DON'T DO THIS - Functions to execute in the pool should not have
   // an argument.
   tp.post([](const std::string& s) {
      });
#endif
   
   // index() returns the thread index in the pool, -1 if not a
   // pool thread.
   assert(tp.index() == -1);
   tp.post([&tp]() {
         const auto index = tp.index();
         assert(index >= 0 && index < tp.getThreadCount());
         std::cout << "posted thread index " << index << '\n';
      });

   // dispatch() will execute its function argument immediately if
   // calling thread is in the pool; otherwise it will call post().
   tp.dispatch([&tp]() {
         const auto index = tp.index();
         assert(index >= 0 && index < tp.getThreadCount());
         std::cout << "dispatched thread index " << index << '\n';
      });
   
   tp.post([&tp]() {
         // Calling dispatch() here invokes the function synchronously.
         std::cout << "calling dispatch() from " << tp.index() << "...\n";
         tp.dispatch([&tp]() {
               std::cout << "...executes synchronously on " << tp.index() << '\n';
            });
      });

   // wrap() transforms a function into a new function that dispatches
   // the original function.
   auto wrapped = tp.wrap([&tp]() {
         std::cout << "wrapped function on " << tp.index() << '\n';
      });
   wrapped();
   
   // post() and dispatch() return a Promise that settles with the
   // result of the function.
   Promise p0 = tp.post([]() {
         return std::string("foo");
      });

   p0.then([](const std::string& s) {
         // Be aware that chained Promises will not necessarily
         // execute on a ThreadPool thread (though they often will).
         // If the returned Promise p0 is settled before then() is
         // called, this function will execute on the current thread.
         //
         // If you really want callbacks to execute in the pool then
         // you must invoke them with a ThreadPool method.
         std::cout << "posted function returned " << s << '\n';
      });

   Promise p1 = tp.dispatch([]() {
         throw std::runtime_error("bar");
      });

   p1.except([](const std::exception_ptr& e) {
         // Be aware that chained Promises will not necessarily
         // execute on a ThreadPool thread (though they often will).
         try {
            if (e)
               std::rethrow_exception(e);
         }
         catch (const std::exception& e) {
            std::cout << "dispatched function threw " << e.what() << '\n';
         }
      });

   // synchronize() is an asynchronous barrier. It ensures that any
   // functions queued on the ThreadPool before the call complete
   // before any functions queued after the call begin. However, by
   // itself it does *not* block - i.e. there is no guarantee what
   // is or is not executing at the moment synchronize() returns.
   tp.synchronize();
   tp.post([]() {
         // When this executes, which may not be until after the
         // below synchronize() call returns, it will not overlap
         // with any other ThreadPool function.
      });
   tp.synchronize();

   // synchronize() does return a std::shared_future<void> that *can*
   // be used to block until the pool is idle.
   std::atomic<int> counter(0);
   for (int i = 0; i < 4; ++i) {
      tp.post([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            ++counter;
         });
   }

   std::shared_future<void> future = tp.synchronize();
   future.wait();
   assert(counter == 4);
#if 0
   // DON'T DO THIS - Do not block inside a ThreadPool thread.
   tp.post([&tp]() {
         tp.synchronize().wait();
      });
#endif
   
   return 0;
}
