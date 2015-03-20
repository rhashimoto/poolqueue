#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Promise

#include <iostream>
#include <set>
#include <unordered_set>
#include <boost/test/unit_test.hpp>

#include "Promise.hpp"

using poolqueue::Promise;

static int f_string_to_int(const std::string& s) {
   return 42;
}

static void f_string_to_void(const std::string& s) {
}

static int f_void_to_int() {
   return 42;
}

static void f_void_to_void() {
}

BOOST_AUTO_TEST_CASE(callback) {
   using namespace poolqueue::detail;

   const std::string ArgumentValue = "how now brown cow";
   const int ResultValue = 42;

   {
      std::unique_ptr<CallbackWrapper> cb(makeCallbackWrapper(&f_string_to_int));
      BOOST_CHECK(!cb->hasRvalueArgument());
      BOOST_CHECK(!cb->hasExceptionPtrArgument());

      Any a(ArgumentValue);
      Any result = (*cb)(std::move(a));
      BOOST_CHECK_EQUAL(a.cast<decltype(ArgumentValue)>(), ArgumentValue);
      BOOST_CHECK_EQUAL(result.cast<decltype(ResultValue)>(), ResultValue);
   }
   
   {
      std::unique_ptr<CallbackWrapper> cb(makeCallbackWrapper(&f_string_to_void));
      BOOST_CHECK(!cb->hasRvalueArgument());
      BOOST_CHECK(!cb->hasExceptionPtrArgument());

      Any a(ArgumentValue);
      Any result = (*cb)(std::move(a));
      BOOST_CHECK_EQUAL(a.cast<decltype(ArgumentValue)>(), ArgumentValue);
      BOOST_CHECK(result.empty());
   }
   
   {
      std::unique_ptr<CallbackWrapper> cb(makeCallbackWrapper(&f_void_to_int));
      BOOST_CHECK(!cb->hasRvalueArgument());
      BOOST_CHECK(!cb->hasExceptionPtrArgument());

      Any a(ArgumentValue);
      Any result = (*cb)(std::move(a));
      BOOST_CHECK_EQUAL(a.cast<decltype(ArgumentValue)>(), ArgumentValue);
      BOOST_CHECK_EQUAL(result.cast<decltype(ResultValue)>(), ResultValue);
   }
   
   {
      std::unique_ptr<CallbackWrapper> cb(makeCallbackWrapper(&f_void_to_void));
      BOOST_CHECK(!cb->hasRvalueArgument());
      BOOST_CHECK(!cb->hasExceptionPtrArgument());

      Any a(ArgumentValue);
      Any result = (*cb)(std::move(a));
      BOOST_CHECK_EQUAL(a.cast<decltype(ArgumentValue)>(), ArgumentValue);
      BOOST_CHECK(result.empty());
   }
   
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
   // Default constructed.
   Promise::Value v;
   BOOST_CHECK(v.empty());

   // Constructed with simple type.
   Promise::Value i(42);
   BOOST_CHECK(i.type() == typeid(int));
   BOOST_CHECK_EQUAL(i.cast<int>(), 42);
   BOOST_CHECK_EQUAL(i.cast<int&>(), 42);
   BOOST_CHECK_EQUAL(i.cast<const int&>(), 42);
   BOOST_CHECK_THROW(i.cast<std::string>(), std::bad_cast);

   // Const instance.
   const Promise::Value ci(42);
   BOOST_CHECK(ci.type() == typeid(int));
   BOOST_CHECK_EQUAL(ci.cast<int>(), 42);
   BOOST_CHECK_EQUAL(ci.cast<const int&>(), 42);
   BOOST_CHECK_THROW(ci.cast<std::string>(), std::bad_cast);

   // Constructed with class type.
   Promise::Value s(std::string("foo"));
   BOOST_CHECK(s.type() == typeid(std::string));
   BOOST_CHECK_EQUAL(s.cast<std::string>(), "foo");
   BOOST_CHECK_THROW(s.cast<int>(), std::bad_cast);

   // Copy constructor.
   Promise::Value iCopy(i);
   BOOST_CHECK(i.type() == typeid(int));
   BOOST_CHECK_EQUAL(i.cast<int>(), 42);
   BOOST_CHECK(iCopy.type() == typeid(int));
   BOOST_CHECK_EQUAL(iCopy.cast<int>(), 42);

   // Move constructor.
   Promise::Value iMove(std::move(i));
   BOOST_CHECK(i.empty());
   BOOST_CHECK(iMove.type() == typeid(int));
   BOOST_CHECK_EQUAL(iMove.cast<int>(), 42);

   // Copy assignment.
   i = 94;
   iCopy = i;
   BOOST_CHECK_EQUAL(iCopy.cast<int>(), 94);
   BOOST_CHECK_EQUAL(i.cast<int>(), 94);

   // Move assignment.
   i = 49;
   iMove = std::move(i);
   BOOST_CHECK_EQUAL(iMove.cast<int>(), 49);
   BOOST_CHECK_EQUAL(i.cast<int>(), 42);
   
   // Value assignment of simple type.
   i = 14;
   BOOST_CHECK(i.type() == typeid(int));
   BOOST_CHECK_EQUAL(i.cast<int>(), 14);

   // Value assignment of class type.
   s = std::string("bar");
   BOOST_CHECK(s.type() == typeid(std::string));
   BOOST_CHECK_EQUAL(s.cast<std::string>(), "bar");
   BOOST_CHECK_EQUAL(s.cast<const std::string&>(), "bar");
   BOOST_CHECK_EQUAL(const_cast<const Promise::Value&>(s).cast<std::string>(), "bar");
   BOOST_CHECK_EQUAL(const_cast<const Promise::Value&>(s).cast<const std::string&>(), "bar");

   // Assign to held value.
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

BOOST_AUTO_TEST_CASE(constructors) {
   {
      // Default constructor.
      Promise p;
      BOOST_CHECK(!p.settled());
      BOOST_CHECK(!p.closed());
   }

   {
      Promise p = Promise().settle(42);
      BOOST_CHECK(p.settled());
      BOOST_CHECK(!p.closed());

      // Copy constructor.
      Promise pCopied(p);
      BOOST_CHECK(p.settled());
      BOOST_CHECK(!p.closed());
      BOOST_CHECK(pCopied.settled());
      BOOST_CHECK(!pCopied.closed());
      
      pCopied.then([](int value) {
            BOOST_CHECK_EQUAL(value, 42);
         });
   }

   {
      Promise p = Promise().settle(42).close();
      BOOST_CHECK(p.settled());
      BOOST_CHECK(p.closed());

      // Copy constructor.
      Promise pCopied(p);
      BOOST_CHECK(p.settled());
      BOOST_CHECK(p.closed());
      BOOST_CHECK(pCopied.settled());
      BOOST_CHECK(pCopied.closed());
   }

   {
      Promise p = Promise().settle(17);
      BOOST_CHECK(p.settled());
      BOOST_CHECK(!p.closed());
      
      // Move constructor.
      Promise pMoved(std::move(p));
      BOOST_CHECK(!p.settled());
      BOOST_CHECK(!p.closed());
      BOOST_CHECK(pMoved.settled());
      BOOST_CHECK(!pMoved.closed());
      
      pMoved.then([](int value) {
            BOOST_CHECK_EQUAL(value, 17);
         });
   }

   {
      Promise p = Promise().settle(17).close();
      BOOST_CHECK(p.settled());
      BOOST_CHECK(p.closed());
      
      // Move constructor.
      Promise pMoved(std::move(p));
      BOOST_CHECK(!p.settled());
      BOOST_CHECK(!p.closed());
      BOOST_CHECK(pMoved.settled());
      BOOST_CHECK(pMoved.closed());
   }

   {
      // Callback constructor + fulfil.
      bool success = false;
      Promise p(
         [&](const std::string& s) {
            BOOST_CHECK_EQUAL(s, "foo");
            success = true;
         },
         [](const std::exception_ptr& e) {
            BOOST_CHECK(false);
         });
      BOOST_CHECK(!p.settled());
      BOOST_CHECK(!p.closed());
      BOOST_CHECK(!success);

      p.settle(std::string("foo"));
      BOOST_CHECK(p.settled());
      BOOST_CHECK(!p.closed());
      BOOST_CHECK(success);
   }

   {
      // Callback constructor + reject and return values.
      bool success = false;
      Promise p(
         [](const std::string& s) {
            BOOST_CHECK(false);
            return 0;
         },
         [&](const std::exception_ptr& e) {
            try {
               std::rethrow_exception(e);
            }
            catch(const std::exception& e) {
               BOOST_CHECK_EQUAL(e.what(), std::string("bar"));
               success = true;
               return -1;
            }
         });
      BOOST_CHECK(!p.settled());
      BOOST_CHECK(!p.closed());
      BOOST_CHECK(!success);

      p.settle(std::make_exception_ptr(std::runtime_error("bar")));
      BOOST_CHECK(p.settled());
      BOOST_CHECK(!p.closed());
      BOOST_CHECK(success);
   }

   {
      // Callback constructor with elided onReject.
      bool success = false;
      Promise p(
         [&](const std::string& s) {
            BOOST_CHECK_EQUAL(s, "foo");
            success = true;
         });
      BOOST_CHECK(!p.settled());
      BOOST_CHECK(!p.closed());
      BOOST_CHECK(!success);

      p.settle(std::string("foo"));
      BOOST_CHECK(p.settled());
      BOOST_CHECK(!p.closed());
      BOOST_CHECK(success);
   }

   {
      // Callback constructor + no arguments.
      bool success = false;
      Promise p(
         [&]() {
            success = true;
         },
         []() {
            BOOST_CHECK(false);
         });
      BOOST_CHECK(!p.settled());
      BOOST_CHECK(!p.closed());
      BOOST_CHECK(!success);

      p.settle(std::string("foo"));
      BOOST_CHECK(p.settled());
      BOOST_CHECK(!p.closed());
      BOOST_CHECK(success);
   }

   {
      // Callback constructor + no arguments and return values.
      bool success = false;
      Promise p(
         [&]() {
            success = true;
            return 0;
         },
         []() {
            BOOST_CHECK(false);
            return -1;
         });
      BOOST_CHECK(!p.settled());
      BOOST_CHECK(!p.closed());
      BOOST_CHECK(!success);

      p.settle(std::string("foo"));
      BOOST_CHECK(p.settled());
      BOOST_CHECK(!p.closed());
      BOOST_CHECK(success);
   }
}

BOOST_AUTO_TEST_CASE(undeliveredException) {
   bool triggered = false;
   Promise::ExceptionHandler originalHandler =
      Promise::setUndeliveredExceptionHandler(
         [&triggered](const std::exception_ptr&) {
            triggered = true;
         });

   {
      Promise p;
      BOOST_CHECK(!p.settled());
      p.settle(std::make_exception_ptr(0));
      BOOST_CHECK(p.settled());
   }
   BOOST_CHECK(triggered);
   
   Promise::setUndeliveredExceptionHandler(originalHandler);
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
      all.then([&](const std::vector<size_t>& results) {
            BOOST_CHECK_EQUAL(v.size(), results.size());
            for (size_t i = 0; i < v.size(); ++i) {
               v[i].then([=, &complete, &results](size_t value) {
                     BOOST_CHECK_EQUAL(value, results[i]);
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
      all.then([&](const std::vector<size_t>& results) {
            BOOST_CHECK_EQUAL(v.size(), results.size());
            for (size_t i = 0; i < v.size(); ++i) {
               v[i].then([=, &complete, &results](size_t value) {
                     BOOST_CHECK_EQUAL(value, results[i]);
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

      size_t complete = 0;
      Promise all = Promise::all(v.begin(), v.end());
      all.then([&](std::vector<size_t>&& results) {
            BOOST_CHECK_EQUAL(v.size(), results.size());
            for (size_t i = 0; i < v.size(); ++i) {
               v[i].then([=, &complete, &results](size_t value) {
                     BOOST_CHECK_EQUAL(value, results[i]);
                     ++complete;
                  });
            }
            return 0;
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
      for (int i = 0; i < 3; ++i)
         v.push_back(Promise());

      size_t complete = 0;
      Promise all = Promise::all(v.begin(), v.end());
      all.then([&](const std::tuple<int, float, std::string>& results) {
            v[0].then([=, &complete, &results](const int& value) {
                  BOOST_CHECK_EQUAL(value, std::get<0>(results));
                  ++complete;
               });
            v[1].then([=, &complete, &results](const float& value) {
                  BOOST_CHECK_EQUAL(value, std::get<1>(results));
                  ++complete;
               });
            v[2].then([=, &complete, &results](const std::string& value) {
                  BOOST_CHECK_EQUAL(value, std::get<2>(results));
                  ++complete;
               });
         });

      v[0].settle(42);
      v[1].settle(3.14f);
      v[2].settle(std::string("foo"));
      BOOST_CHECK_EQUAL(complete, v.size());
   }

   {
      std::vector<Promise> v;
      for (int i = 0; i < 3; ++i)
         v.push_back(Promise());

      size_t complete = 0;
      Promise all = Promise::all(v.begin(), v.end());
      all.then([&](std::tuple<int, float, std::string>&& results) {
            v[0].then([=, &complete, &results](const int& value) {
                  BOOST_CHECK_EQUAL(value, std::get<0>(results));
                  ++complete;
               });
            v[1].then([=, &complete, &results](const float& value) {
                  BOOST_CHECK_EQUAL(value, std::get<1>(results));
                  ++complete;
               });
            v[2].then([=, &complete, &results](const std::string& value) {
                  BOOST_CHECK_EQUAL(value, std::get<2>(results));
                  ++complete;
               });
            return 0;
         });

      v[0].settle(42);
      v[1].settle(3.14f);
      v[2].settle(std::string("foo"));
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

BOOST_AUTO_TEST_CASE(any) {
   {
      std::vector<Promise> v;

      size_t complete = 0;
      Promise any = Promise::any(v.begin(), v.end());
      any.except([&]() {
            for (size_t i = 0; i < v.size(); ++i) {
               v[i].except([=, &complete]() {
                     ++complete;
                  });
            }
         });

      for (size_t i = 0; i < v.size(); ++i)
         v[i].settle(std::make_exception_ptr(std::runtime_error("")));

      BOOST_CHECK_EQUAL(complete, v.size());
   }

   {
      std::vector<Promise> v;
      for (int i = 0; i < 4; ++i)
         v.push_back(Promise());

      size_t complete = 0;
      Promise any = Promise::any(v.begin(), v.end());
      any.except([&]() {
            for (size_t i = 0; i < v.size(); ++i) {
               v[i].except([=, &complete]() {
                     ++complete;
                  });
            }
         });

      for (size_t i = 0; i < v.size(); ++i) {
         v[i].settle(std::make_exception_ptr(std::runtime_error("")));
         if (i + 1 < v.size())
            BOOST_CHECK(!any.settled());
         else
            BOOST_CHECK(any.settled());
      }
      BOOST_CHECK_EQUAL(complete, v.size());
   }

   {
      std::vector<Promise> v;
      for (int i = 0; i < 4; ++i)
         v.push_back(Promise());

      bool complete = false;
      Promise any = Promise::any(v.begin(), v.end());
      any.then([&](const std::string& s) {
            BOOST_CHECK_EQUAL(s, std::string("foo"));
            complete = true;
         });

      for (size_t i = 1; i < v.size(); ++i)
         v[i].settle(std::make_exception_ptr(std::runtime_error("")));
      BOOST_CHECK(!any.settled());
      
      v[0].settle(std::string("foo"));
      BOOST_CHECK(any.settled());            
      BOOST_CHECK(complete);
   }

   {
      Promise p0, p1, p2, p3;

      int complete = 0;
      Promise any = Promise::any({p0, p1, p2, p3});
      any.except([&]() {
            p0.except([&]() {
                  ++complete;
               });
            p1.except([&]() {
                  ++complete;
               });
            p2.except([&]() {
                  ++complete;
               });
            p3.except([&]() {
                  ++complete;
               });
         });

      p0.settle(std::make_exception_ptr(std::runtime_error("")));
      BOOST_CHECK_EQUAL(complete, 0);
      p1.settle(std::make_exception_ptr(std::runtime_error("")));
      BOOST_CHECK_EQUAL(complete, 0);
      p2.settle(std::make_exception_ptr(std::runtime_error("")));
      BOOST_CHECK_EQUAL(complete, 0);
      p3.settle(std::make_exception_ptr(std::runtime_error("")));
      BOOST_CHECK_EQUAL(complete, 4);
   }
}
