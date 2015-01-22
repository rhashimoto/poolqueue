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
      // instance. A non-dependent Promise should be settled by
      // calling <resolve> or <reject>.
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
      Promise(Promise&&) = default;

      // Move assignment.
      Promise& operator=(Promise&&) = default;

      // Construct a non-dependent Promise with callbacks.
      // @onResolve Function/functor to be called if the Promise is resolved.
      //            onResolve may take a single argument by value, const
      //            reference, or rvalue reference. In the case of an
      //            rvalue reference argument, the Promise will be closed
      //            to additional callbacks. onResolve may return a value.
      // @onReject  Optional function/functor to be called if the Promise is
      //            rejected. onReject may take a single argument of
      //            const std::exception_ptr& and may return a value.
      //
      // This creates a new instance that is not attached to any other
      // instance. A default constructed Promise should be settled by
      // calling <resolve> or <reject>.
      //
      // When the instance is settled, the appropriate callback argument
      // will be called.
      template<typename Resolve, typename Reject = detail::NullReject,
               typename = typename std::enable_if<!std::is_same<typename std::decay<Resolve>::type, Promise>::value>::type>
      Promise(Resolve&& onResolve, Reject&& onReject = Reject())
         : Promise(
            !std::is_same<typename std::decay<Resolve>::type, detail::NullResolve>::value ?
            detail::makeCallbackWrapper(std::forward<Resolve>(onResolve)) :
            static_cast<detail::CallbackWrapper *>(nullptr),
            !std::is_same<typename std::decay<Reject>::type, detail::NullReject>::value ?            
            detail::makeCallbackWrapper(std::forward<Reject>(onReject)) :
            static_cast<detail::CallbackWrapper *>(nullptr)) {
         typedef typename detail::CallableTraits<Resolve>::ArgumentType ResolveArgument;
         static_assert(!std::is_same<typename std::decay<ResolveArgument>::type, std::exception_ptr>::value,
                       "onResolve callback cannot take a std::exception_ptr argument.");
         typedef typename detail::CallableTraits<Reject>::ArgumentType RejectArgument;
         static_assert(std::is_same<typename std::decay<RejectArgument>::type, std::exception_ptr>::value,
                       "onReject callback must take a std::exception_ptr argument.");

         constexpr bool isResolveNull = std::is_same<typename std::decay<Resolve>::type, detail::NullResolve>::value;
         constexpr bool isRejectNull = std::is_same<typename std::decay<Reject>::type, detail::NullReject>::value;
         typedef typename detail::CallableTraits<Resolve>::ResultType ResolveResult;
         typedef typename detail::CallableTraits<Reject>::ResultType RejectResult;
         static_assert(isResolveNull || isRejectNull ||
                       std::is_same<ResolveResult, RejectResult>::value,
                       "onResolve and onReject return types must match");
      }

      ~Promise() noexcept;
         
      // Resolve a non-dependent Promise with a value.
      // @value Copyable or Movable value.
      //
      // @return *this to allow return Promise().resolve(value);
      template<typename T>
      const Promise& resolve(T&& value) const {
         static_assert(
            !std::is_same<T, std::exception_ptr>::value,
            "std::exception_ptr argument invalid for resolve()");
         settle(Value(std::forward<T>(value)));
         return *this;
      }

      // Resolve a non-dependent Promise with no value.
      //
      // @return *this to allow return Promise().resolve(value);
      const Promise& resolve() const {
         settle(Value());
         return *this;
      }

      // Reject a non-dependent Promise.
      // @error Exception pointer, e.g. from std::make_exception_ptr().
      //
      // @return *this to allow return Promise().reject(error);
      const Promise& reject(const std::exception_ptr& error) const {
         settle(Value(error));
         return *this;
      }

      // Attach resolve/reject callbacks.
      // @onResolve Function/functor to be called if the Promise is resolved.
      //            onResolve may take a single argument by value, const
      //            reference, or rvalue reference. In the case of an
      //            rvalue reference argument, the Promise will be closed
      //            to additional callbacks. onResolve may return a value.
      // @onReject  Optional function/functor to be called if the Promise is
      //            rejected. onReject may take a single argument of
      //            const std::exception_ptr& and may return a value. 
      //
      // This method produces a dependent Promise that receives the value
      // or error from the upstream Promise and passes it through the
      // matching callback. At most one of the callbacks will be invoked,
      // never both.
      //
      // If the executed callback returns a value that is not a Promise,
      // the Promise returned by then is resolved with that value. If the
      // the executed callback throws an exception, the Promise returned
      // by then() is rejected with that exception.
      //
      // If the executed callback returns a Promise q, the Promise p
      // returned by then() will receive q's value or error (which may
      // not happen immediately).
      //
      // @return Dependent Promise to receive the eventual result.
      template<typename Resolve, typename Reject = detail::NullReject >
      Promise then(Resolve&& onResolve, Reject&& onReject = Reject()) const {
         if (closed())
            throw std::logic_error("Promise is closed");
         
         Promise next(std::forward<Resolve>(onResolve), std::forward<Reject>(onReject));
         attach(next);
         return next;
      }
         
      // Attach reject callback only.
      // @onReject  Function/functor to be called if the Promise is
      //            rejected. onReject may take a single argument of
      //            const std::exception_ptr& and may return a value. 
      //
      // This method is the same as then() except that it only
      // attaches a reject callback.
      //
      // @return Dependent Promise to receive the eventual result.
      template<typename Reject>
      Promise except(Reject&& onReject) const {
         return then(detail::NullResolve(), std::forward<Reject>(onReject));
      }

      // Disallow future then/except calls.
      //
      // This method explicitly closes a Promise to disallow calling
      // then() or except(). A closed Promise may settle slightly
      // faster than an unclosed Promise.
      //
      // @return *this
      Promise& close();
      
      // Get the settled state.
      //
      // A Promise is settled when it has been either resolved or
      // rejected.
      //
      // @return true if settled.
      bool settled() const;

      // Get the closed state.
      //
      // A Promise is closed when either (1) its close() method has
      // been called or (2) an onResolve callback that takes an rvalue
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
      // This static function returns a Promise that resolves when
      // all of the Promises in the input range resolve, or rejects
      // when any of the Promises in the input range reject.
      //
      // If the returned Promise resolves, no value is passed. Values
      // can be collected from the input Promises.
      //
      // @return Dependent Promise that resolves on all or rejects on
      //         any.
      template<typename Iterator>
      static Promise all(Iterator bgn, Iterator end) {
         Promise p;
         if (const size_t n = std::distance(bgn, end)) {
            auto count = std::make_shared<std::atomic<size_t>>(n);
            auto rejected = std::make_shared<std::atomic<bool>>(false);
            for (auto i = bgn; i != end; ++i) {
               i->then(
                  [=]() {
                     if (count->fetch_sub(1, std::memory_order_relaxed) == 1) {
                        p.resolve();
                     }
                  },
                  [=](const std::exception_ptr& e) {
                     if (!rejected->exchange(true, std::memory_order_relaxed))
                        p.reject(e);
                  });
            }
         }
         else {
            // Range is empty so resolve immediately.
            p.resolve();
         }
            
         return p;
      }

      // Promise conjunction on initializer list.
      // @promises Input promises.
      //
      // This static function returns a Promise that resolves when
      // all of the Promises in the input list resolve, or rejects
      // when any of the Promises in the input list reject.
      //
      // If the returned Promise resolves, no value is passed. Values
      // can be collected from the input Promises.
      //
      // @return Dependent Promise that resolves on all or rejects on
      //         any.
      static Promise all(std::initializer_list<Promise> promises) {
         return all(promises.begin(), promises.end());
      }

      // Set undelivered exception handler.
      // @handler Has signature void handler(const std::exception_ptr&).
      //
      // Callback exceptions that are never delivered to an
      // onReject callback are passed to a global handler that can
      // be set with this function. A copy of the previous handler
      // is returned. The default handler calls std::abort().
      //
      // Note that the handler is called from the Promise destructor
      // so the handler should not throw.
      typedef std::function<void(const std::exception_ptr&)> ExceptionHandler;
      static ExceptionHandler setUndeliveredExceptionHandler(const ExceptionHandler& handler);

      // Set bad cast exception handler.
      // @handler Has signature void handler(const Promise::bad_cast&).
      //
      // If the upstream value does not match the argument of an
      // onResolve callback a Promise::bad_cast exception is
      // thrown. This mismatch between output and input is usually
      // a programming error, so the default action is to let this
      // exception propagate in the normal manner (i.e. unwind the
      // stack) instead of capturing it to pass to the next
      // Promise.
      //
      // If a different behavior is desired, the default handler
      // can be replaced. If the replacement handler does not throw
      // then the exception will be captured.
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
