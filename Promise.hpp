/*
Copyright 2015 Shoestring Research, LLC.  All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#ifndef poolqueue_Promise_hpp
#define poolqueue_Promise_hpp

#include <atomic>
#include <functional>
#include <initializer_list>
#include <memory>

#include "Promise_detail.hpp"

namespace poolqueue {

   // Promises/A+ style asynchronous operations.
   //
   // Promise is inspired by the Promises/A+ specification
   // (https://promisesaplus.com/):
   //
   //   A promise represents the eventual result of an asynchronous
   //   operation. The primary way of interacting with a promise is
   //   through its then method, which registers callbacks to receive
   //   either a promise’s eventual value or the reason why the
   //   promise cannot be fulfilled.
   //
   // A Promise instance references shared state. Copying an instance
   // provides another reference to the same state. The lifetime of
   // the state is independent of the instance; i.e. the state lives
   // as long as necessary to propagate results.
   class Promise {
   public:
      typedef detail::Any Value;
      typedef detail::bad_cast bad_cast;
         
      // Construct a non-dependent Promise.
      //
      // This creates a new instance that is not attached to any other
      // instance.
      Promise();

      // Copy constructor.
      //
      // Copying a Promise instance results in a second instance that
      // references the same state as the first, via a shared_ptr.
      Promise(const Promise&) = default;
      
      // Copy assignment.
      //
      // Copying a Promise instance results in a second instance that
      // references the same state as the first, via a shared_ptr.
      Promise& operator=(const Promise&) = default;

      // Move constructor.
      Promise(Promise&&) noexcept;

      // Move assignment.
      Promise& operator=(Promise&&) = default;

      // Construct a non-dependent Promise with callbacks.
      // @onFulfil Function/functor to be called if the Promise is fulfilled.
      //            onFulfil may take a single argument by value, const
      //            reference, or rvalue reference. In the case of an
      //            rvalue reference argument, the Promise will be closed
      //            to additional callbacks. onFulfil must return a value.
      // @onReject  Optional function/functor to be called if the Promise is
      //            rejected. onReject may take a single argument of
      //            const std::exception_ptr& and must return a value.
      //
      // This creates a new instance that is not attached to any other
      // instance.  When the instance is settled, the appropriate
      // callback argument will be called.
      template<typename Fulfil, typename Reject = detail::NullReject,
               typename = typename std::enable_if<!std::is_same<typename std::decay<Fulfil>::type, Promise>::value>::type>
      Promise(Fulfil&& onFulfil, Reject&& onReject = Reject())
         : Promise(
            !std::is_same<typename std::decay<Fulfil>::type, detail::NullFulfil>::value ?
            detail::makeCallbackWrapper(std::forward<Fulfil>(onFulfil)) :
            static_cast<detail::CallbackWrapper *>(nullptr),
            !std::is_same<typename std::decay<Reject>::type, detail::NullReject>::value ?            
            detail::makeCallbackWrapper(std::forward<Reject>(onReject)) :
            static_cast<detail::CallbackWrapper *>(nullptr)) {
         typedef typename detail::CallableTraits<Fulfil>::ArgumentType FulfilArgument;
         static_assert(!std::is_same<typename std::decay<FulfilArgument>::type, Promise>::value,
                       "onFulfil callback cannot take a Promise argument.");
         static_assert(!std::is_same<typename std::decay<FulfilArgument>::type, std::exception_ptr>::value,
                       "onFulfil callback cannot take a std::exception_ptr argument.");
         typedef typename detail::CallableTraits<Reject>::ArgumentType RejectArgument;
         static_assert(std::is_same<typename std::decay<RejectArgument>::type, std::exception_ptr>::value ||
                       std::is_same<typename std::decay<RejectArgument>::type, void>::value,
                       "onReject callback must take a void or std::exception_ptr argument.");

         constexpr bool isFulfilNull = std::is_same<typename std::decay<Fulfil>::type, detail::NullFulfil>::value;
         constexpr bool isRejectNull = std::is_same<typename std::decay<Reject>::type, detail::NullReject>::value;
         typedef typename detail::CallableTraits<Fulfil>::ResultType FulfilResult;
         typedef typename detail::CallableTraits<Reject>::ResultType RejectResult;
         static_assert(isFulfilNull || isRejectNull ||
                       std::is_same<FulfilResult, RejectResult>::value,
                       "onFulfil and onReject return types must match");
         static_assert(!std::is_same<typename std::decay<FulfilResult>::type, void>::value,
                       "onFulfil callback must return a value.");
         static_assert(!std::is_same<typename std::decay<RejectResult>::type, void>::value,
                       "onFulfil callback must return a value.");
      }

      ~Promise() noexcept;

      // Settle a non-dependent Promise with a value.
      // @value Copyable or Movable value.
      //
      // @return *this to allow return Promise().settle(value);
      template<typename T>
      const Promise& settle(T&& value) const {
         settle(Value(std::forward<T>(value)));
         return *this;
      }
      
      // Settle a non-dependent Promise with an empty value.
      //
      // @return *this to allow return Promise().settle();
      const Promise& settle() const {
         settle(Value());
         return *this;
      }
      
      // Attach fulfil/reject callbacks.
      // @onFulfil Function/functor to be called if the Promise is fulfilled.
      //            onFulfil may take a single argument by value, const
      //            reference, or rvalue reference. In the case of an
      //            rvalue reference argument, the Promise will be closed
      //            to additional callbacks. onFulfil must return a value.
      // @onReject  Optional function/functor to be called if the Promise is
      //            rejected. onReject may take a single argument of
      //            const std::exception_ptr& and must return a value. 
      //
      // This method produces a dependent Promise that receives the value
      // or error from the upstream Promise and passes it through the
      // matching callback. At most one of the callbacks will be invoked,
      // never both.
      //
      // If the executed callback returns a value that is not a
      // Promise, the Promise returned by then is fulfilled with that
      // value. If the the executed callback throws an exception, the
      // Promise returned by then() is rejected with that exception.
      //
      // If the executed callback returns a Promise q, the Promise p
      // returned by then() will receive q's value or error (which may
      // not happen immediately).
      //
      // @return Dependent Promise to receive the eventual result.
      template<typename Fulfil, typename Reject = detail::NullReject >
      Promise then(Fulfil&& onFulfil, Reject&& onReject = Reject()) const {
         typedef typename detail::CallableTraits<Fulfil>::ResultType FulfilResult;
         typedef typename detail::CallableTraits<Reject>::ResultType RejectResult;
         static_assert(!std::is_same<typename std::decay<FulfilResult>::type, void>::value,
                       "onFulfil callback must return a value.");
         static_assert(!std::is_same<typename std::decay<RejectResult>::type, void>::value,
                       "onReject callback must return a value.");
         if (closed())
            throw std::logic_error("Promise is closed");
         
         Promise next(std::forward<Fulfil>(onFulfil), std::forward<Reject>(onReject));
         attach(next);
         return next;
      }
         
      // Attach reject callback only.
      // @onReject  Function/functor to be called if the Promise is
      //            rejected. onReject may take a single argument of
      //            const std::exception_ptr& and must return a value. 
      //
      // This method is the same as then() except that it only
      // attaches a reject callback.
      //
      // @return Dependent Promise to receive the eventual result.
      template<typename Reject>
      Promise except(Reject&& onReject) const {
         typedef typename detail::CallableTraits<Reject>::ResultType RejectResult;
         static_assert(!std::is_same<typename std::decay<RejectResult>::type, void>::value,
                       "onFulfil callback must return a value.");
         return then(detail::NullFulfil(), std::forward<Reject>(onReject));
      }

      // Disallow future then/except calls.
      //
      // This method explicitly closes a Promise to disallow calling
      // then() or except(). A closed Promise may settle slightly
      // faster than an unclosed Promise.
      //
      // @return *this
      Promise& close();
      
      // Disallow future then/except calls.
      //
      // This method explicitly closes a Promise to disallow calling
      // then() or except(). A closed Promise may settle slightly
      // faster than an unclosed Promise.
      //
      // @return *this
      const Promise& close() const;
      
      // Get the settled state.
      //
      // A Promise is settled when it has been either fulfilled or
      // rejected.
      //
      // @return true if settled.
      bool settled() const;

      // Get the closed state.
      //
      // A Promise is closed when either (1) its close() method has
      // been called or (2) an onFulfil callback that takes an rvalue
      // reference argument has been attached with then().  No
      // additional callbacks can be added to a closed Promise (i.e.
      // calls to then() or except() will throw an exception).
      //
      // @return true if closed.
      bool closed() const;

      // Promise conjunction on iterator range.
      // @bgn Begin iterator.
      // @end End iterator.
      //
      // This static function returns a Promise that fulfils when
      // all of the Promises in the input range fulfil, or rejects
      // when any of the Promises in the input range reject.
      //
      // If the returned Promise fulfils, the values from the input
      // Promises can be received by an onFulfil callback as type
      // std::vector<T> (if the input Promises all return the same
      // type) or as type std::tuple<T0, T1...> (if the number of
      // input Promises is known at compile time). For other cases the
      // value can be retrieved from the individual input Promises.
      //
      // @return Dependent Promise that fulfils on all or rejects on
      //         any.
      template<typename Iterator>
      static Promise all(Iterator bgn, Iterator end) {
         Promise p;
         if (const size_t n = std::distance(bgn, end)) {
            struct Context {
               std::vector<Value> values;
               std::atomic<size_t> count;
               std::atomic<bool> rejected;

               Context(size_t n)
                  : values(n)
                  , count(n)
                  , rejected(false) {
               }
            };
            auto context = std::make_shared<Context>(n);

            size_t index = 0;
            for (auto i = bgn; i != end; ++i, ++index) {
               i->then(
                  [=](const Value& value) {
                     context->values[index] = value;
                     if (context->count.fetch_sub(1) == 1) {
                        p.settle(std::move(context->values));
                     }
                     return detail::Null();
                  },
                  [=](const std::exception_ptr& e) {
                     if (!context->rejected.exchange(true, std::memory_order_relaxed))
                        p.settle(e);
                     return detail::Null();
                  });
            }
         }
         else {
            // Range is empty so fulfil immediately.
            p.settle(std::vector<Value>());
         }
            
         return p;
      }

      // Promise conjunction on initializer list.
      // @promises Input promises.
      //
      // This static function returns a Promise that fulfils when
      // all of the Promises in the input list fulfil, or rejects
      // when any of the Promises in the input list reject.
      //
      // If the returned Promise fulfils, the values from the input
      // Promises can be received by an onFulfil callback as type
      // std::vector<T> (if the input Promises all return the same
      // type) or as type std::tuple<T0, T1...> (if the number of
      // input Promises is known at compile time). For other cases the
      // value can be retrieved from the individual input Promises.
      //
      // @return Dependent Promise that fulfils on all or rejects on
      //         any.
      static Promise all(std::initializer_list<Promise> promises) {
         return all(promises.begin(), promises.end());
      }

      // Fulfil with first Promise of iterator range to fulfil.
      // @bgn Begin iterator.
      // @end End iterator.
      //
      // This static function returns a Promise that fulfils when
      // at least one of the Promises in the input range fulfils,
      // or rejects when all of the Promises in the input range
      // reject.
      //
      // If the returned Promise rejects, the std::exception_ptr
      // argument does not contain an exception.
      //
      // @return Dependent Promise that fulfils on any or rejects
      //         on all.
      template<typename Iterator>
      static Promise any(Iterator bgn, Iterator end) {
         Promise p;
         if (const size_t n = std::distance(bgn, end)) {
            struct Context {
               std::atomic<size_t> count;
               std::atomic<bool> fulfilled;

               Context(size_t n)
                  : count(n)
                  , fulfilled(false) {
               }
            };
            auto context = std::make_shared<Context>(n);

            for (auto i = bgn; i != end; ++i) {
               i->then(
                  [=](const Value& value) {
                     if (!context->fulfilled.exchange(true, std::memory_order_relaxed))
                        p.settle(value);
                     return detail::Null();
                  },
                  [=](const std::exception_ptr&) {
                     if (context->count.fetch_sub(1, std::memory_order_relaxed) == 1)
                        p.settle(std::exception_ptr());
                     return detail::Null();
                  });
            }
         }
         else {
            // Range is empty so reject immediately.
            p.settle(std::exception_ptr());
         }
            
         return p;
      }
      
      // Fulfil with first Promise of initializer list to fulfil.
      // @promises Input promises.
      //
      // This static function returns a Promise that fulfils when
      // at least one of the Promises in the input list fulfils,
      // or rejects when all of the Promises in the input list
      // reject.
      //
      // If the returned Promise rejects, the std::exception_ptr
      // argument does not contain an exception.
      //
      // @return Dependent Promise that fulfils on any or rejects
      //         on all.
      static Promise any(std::initializer_list<Promise> promises) {
         return any(promises.begin(), promises.end());
      }
      
      // Set undelivered exception handler.
      // @handler Has signature void handler(const std::exception_ptr&).
      //
      // Callback exceptions that are never delivered to an
      // onReject callback are passed to a global handler that can
      // be set with this function. A copy of the previous handler
      // is returned. The default handler calls std::unexpected().
      //
      // Note that the handler is called from the Promise destructor
      // so the handler should not throw.
      //
      // @return Copy of the previous handler.
      typedef std::function<void(const std::exception_ptr&)> ExceptionHandler;
      static ExceptionHandler setUndeliveredExceptionHandler(const ExceptionHandler& handler);

      // Set bad cast exception handler.
      // @handler Has signature void handler(const Promise::bad_cast&).
      //
      // If the upstream value does not match the argument of an
      // onFulfil callback a Promise::bad_cast exception is
      // thrown. This mismatch between output and input is usually
      // a programming error, so the default action is to let this
      // exception propagate in the normal manner (i.e. unwind the
      // stack) instead of capturing it to pass to the next
      // Promise.
      //
      // If a different behavior is desired, the default handler
      // can be replaced. If the replacement handler does not throw
      // then the exception will be captured.
      //
      // @return Copy of the previous handler.
      typedef std::function<void(const Promise::bad_cast&)> BadCastHandler;
      static BadCastHandler setBadCastExceptionHandler(const BadCastHandler& handler);
         
      friend bool operator==(const Promise& a, const Promise& b) {
         return a.pimpl == b.pimpl;
      }

      friend bool operator<(const Promise& a, const Promise& b) {
         return a.pimpl < b.pimpl;
      }

      size_t hash() const {
         return std::hash<Pimpl *>()(pimpl.get());
      }
         
      void swap(Promise& other) {
         pimpl.swap(other.pimpl);
      }
         
   private:
      struct Pimpl;
      std::shared_ptr<Pimpl> pimpl;

      Promise(detail::CallbackWrapper *, detail::CallbackWrapper *);
      
      void settle(Value&& result) const;
      void attach(const Promise& next) const;
   };

   inline void swap(Promise& a, Promise& b) {
      a.swap(b);
   }

} // namespace poolqueue

namespace std {
   template<>
   struct hash<poolqueue::Promise> {
      size_t operator()(const poolqueue::Promise& p) const {
         return p.hash();
      }
   };
}

#endif // poolqueue_Promise_hpp
