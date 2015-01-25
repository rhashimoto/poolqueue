#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>

#include <poolqueue/ThreadPool.hpp>

using poolqueue::ThreadPool;
using poolqueue::Promise;

int main() {
   // The default size of the pool is the number of hardware threads.
   std::cout << ThreadPool::getThreadCount() << " default threads\n";

   // The size of the pool can be changed if desired.
   ThreadPool::setThreadCount(5);
   assert(ThreadPool::getThreadCount() == 5);
   std::cout << "Changed to " << ThreadPool::getThreadCount() << " threads\n";
#if 0
   // ERROR - Don't change the thread count from within a pool thread.
   ThreadPool::post([]() {
         ThreadPool::setThreadCount(1);
      });
#endif

   // Use post() to queue a function to execute in the pool.
   std::cout << "main() thread " << std::this_thread::get_id() << '\n';
   ThreadPool::post([]() {
         std::cout << "function running in thread " << std::this_thread::get_id() << '\n';
      });

#if 0
   // ERROR - Functions to execute in the pool should not have an argument.
   ThreadPool::post([](const std::string& s) {
      });
#endif
   
   // index() returns the thread index in the pool, -1 if not a
   // pool thread.
   assert(ThreadPool::index() == -1);
   ThreadPool::post([]() {
         const auto index = ThreadPool::index();
         assert(index >= 0 && index < ThreadPool::getThreadCount());
         std::cout << "posted thread index " << index << '\n';
      });

   // dispatch() will execute its function argument immediately if
   // calling thread is in the pool; otherwise it will call post().
   ThreadPool::dispatch([]() {
         const auto index = ThreadPool::index();
         assert(index >= 0 && index < ThreadPool::getThreadCount());
         std::cout << "dispatched thread index " << index << '\n';
      });
   
   ThreadPool::post([]() {
         // Calling dispatch() here invokes the function synchronously.
         std::cout << "calling dispatch() from " << ThreadPool::index() << "...\n";
         ThreadPool::dispatch([]() {
               std::cout << "...executes synchronously on " << ThreadPool::index() << '\n';
            });
      });

   // wrap() transforms a function into a new function that dispatches
   // the original function.
   auto wrapped = ThreadPool::wrap([]() {
         std::cout << "wrapped function on " << ThreadPool::index() << '\n';
      });
   wrapped();
   
   // post() and dispatch() return a Promise that settles with the
   // result of the function.
   Promise p0 = ThreadPool::post([]() {
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

   Promise p1 = ThreadPool::dispatch([]() {
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
   ThreadPool::synchronize();
   ThreadPool::post([]() {
         // When this executes, which may not be until after the
         // below synchronize() call returns, it will not overlap
         // with any other ThreadPool function.
      });
   ThreadPool::synchronize();

   // synchronize() does return a std::shared_future<void> that *can*
   // be used to block until the pool is idle.
   std::atomic<int> counter(0);
   for (int i = 0; i < 4; ++i) {
      ThreadPool::post([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            ++counter;
         });
   }

   std::shared_future<void> future = ThreadPool::synchronize();
   future.wait();
   assert(counter == 4);
#if 0
   // ERROR - Do not block inside a ThreadPool thread.
   ThreadPool::post([]() {
         ThreadPool::synchronize().wait();
      });
#endif
   
   return 0;
}
