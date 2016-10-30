# PoolQueue
PoolQueue is a C++ library for asynchronous operations. Its primary
feature is asynchronous promises, inspired by the [Javascript
Promises/A+ specification](https://promisesaplus.com/). The library
also contains a thread pool and a timer, both built using asynchronous
promises. The library implementation makes use of C++11 features but
has no external dependencies (the test suite and some examples require
Boost).

The key distinction between Promises/A+ and `std::promise` in C++11 is
that Promises/A+ provides *non-blocking* synchronization (via chaining
function objects) and `std::promise` provides *blocking*
synchronization (or polling). Both have their uses and one is not a
direct replacement for the other.

`boost::future` provides a [`then()` method](http://www.boost.org/doc/libs/1_58_0/doc/html/thread/synchronization.html#thread.synchronization.futures.then)
which is non-blocking and is closer in spirit to PoolQueue, but there
are a number of key differences. The `boost::future` implementation of
`then()` spawns a thread to block on the future, while PoolQueue
simply chains a callback. Returning a Promise from a callback is an
important Promises/A+ feature, but is not allowed with `boost::future`.
The `then()` method can be invoked at most once on a `boost::future`
instance, while Promises/A+ does not have this restriction.

PoolQueue is developed by Shoestring Research, LLC and is available
under the [Apache License Version
2.0](http://www.apache.org/licenses/LICENSE-2.0).

## Promise overview
The [Javascript
Promises/A+ specification](https://promisesaplus.com/) describes a promise:

> A promise represents the eventual result of an asynchronous
> operation. The primary way of interacting with a promise is through
> its then method, which registers callbacks to receive either a
> promise’s eventual value or the reason why the promise cannot be
> fulfilled.

A PoolQueue `Promise` can be in one of three states: *pending* (aka
not settled), *fulfilled*, or *rejected*. Pending means that the
`Promise` has not yet been fulfilled or rejected. Fulfilled means that
the `Promise` has a value. Rejected means that the `Promise` has an
exception.

A `Promise` can have function callbacks to invoke when it is settled
(either fulfilled or rejected). Whichever callback is invoked, the
`Promise` will be fulfilled if the callback returns or rejected if the
callback throws an exception. The exception to this is if the callback
returns a `Promise`. In this case the original `Promise` will settle
with the result of the returned `Promise` (callbacks will not be
invoked again).

A `Promise` can have dependent `Promise`s created and attached using
the `then()` or `except()` methods. When a `Promise` is settled, it
invokes the appropriate callback if present, and then recursively
settles dependent `Promise`s with the result (value or exception). A
dependent `Promise` newly attached to an already settled `Promise`
will be settled immediately.

Example:

    #include <poolqueue/Promise.hpp>
    ...
    poolqueue::Promise p;
    p.then(
      [](const std::string& s) {
        std::cout << "fulfilled with " << s << '\n';
        return nullptr;
      },
      [](const std::exception_ptr& e) {
        try {
          if (e)
            std::rethrow_exception(e);
        }
        catch (const std::exception& e) {
          std::cout << "rejected with " << e.what() << '\n';
        }
        return nullptr;
      });
    ...
    // possibly in another thread
    p.settle(std::string("how now brown cow");

The lambdas attached with `then()` are deferred until the `Promise` is
settled.

Additional example code is under examples/:

* [Basic `Promise` usage](https://github.com/rhashimoto/poolqueue/blob/master/examples/Promise_basics.cpp)
* [`Promise::all`](https://github.com/rhashimoto/poolqueue/blob/master/examples/Promise_all.cpp)
* [`Promise::any`](https://github.com/rhashimoto/poolqueue/blob/master/examples/Promise_any.cpp)
* [Closed `Promise`](https://github.com/rhashimoto/poolqueue/blob/master/examples/Promise_close.cpp)

## Promise details
A PoolQueue `Promise` holds a shared pointer to its state. Copying a
`Promise` produces another reference to the same state, not a brand
new `Promise`. This allows lambdas to capture `Promise`s by value.

Note that unlike Javascript, a PoolQueue `Promise` callback must
return a value. This requirement helps to avoid a common programming
mistake that is hard to debug. If a callback does not compute a
meaningful result then a dummy value, e.g. `nullptr`, can be returned.

Another difference from Javascript is that callback functions attached
to a `Promise` (with `then()` or `except()`) are called synchronously
when the `Promise` settles, instead of being queued for later.

A `Promise` can be closed to `then()` and `except()` methods, either
explicitly using `close()` or implicitly by passing an `onFulfil`
callback to `then()` that takes an rvalue reference argument. Closed
`Promises`s may settle slightly faster (due to an internal
optimization), plus moving a value from an rvalue reference avoids a
copy.

The static method `Promise::all()` can be used to create a new
`Promise` dependent on an input set of `Promise`s. The new `Promise`
fulfils (with an empty value) when all the input `Promise`s fulfil, or
rejects when any of the input `Promise`s reject (with the exception
from the first to reject).

The static method `Promise::any()` also creates a new `Promise`
dependent on an input set of `Promise`s. The new `Promise` fulfils
when any of the input `Promise`s fulfil (with the value from the
first to fulfil), or rejects when all of the input `Promise`s reject.

A rejected Promise that never delivers its exception to an `onReject`
callback will invoke an undelivered exception handler in its
destructor. The default handler calls `std::unexpected()`, which helps
to catch errors that are mistakenly being ignored. The default handler
can be replaced with `Promise::setUndeliveredExceptionHandler()`.

The library attempts to report type mismatches within calls to
`then()` or `except()` by throwing `std::logic_error` but this is not
always possible (e.g. when a callback returns a `Promise`). When a
type mismatch occurs when trying invoke a callback, a bad cast handler
is invoked. The default handler throws `Promise::bad_cast`. The
default handler can be replaced with
`Promise::setBadCastExceptionHandler()`. If the replacement handler
does not throw then the exception will be captured just like any other
callback exception.

## Delay
The benefits of `Promise`s don't become apparent until you have
asynchronous services that return them. `Delay` is a simple but
useful service to create a `Promise` that is fulfilled when a timer
expires or rejected when it is cancelled:

    #include <poolqueue/Delay.hpp>
    ...
    poolqueue::Delay::after(std::chrono::seconds(5))
      .then(
        []() {
          std::cout << "I waited.\n";
          return nullptr;
        },
        [](const std::exception_ptr& e) {
          std::cout << "Couldn't wait.\n";
          return nullptr;
        });

Additional example code is under examples/:

* [Delay basics](https://github.com/rhashimoto/poolqueue/blob/master/examples/Delay_basics.cpp)

## ThreadPool
PoolQueue also contains a thread pool built on `Promise`s.  Passing a
callable object to `ThreadPool::post()` adds it to a queue and returns
a `Promise` that is settled with the result of the callable object
when it is executed on one of the threads in the pool:

    #include <poolqueue/ThreadPool.hpp>
    ...
    poolqueue::ThreadPool tp;
    poolqueue::Promise p = tp.post(
      []() {
        std::cout << "I'm running in the pool.\n";
        return std::string("my data");
      });
      
    p.then(
      [](std::string& s) {
        std::cout << "Worker result is " << s << '\n';
        return nullptr;
      },
      [](const std::exception& e) {
        std::cout << "Worker threw an exception\n";
        return nullptr;
      });

The default number of pool threads is the detected hardware
concurrency support.

Additional example code is under examples/:

* [ThreadPool basics](https://github.com/rhashimoto/poolqueue/blob/master/examples/ThreadPool_basics.cpp)
* [ThreadPool strand](https://github.com/rhashimoto/poolqueue/blob/master/examples/ThreadPool_strand.cpp)

## MPI
If both MPI and Boost libraries are available then PoolQueue also
offers remote function and procedure object calls across process
and/or machine boundaries. Calls on function objects return a
Promise that settles when the remote call completes; calls on
procedure objects are fire-and-forget.
