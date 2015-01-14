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

   // This class represents the result of an asynchronous
   // operation. It is inspired by the Promises/A+ specification
   // (https://promisesaplus.com/).
   //
   // A Promise instance references shared state. Copying an
   // instance provides another reference to the same state. The
   // lifetime of the state is independent of the instance;
   // i.e. the state lives as long as necessary to propagate
   // results.
   class Promise {
   public:
      typedef detail::Any Value;
      typedef detail::bad_cast bad_cast;
         
      // Construct a Promise that can be settled by resolve() or reject().
      Promise();

      Promise(const Promise&) = default;
      Promise(Promise&&) = default;
      Promise& operator=(const Promise&) = default;
      Promise& operator=(Promise&&) = default;

      ~Promise() noexcept;
         
      // Resolve a default constructed Promise. For convenience
      // the same Promise is returned.
      template<typename T>
      const Promise& resolve(T&& value) const {
         static_assert(
            !std::is_same<T, std::exception_ptr>::value,
            "std::exception_ptr argument invalid for resolve()");
         settle(Value(std::forward<T>(value)));
         return *this;
      }
      const Promise& resolve() const {
         settle(Value());
         return *this;
      }

      // Reject a default constructed Promise. For convenience
      // the same Promise is returned.
      const Promise& reject(const std::exception_ptr& error) const {
         settle(Value(error));
         return *this;
      }

      // Attach onResolve and optional onReject callbacks, returning
      // a new Promise. When the original Promise is settled, the
      // appropriate callback will be invoked and its outcome will
      // be available via the returned Promise.
      //
      // Calling then() on a closed Promise will throw std::logic_error.
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
         
      // Attach only an onReject callback, returning a new Promise.
      template<typename Reject>
      Promise except(Reject&& onReject) const {
         typedef typename detail::CallableTraits<Reject>::ArgumentType RejectArgument;
         static_assert(std::is_same<RejectArgument, const std::exception_ptr&>::value,
                       "except callback must take a const std::exception_ptr& argument.");
         return attach(detail::makeCallbackWrapper(std::forward<Reject>(onReject)));
      }

      // Return the Promise state. A Promise is settled when it has
      // been either resolved or rejected. A Promise is closed when
      // it has an onResolve callback that takes a rvalue reference
      // argument.
      bool settled() const;
      bool closed() const;

      // Access the type of a settled Promise. An exception is thrown
      // if the Promise is not settled.
      const std::type_info& type() const;

      // Access the value of a settled Promise.
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

      // This static function returns a Promise that resolves when
      // all of the Promises in the input range resolve, or rejects
      // when any of the Promises in the input range reject.
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

      static Promise all(std::initializer_list<Promise> promises) {
         return all(promises.begin(), promises.end());
      }
         
      // Callback exceptions that are never delivered to an
      // onReject callback are passed to a global handler that can
      // be set with this function. A copy of the previous handler
      // is returned. The default handler calls std::abort().
      //
      // Note that the handler is called from the Promise destructor
      // so the handler should not throw.
      typedef std::function<void(const std::exception_ptr&)> ExceptionHandler;
      static ExceptionHandler setUndeliveredExceptionHandler(const ExceptionHandler& handler);

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
