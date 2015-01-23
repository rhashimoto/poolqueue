#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Promise

#include <iostream>
#include <set>
#include <unordered_set>
#include <boost/test/unit_test.hpp>

#include "Promise.hpp"

using poolqueue::Promise;

BOOST_AUTO_TEST_CASE(callback) {
   using namespace poolqueue::detail;

   const std::string ArgumentValue = "how now brown cow";
   const int ResultValue = 42;

   {
      // std::string f(std::string)
      std::unique_ptr<CallbackWrapper> cb(makeCallbackWrapper([=](std::string value) {
               BOOST_CHECK_EQUAL(value, ArgumentValue);
               return ResultValue;
            }));
      BOOST_CHECK(!cb->hasRvalueArgument());
      BOOST_CHECK(!cb->hasExceptionPtrArgument());
      
      Any a(ArgumentValue);
      Any result = (*cb)(std::move(a));
      BOOST_CHECK_EQUAL(a.cast<decltype(ArgumentValue)>(), ArgumentValue);
      BOOST_CHECK_EQUAL(result.cast<decltype(ResultValue)>(), ResultValue);
   }

   {
      // std::string f(const std::string&)
      std::unique_ptr<CallbackWrapper> cb(makeCallbackWrapper([=](const std::string& value) {
               BOOST_CHECK_EQUAL(value, ArgumentValue);
               return ResultValue;
            }));
      BOOST_CHECK(!cb->hasRvalueArgument());
      BOOST_CHECK(!cb->hasExceptionPtrArgument());
      
      Any a(ArgumentValue);
      Any result = (*cb)(std::move(a));
      BOOST_CHECK_EQUAL(a.cast<decltype(ArgumentValue)>(), ArgumentValue);
      BOOST_CHECK_EQUAL(result.cast<decltype(ResultValue)>(), ResultValue);
   }

   {
      // std::string f(std::string&&)
      std::unique_ptr<CallbackWrapper> cb(makeCallbackWrapper([=](std::string&& value) {
               BOOST_CHECK_EQUAL(value, ArgumentValue);
               std::string s;
               s.swap(value);
               return ResultValue;
            }));
      BOOST_CHECK(cb->hasRvalueArgument());
      BOOST_CHECK(!cb->hasExceptionPtrArgument());
      
      Any a(ArgumentValue);
      Any result = (*cb)(std::move(a));
      BOOST_CHECK_EQUAL(a.cast<decltype(ArgumentValue)>(), decltype(ArgumentValue)());
      BOOST_CHECK_EQUAL(result.cast<decltype(ResultValue)>(), ResultValue);
   }

   {
      // void f(std::string)
      std::unique_ptr<CallbackWrapper> cb(makeCallbackWrapper([=](std::string value) {
               BOOST_CHECK_EQUAL(value, ArgumentValue);
            }));
      BOOST_CHECK(!cb->hasRvalueArgument());
      BOOST_CHECK(!cb->hasExceptionPtrArgument());
      
      Any a(ArgumentValue);
      Any result = (*cb)(std::move(a));
      BOOST_CHECK_EQUAL(a.cast<decltype(ArgumentValue)>(), ArgumentValue);
      BOOST_CHECK(result.empty());
   }

   {
      // int f()
      std::unique_ptr<CallbackWrapper> cb(makeCallbackWrapper([=]() {
               return ResultValue;
            }));
      BOOST_CHECK(!cb->hasRvalueArgument());
      BOOST_CHECK(!cb->hasExceptionPtrArgument());
      
      Any a(ArgumentValue);
      Any result = (*cb)(std::move(a));
      BOOST_CHECK_EQUAL(a.cast<decltype(ArgumentValue)>(), ArgumentValue);
      BOOST_CHECK_EQUAL(result.cast<decltype(ResultValue)>(), ResultValue);
   }

   {
      // void f()
      std::unique_ptr<CallbackWrapper> cb(makeCallbackWrapper([=]() {
            }));
      BOOST_CHECK(!cb->hasRvalueArgument());
      BOOST_CHECK(!cb->hasExceptionPtrArgument());
      
      Any a(ArgumentValue);
      Any result = (*cb)(std::move(a));
      BOOST_CHECK_EQUAL(a.cast<decltype(ArgumentValue)>(), ArgumentValue);
      BOOST_CHECK(result.empty());
   }

   {
      // R f(const Any&)
      std::unique_ptr<CallbackWrapper> cb(makeCallbackWrapper([=](const Promise::Value& a) {
               BOOST_CHECK_EQUAL(a.cast<std::string>(), ArgumentValue);
               return ResultValue;
            }));
      BOOST_CHECK(!cb->hasRvalueArgument());
      BOOST_CHECK(!cb->hasExceptionPtrArgument());
      
      Any a(ArgumentValue);
      Any result = (*cb)(std::move(a));
      BOOST_CHECK_EQUAL(a.cast<decltype(ArgumentValue)>(), ArgumentValue);
      BOOST_CHECK_EQUAL(result.cast<decltype(ResultValue)>(), ResultValue);
   }

   {
      // void f(const Any&)
      std::unique_ptr<CallbackWrapper> cb(makeCallbackWrapper([=](const Promise::Value& a) {
               BOOST_CHECK_EQUAL(a.cast<std::string>(), ArgumentValue);
            }));
      BOOST_CHECK(!cb->hasRvalueArgument());
      BOOST_CHECK(!cb->hasExceptionPtrArgument());
      
      Any a(ArgumentValue);
      Any result = (*cb)(std::move(a));
      BOOST_CHECK_EQUAL(a.cast<decltype(ArgumentValue)>(), ArgumentValue);
      BOOST_CHECK(result.empty());
   }

   {
      // R f(Any&&)
      std::unique_ptr<CallbackWrapper> cb(makeCallbackWrapper([=](Promise::Value&& a) {
               BOOST_CHECK_EQUAL(a.cast<std::string>(), ArgumentValue);
               Any tmp;
               tmp.swap(a);
               return ResultValue;
            }));
      BOOST_CHECK(cb->hasRvalueArgument());
      BOOST_CHECK(!cb->hasExceptionPtrArgument());
      
      Any a(ArgumentValue);
      Any result = (*cb)(std::move(a));
      BOOST_CHECK(a.empty());
      BOOST_CHECK_EQUAL(result.cast<decltype(ResultValue)>(), ResultValue);
   }

   {
      // void f(Any&&)
      std::unique_ptr<CallbackWrapper> cb(makeCallbackWrapper([=](Promise::Value&& a) {
               BOOST_CHECK_EQUAL(a.cast<std::string>(), ArgumentValue);
               Any tmp;
               tmp.swap(a);
            }));
      BOOST_CHECK(cb->hasRvalueArgument());
      BOOST_CHECK(!cb->hasExceptionPtrArgument());
      
      Any a(ArgumentValue);
      Any result = (*cb)(std::move(a));
      BOOST_CHECK(a.empty());
      BOOST_CHECK(result.empty());
   }

   {
      // std::string f(const std::exception_ptr&)
      std::unique_ptr<CallbackWrapper> cb(makeCallbackWrapper([=](const std::exception_ptr& e) {
               return ResultValue;
            }));
      BOOST_CHECK(!cb->hasRvalueArgument());
      BOOST_CHECK(cb->hasExceptionPtrArgument());
      
      Any a(std::make_exception_ptr(std::runtime_error("foo")));
      Any result = (*cb)(std::move(a));
      BOOST_CHECK_EQUAL(result.cast<decltype(ResultValue)>(), ResultValue);
   }

}

BOOST_AUTO_TEST_CASE(value) {
   Promise::Value v;
   BOOST_CHECK(v.empty());

   Promise::Value i(42);
   BOOST_CHECK(i.type() == typeid(int));
   BOOST_CHECK_EQUAL(i.cast<int>(), 42);
   BOOST_CHECK_EQUAL(i.cast<int&>(), 42);
   BOOST_CHECK_EQUAL(i.cast<const int&>(), 42);
   BOOST_CHECK_THROW(i.cast<std::string>(), std::bad_cast);
   
   const Promise::Value ci(42);
   BOOST_CHECK(ci.type() == typeid(int));
   BOOST_CHECK_EQUAL(ci.cast<int>(), 42);
   BOOST_CHECK_EQUAL(ci.cast<const int&>(), 42);
   BOOST_CHECK_THROW(ci.cast<std::string>(), std::bad_cast);
   
   Promise::Value s(std::string("foo"));
   BOOST_CHECK(s.type() == typeid(std::string));
   BOOST_CHECK_EQUAL(s.cast<std::string>(), "foo");
   BOOST_CHECK_THROW(s.cast<int>(), std::bad_cast);

   Promise::Value iCopy(i);
   BOOST_CHECK(i.type() == typeid(int));
   BOOST_CHECK_EQUAL(i.cast<int>(), 42);
   BOOST_CHECK(iCopy.type() == typeid(int));
   BOOST_CHECK_EQUAL(iCopy.cast<int>(), 42);

   Promise::Value iMove(std::move(i));
   BOOST_CHECK(i.empty());
   BOOST_CHECK(iCopy.type() == typeid(int));
   BOOST_CHECK_EQUAL(iCopy.cast<int>(), 42);

   i = 14;
   BOOST_CHECK(i.type() == typeid(int));
   BOOST_CHECK_EQUAL(i.cast<int>(), 14);
   
   s = std::string("bar");
   BOOST_CHECK(s.type() == typeid(std::string));
   BOOST_CHECK_EQUAL(s.cast<std::string>(), "bar");
   BOOST_CHECK_EQUAL(s.cast<const std::string&>(), "bar");
   BOOST_CHECK_EQUAL(const_cast<const Promise::Value&>(s).cast<std::string>(), "bar");
   BOOST_CHECK_EQUAL(const_cast<const Promise::Value&>(s).cast<const std::string&>(), "bar");

   s.cast<std::string&>() = "foobar";
   BOOST_CHECK_EQUAL(s.cast<std::string>(), "foobar");

   Promise::Value a;
   Promise::Value b(42);
   int *ptr = &b.cast<int&>();
   
   using std::swap;
   swap(a, b);

   BOOST_CHECK_EQUAL(a.cast<int>(), 42);
   BOOST_CHECK_EQUAL(&a.cast<int&>(), ptr);
   BOOST_CHECK(b.empty());
}

BOOST_AUTO_TEST_CASE(basic) {
   {
      Promise p;
      BOOST_CHECK(!p.settled());
   
      p.settle(42);
      BOOST_CHECK(p.settled());
   }

   Promise::ExceptionHandler originalHandler = Promise::setUndeliveredExceptionHandler([](const std::exception_ptr&) {});
   {

      Promise p;
      BOOST_CHECK(!p.settled());
      p.settle(std::make_exception_ptr(0));
      BOOST_CHECK(p.settled());

   }
   Promise::setUndeliveredExceptionHandler(originalHandler);
   
   {
      bool complete = false;
      Promise p(
         [&](int value) {
            BOOST_CHECK_EQUAL(value, 42);
            complete = true;
         });

      BOOST_CHECK(!complete);

      p.settle(42);
      BOOST_CHECK(complete);
   }

   {
      bool complete = false;
      Promise p(
         [&](int value) {
            BOOST_CHECK_EQUAL(value, 42);
            complete = true;
         },
         [&]() {
            BOOST_CHECK(false);
         });

      BOOST_CHECK(!complete);

      p.settle(42);
      BOOST_CHECK(complete);
   }

   {
      bool complete = false;
      Promise p(
         [&](int value) {
            BOOST_CHECK(false);
         },
         [&](const std::exception_ptr& e) {
            BOOST_CHECK_THROW(std::rethrow_exception(e), std::runtime_error);
            complete = true;
         });

      BOOST_CHECK(!complete);

      p.settle(std::make_exception_ptr(std::runtime_error("")));
      BOOST_CHECK(complete);
   }
}

static bool thenCalled = false;
static void then_function() {
   thenCalled = true;
}

BOOST_AUTO_TEST_CASE(basic_then) {
   Promise p;
   p.settle(42);
   
   int coverage = 0;
   p.then([&](int) {
         ++coverage;
         return 0;
      });
   BOOST_CHECK_EQUAL(coverage, 1);

   p.then([&]() {
         ++coverage;
         return 0;
      });
   BOOST_CHECK_EQUAL(coverage, 2);

   p.then([&](int) {
         ++coverage;
      });
   BOOST_CHECK_EQUAL(coverage, 3);

   p.then([&]() {
         ++coverage;
      });
   BOOST_CHECK_EQUAL(coverage, 4);

   p.then(&then_function);
   BOOST_CHECK(thenCalled);
   
   p.except([&](const std::exception_ptr&) {
         BOOST_CHECK(false);
      });

   BOOST_CHECK_THROW(p.then([](float) {}), std::bad_cast);
   BOOST_CHECK_THROW(p.settle(0), std::logic_error);
}

static bool exceptCalled = false;
static void except_function(const std::exception_ptr&) {
   exceptCalled = true;
}

BOOST_AUTO_TEST_CASE(basic_except) {
   Promise p([]() { throw 0; });
   p.settle();
   
   int coverage = 0;
   p.except([&](const std::exception_ptr&) {
         ++coverage;
         return 0;
      });
   BOOST_CHECK_EQUAL(coverage, 1);

   p.except([&](const std::exception_ptr&) {
         ++coverage;
      });
   BOOST_CHECK_EQUAL(coverage, 2);

   p.except(&except_function);
   BOOST_CHECK(exceptCalled);
   
   bool handlerCalled = false;
   Promise::ExceptionHandler originalHandler = Promise::setUndeliveredExceptionHandler([&](const std::exception_ptr&) {
         handlerCalled = true;
      });
   p.then([&]() {
         BOOST_CHECK(false);
      });
   BOOST_CHECK(handlerCalled);
   Promise::setUndeliveredExceptionHandler(originalHandler);
   
   BOOST_CHECK_THROW(p.settle(0), std::logic_error);
}

BOOST_AUTO_TEST_CASE(biway_then) {
   {
      Promise p;

      bool fulfilled = false;
      bool rejected = false;
      Promise q = p.then(
         [&](int i) {
            fulfilled = true;
            return i;
         },
         [&](const std::exception_ptr&) {
            rejected = true;
            return 13;
         });

      bool done = false;
      q.then([&](int i) {
            done = true;
            BOOST_CHECK_EQUAL(i, 42);
         });

      p.settle(42);
      BOOST_CHECK(fulfilled);
      BOOST_CHECK(!rejected);
      BOOST_CHECK(done);
   }
}

BOOST_AUTO_TEST_CASE(chain) {
   Promise p;

   int coverage = 0;
   Promise tail = p
      .then([&](int i) {
            BOOST_CHECK_EQUAL(i, 0);
            ++coverage;
            return 1;
         })
      .then([&](int i) {
            BOOST_CHECK_EQUAL(i, 1);
            ++coverage;
            throw std::runtime_error("");
         })
      .then([&]() {
            // Won't get here because exception was thrown.
            BOOST_CHECK(false);
         })
      .except([&](const std::exception_ptr& e) {
            ++coverage;
            return 2;
         })
      .except([&](const std::exception_ptr& e) {
            // Won't get here because exception was already delivered.
            BOOST_CHECK(false);
            return 0;
         })
      .then([&](int i) {
            BOOST_CHECK_EQUAL(i, 2);
            ++coverage;
         })
      ;
   BOOST_CHECK(!tail.settled());
   BOOST_CHECK_EQUAL(coverage, 0);

   p.settle(0);
   BOOST_CHECK(tail.settled());
   BOOST_CHECK_EQUAL(coverage, 4);
}

BOOST_AUTO_TEST_CASE(subpromise) {
   Promise inner;

   int coverage = 0;
   Promise().settle(0)
      .then([&](int i) {
            BOOST_CHECK_EQUAL(i, 0);
            ++coverage;
            return inner;
         })
      .then([&](int i) {
            BOOST_CHECK_EQUAL(i, 1);
            ++coverage;

            throw std::runtime_error("");
         })
      .except([&](const std::exception_ptr&) {
            ++coverage;
            return inner;
         })
      .then([&](int i) {
            BOOST_CHECK_EQUAL(i, 1);
            ++coverage;
         })
      ;

   // Second then callback waiting on inner promise.
   BOOST_CHECK_EQUAL(coverage, 1);

   inner.settle(1);
   BOOST_CHECK_EQUAL(coverage, 4);
}

namespace {
   struct NonCopyable {
      static int nInstances;
   
      NonCopyable() {
         ++nInstances;
      }

      NonCopyable(const NonCopyable&) = delete;
      NonCopyable(NonCopyable&&) = default;

      NonCopyable& operator=(const NonCopyable&) = delete;
      NonCopyable& operator=(NonCopyable&&) = default;
   };
   int NonCopyable::nInstances = 0;
}

BOOST_AUTO_TEST_CASE(rvalue) {
   Promise p;

   // Non rvalue reference arguments.
   p.then([](const std::string& s) {
         BOOST_CHECK(!s.empty());
      });
   BOOST_CHECK(!p.closed());
   p.then([](std::string s) {
         BOOST_CHECK(!s.empty());
      });
   BOOST_CHECK(!p.closed());

   // rvalue reference argument.
   p.then([](std::string&& s) {
         BOOST_CHECK(!s.empty());
         std::string tmp(std::move(s));
         std::cout << tmp << '\n';
         BOOST_CHECK(s.empty());
      });
   BOOST_CHECK(p.closed());

   // No more callbacks allowed.
   BOOST_CHECK_THROW(p.then([](const std::string&) {}), std::logic_error);
   BOOST_CHECK_THROW(p.except([](const std::exception_ptr&) {}), std::logic_error);

   // Verify that no accidental copies are made.
   int coverage = 0;
   Promise().settle(NonCopyable())
      .then([&](NonCopyable&& arg) {
            ++coverage;
            return NonCopyable(std::move(arg));
         })
      .then([&](NonCopyable&& arg) {
            ++coverage;
            return NonCopyable(std::move(arg));
         })
      .then([&](NonCopyable&& arg) {
            ++coverage;
            return NonCopyable(std::move(arg));
         });
   BOOST_CHECK_EQUAL(coverage, 3);
   BOOST_CHECK_EQUAL(NonCopyable::nInstances, 1);
}

BOOST_AUTO_TEST_CASE(key) {
   std::set<Promise>{};
   std::unordered_set<Promise>{};
}

BOOST_AUTO_TEST_CASE(all) {
   {
      std::vector<Promise> v;

      size_t complete = 0;
      Promise all = Promise::all(v.begin(), v.end());
      all.then([&]() {
            for (size_t i = 0; i < v.size(); ++i) {
               v[i].then([=, &complete](size_t value) {
                     BOOST_CHECK_EQUAL(value, i);
                     ++complete;
                  });
            }
         });

      for (size_t i = 0; i < v.size(); ++i)
         v[i].settle(i);

      BOOST_CHECK_EQUAL(complete, v.size());
   }
   
   {
      std::vector<Promise> v;
      for (int i = 0; i < 4; ++i)
         v.push_back(Promise());

      size_t complete = 0;
      Promise all = Promise::all(v.begin(), v.end());
      all.then([&]() {
            for (size_t i = 0; i < v.size(); ++i) {
               v[i].then([=, &complete](size_t value) {
                     BOOST_CHECK_EQUAL(value, i);
                     ++complete;
                  });
            }
         });

      for (size_t i = 0; i < v.size(); ++i) {
         v[i].settle(i);
         if (i + 1 < v.size())
            BOOST_CHECK(!all.settled());
         else
            BOOST_CHECK(all.settled());
      }
      BOOST_CHECK_EQUAL(complete, v.size());
   }

   {
      std::vector<Promise> v;
      for (int i = 0; i < 4; ++i)
         v.push_back(Promise());

      bool complete = false;
      Promise all = Promise::all(v.begin(), v.end());
      all.except([&](const std::exception_ptr& e) {
            try {
               std::rethrow_exception(e);
            }
            catch (const std::runtime_error& error) {
               BOOST_CHECK_EQUAL(error.what(), std::string("foo"));
            }
            complete = true;
         });

      for (size_t i = 1; i < v.size(); ++i)
         v[i].settle(i);
      BOOST_CHECK(!all.settled());
      
      v[0].settle(std::make_exception_ptr(std::runtime_error("foo")));
      BOOST_CHECK(all.settled());            
      BOOST_CHECK(complete);
   }

   {
      Promise p0, p1, p2, p3;

      int complete = 0;
      Promise all = Promise::all({p0, p1, p2, p3});
      all.then([&]() {
            p0.then([&](int value) {
                  BOOST_CHECK_EQUAL(value, 0);
                  ++complete;
               });
            p1.then([&](int value) {
                  BOOST_CHECK_EQUAL(value, 1);
                  ++complete;
               });
            p2.then([&](int value) {
                  BOOST_CHECK_EQUAL(value, 2);
                  ++complete;
               });
            p3.then([&](int value) {
                  BOOST_CHECK_EQUAL(value, 3);
                  ++complete;
               });
         });

      p0.settle(0);
      BOOST_CHECK_EQUAL(complete, 0);
      p1.settle(1);
      BOOST_CHECK_EQUAL(complete, 0);
      p2.settle(2);
      BOOST_CHECK_EQUAL(complete, 0);
      p3.settle(3);
      BOOST_CHECK_EQUAL(complete, 4);
   }
}
