#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE ThreadPool

#include <iostream>
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

   for (int i = 1; i < 100; ++i) {
      tp.setThreadCount(i);
      BOOST_CHECK_EQUAL(tp.getThreadCount(), i);
      
      for (int j = 0; j < 2*i; ++j)
         tp.post([]() {});
   }

   tp.synchronize().wait();
   tp.setThreadCount(nThreads);
}
