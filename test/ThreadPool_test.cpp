#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE ThreadPool

#include <iostream>
#include <mutex>
#include <set>
#include <thread>
#include <boost/format.hpp>
#include <boost/test/unit_test.hpp>

#include "ThreadPool.hpp"

BOOST_AUTO_TEST_CASE(basic) {
   poolqueue::ThreadPool tp;
   BOOST_CHECK_EQUAL(tp.index(), -1);

   int count = 0;
   std::mutex exclusive;
   tp.post([&]() {
         std::lock_guard<std::mutex> lock(exclusive);
         BOOST_CHECK_GE(tp.index(), 0);
         ++count;
      });
   tp.dispatch([&]() {
         std::lock_guard<std::mutex> lock(exclusive);
         BOOST_CHECK_GE(tp.index(), 0);
         ++count;
      });
   tp.wrap([&]() {
         std::lock_guard<std::mutex> lock(exclusive);
         BOOST_CHECK_GE(tp.index(), 0);
         ++count;
      })();

   tp.synchronize().wait();
   BOOST_CHECK_EQUAL(count, 3);
}

BOOST_AUTO_TEST_CASE(promise) {
   poolqueue::ThreadPool tp;
   
   bool complete = false;
   tp.post(
      [&tp]() {
         BOOST_CHECK_GE(tp.index(), 0);
         return 42;
      })
      .then([&complete](int i) {
         BOOST_CHECK_EQUAL(i, 42);
         complete = true;
         });

   tp.synchronize().wait();
   BOOST_CHECK(complete);
}

BOOST_AUTO_TEST_CASE(post) {
   poolqueue::ThreadPool tp;
   
   // Make sure that threads can post.
   std::promise<void> promise;
   tp.post([&tp, &promise]() {
         tp.post([&promise]() {
               promise.set_value();
            });
      });

   promise.get_future().wait();
   tp.synchronize().wait();
}

BOOST_AUTO_TEST_CASE(dispatch) {
   poolqueue::ThreadPool tp;
   
   // Verify synchronous dispatch.
   tp.post([&tp]() {
         static int value;
         value = 0xdeadbeef;
         std::thread::id tid = std::this_thread::get_id();

         tp.dispatch([&]() {
               BOOST_CHECK_EQUAL(value, 0xdeadbeef);
               BOOST_CHECK_EQUAL(tid, std::this_thread::get_id());
            });

         value = 0;
      });

   tp.synchronize().wait();
}

BOOST_AUTO_TEST_CASE(count) {
   poolqueue::ThreadPool tp;
   
   const int nThreads = tp.getThreadCount();
   BOOST_CHECK_THROW(tp.setThreadCount(0), std::invalid_argument);
   BOOST_CHECK_THROW(tp.setThreadCount(-1), std::invalid_argument);

   std::mutex mutex;
   for (int i = 1; i < 32; ++i) {
      tp.setThreadCount(i);
      BOOST_CHECK_EQUAL(tp.getThreadCount(), i);

      // Try to run a function on every thread. This isn't guaranteed
      // to work but hopefully it will.
      std::set<std::thread::id> threadIds;
      for (int j = 0; j < 2*i; ++j) {
         tp.post([&]() {
               std::unique_lock<std::mutex> lock(mutex);
               const auto index = tp.index();
               BOOST_CHECK_GE(index, 0);
               BOOST_CHECK_LT(index, i);
               threadIds.insert(std::this_thread::get_id());
               lock.unlock();

               std::this_thread::sleep_for(std::chrono::milliseconds(10));
            });
      }
      
      tp.synchronize().wait();
      BOOST_WARN_EQUAL(threadIds.size(), i);
   }

   tp.setThreadCount(nThreads);
}

BOOST_AUTO_TEST_CASE(performance) {
   poolqueue::ThreadPool tp;

   // Measure how quickly n null functions can be queued and executed.
   size_t n = 1;
   auto elapsed = std::chrono::microseconds(0);
   do {
      auto bgnTime = std::chrono::steady_clock::now();
      for (size_t i = 0; i < n; ++i) {
         tp.post([]() {
            });
      }
      tp.synchronize().wait();
      auto endTime = std::chrono::steady_clock::now();

      elapsed = std::chrono::duration_cast<decltype(elapsed)>(endTime - bgnTime);
      std::cout << boost::format("%12d functions in %.6f seconds\n")
         % n
         % static_cast<double>(elapsed.count()/1000000.0);

      n *= 2;
   } while (elapsed < std::chrono::seconds(1));
}
