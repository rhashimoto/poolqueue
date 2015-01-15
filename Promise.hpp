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
#include <vector>

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
      // instance. A default constructed Promise can be settled by
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

      ~Promise() noexcept;
         
      // Resolve a default constructed Promise with a value.
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

      // Resolve a default constructed Promise with no value.
      //
      // @return *this to allow return Promise().resolve(value);
      const Promise& resolve() const {
         settle(Value());
         return *this;
      }

      // Reject a default constructed Promise.
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
      //            to any further then() method calls. onResolve may return
      //            a value.
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
      // Calling then() on a closed Promise will throw std::logic_error.
      //
      // @return Promise to receive the result of a value/error and the
      //         matching callback.
      template<typename Resolve, typename Reject = std::function<void(const std::exception_ptr&)> >
      Promise then(Resolve&& onResolve, Reject&& onReject = Reject()) const {
         typedef typename detail::CallableTraits<Resolve>::ArgumentType ResolveArgument;
         static_assert(!std::is_same<typename std::decay<ResolveArgument>::type, std::exception_ptr>::value,
                       "onResolve callback cannot take a std::exception_ptr argument.");
         typedef typename detail::CallableTraits<Reject>::ArgumentType RejectArgument;
         static_assert(std::is_same<typename std::decay<RejectArgument>::type, std::exception_ptr>::value,
                       "onReject callback must take a std::exception_ptr argument.");

         if (detail::IsDefaultExceptionCallback<Reject>(onReject))
            return attach(detail::makeCallbackWrapper(std::forward<Resolve>(onResolve)));

         Promise p;
         Promise pResolve = attach(detail::makeCallbackWrapper(std::forward<Resolve>(onResolve)));
         pResolve.attach(detail::makeCallbackWrapper([=](Value&& v) {
                  p.resolve(std::move(v));
               }));
         Promise pReject = attach(detail::makeCallbackWrapper(std::forward<Reject>(onReject)));
         pReject.attach(detail::makeCallbackWrapper([=](const std::exception_ptr& e) {
                  p.reject(e);
               }));
               
         return p;
      }
         
      // Attach reject callback only.
      // @onReject  Function/functor to be called if the Promise is
      //            rejected. onReject may take a single argument of
      //            const std::exception_ptr& and may return a value. 
      //
      // This method is the same as then() except that it only
      // attaches a reject callback.
      template<typename Reject>
      Promise except(Reject&& onReject) const {
         typedef typename detail::CallableTraits<Reject>::ArgumentType RejectArgument;
         static_assert(std::is_same<RejectArgument, const std::exception_ptr&>::value,
                       "except callback must take a const std::exception_ptr& argument.");
         return attach(detail::makeCallbackWrapper(std::forward<Reject>(onReject)));
      }

      // Get the settled state.
      //
      // A Promise is settled when it has been either resolved or
      // rejected.
      //
      // @return true if settled.
      bool settled() const;

      // Get the closed state.
      //
      // A Promise is closed when an onResolve callback that takes
      // an rvalue reference argument has been attached with then().
      // No additional calls to then() can be made on a closed
      // Promise.
      //
      // @return true if closed.
      bool closed() const;

      // Get settled Promise type.
      //
      // This method returns the type of the value on a resolved
      // Promise, or typeid(std::exception_ptr) on a rejected
      // Promise. An exception is thrown if the Promise is not
      // settled.
      //
      // @return Type object on the Promise.
      const std::type_info& type() const;

      // Access the value of a settled Promise.
      //
      // This method provides access to the value or error on
      // a settled Promise, either by value or const reference.
      // An exception is thrown if the Promise is not settled
      // or if the cast type is incorrect.
      //
      // @return Cast value.
      template<typename T>
      T cast() const {
         constexpr bool isReference = std::is_reference<T>::value;
         constexpr bool isConst = std::is_const<typename std::remove_reference<T>::type>::value;
         static_assert(!isReference || isConst,
                       "access must be by value or const reference");
         if (!settled())
            throw std::runtime_error("Promise is not settled");
         if (closed())
            throw std::runtime_error("Promise value has been moved");
         return getValue().cast<T>();
      }

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
      // @return New Promise that resolves on all or rejects on any.
      template<typename Iterator>
      static Promise all(Iterator bgn, Iterator end) {
         Promise p;
         if (const size_t n = std::distance(bgn, end)) {
            auto count = std::make_shared<std::atomic<size_t>>(n);
            auto rejected = std::make_shared<std::atomic<bool>>(false);
            for (auto i = bgn; i != end; ++i) {
               i->then([=]() {
                     if (!--*count) {
                        p.resolve();
                     }
                  })
                  .except([=](const std::exception_ptr& e) {
                        if (!rejected->exchange(true))
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
      // @return New Promise that resolves on all or rejects on any.
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

      Promise(std::shared_ptr<Pimpl>&&);
         
      void settle(Value&& result) const;
      Promise attach(detail::CallbackWrapper *) const;
      const Value& getValue() const;

      template<typename T>
      static T cast(Value& value) {
         return value.cast<T>();
      }
      template<typename T>
      static const T& cast(const Value& value) {
         return value.cast<const T&>();
      }
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
