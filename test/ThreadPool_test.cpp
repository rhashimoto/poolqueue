#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE ThreadPool

#include <iostream>
#include <boost/test/unit_test.hpp>

#include "ThreadPool.hpp"

using poolqueue::ThreadPool;

BOOST_AUTO_TEST_CASE(basic) {
   BOOST_CHECK_EQUAL(ThreadPool::threadId(), -1);

   int count = 0;
   std::mutex exclusive;
   ThreadPool::post([&]() {
         std::lock_guard<std::mutex> lock(exclusive);
         BOOST_CHECK_GE(ThreadPool::threadId(), 0);
         ++count;
      });
   ThreadPool::dispatch([&]() {
         std::lock_guard<std::mutex> lock(exclusive);
         BOOST_CHECK_GE(ThreadPool::threadId(), 0);
         ++count;
      });
   ThreadPool::wrap([&]() {
         std::lock_guard<std::mutex> lock(exclusive);
         BOOST_CHECK_GE(ThreadPool::threadId(), 0);
         ++count;
      })();

   ThreadPool::synchronize().wait();
   BOOST_CHECK_EQUAL(count, 3);
}

BOOST_AUTO_TEST_CASE(promise) {
   bool complete = false;
   ThreadPool::post(
      [&]() {
         BOOST_CHECK_GE(ThreadPool::threadId(), 0);
         return 42;
      })
      .then([&](int i) {
         BOOST_CHECK_EQUAL(i, 42);
         complete = true;
         });

   ThreadPool::synchronize().wait();
   BOOST_CHECK(complete);
}

BOOST_AUTO_TEST_CASE(post) {
   // Make sure that threads can post.
   std::promise<void> promise;
   ThreadPool::post([&promise]() {
         ThreadPool::post([&promise]() {
               promise.set_value();
            });
      });

   promise.get_future().wait();
   ThreadPool::synchronize().wait();
}

BOOST_AUTO_TEST_CASE(dispatch) {
   // Verify synchronous dispatch.
   ThreadPool::post([]() {
         static int value;
         value = 0xdeadbeef;
         std::thread::id tid = std::this_thread::get_id();

         ThreadPool::dispatch([&]() {
               BOOST_CHECK_EQUAL(value, 0xdeadbeef);
               BOOST_CHECK_EQUAL(tid, std::this_thread::get_id());
            });

         value = 0;
      });

   ThreadPool::synchronize().wait();
}

BOOST_AUTO_TEST_CASE(count) {
   const int nThreads = ThreadPool::getThreadCount();
   BOOST_CHECK_THROW(ThreadPool::setThreadCount(0), std::invalid_argument);
   BOOST_CHECK_THROW(ThreadPool::setThreadCount(-1), std::invalid_argument);

   for (int i = 1; i < 100; ++i) {
      ThreadPool::setThreadCount(i);
      BOOST_CHECK_EQUAL(ThreadPool::getThreadCount(), i);
      
      for (int j = 0; j < 2*i; ++j)
         ThreadPool::post([]() {});
   }

   ThreadPool::synchronize().wait();
   ThreadPool::setThreadCount(nThreads);
}
