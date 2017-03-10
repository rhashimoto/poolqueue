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
#include <cassert>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include "Promise.hpp"

using namespace poolqueue;

namespace {
   // Private type to signify an unset Promise value. This allows void
   // to be a valid value.
   struct Unset {
   };

   std::mutex gHandlerMutex;
   
   // This is the handler called when a Promise is destroyed and it
   // contains an undelivered exception. There is nothing technically
   // wrong with discarding an undelivered exception but it is much
   // harder to figure out certain bugs. The default handler thus
   // aborts.
   Promise::ExceptionHandler undeliveredExceptionHandler = [](const std::exception_ptr& e) {
      try {
         if (e)
            std::rethrow_exception(e);
      }
      catch (const std::exception& e) {
         std::cerr << e.what() << '\n';
      }
      catch (...) {
         std::cerr << "unknown exception\n";
      }

      // This function is called from a destructor so just abort
      // instead of throwing.
      std::unexpected();
   };

   // This is the handler called on a bad callback argument cast,
   // i.e. when the settled value on a Promise is incompatible with
   // the onFulfil argument. If the handler returns normally, the
   // exception will be propagated along the Promise chain like any
   // other exception. The default handler throws because this is
   // typically a programming error.
   Promise::BadCastHandler badCastHandler = [](const Promise::bad_cast& e) {
      throw e;
   };
}

struct poolqueue::Promise::Pimpl : std::enable_shared_from_this<Pimpl> {
   std::mutex mutex_;
   Pimpl *upstream_;
   std::vector<std::shared_ptr<Pimpl> > downstream_;

   Value value_;
   std::atomic<bool> closed_;
   std::atomic<std::thread::id> settled_;
   std::atomic<bool> undeliveredException_;
   
   std::unique_ptr<detail::CallbackWrapper> onFulfil_;
   std::unique_ptr<detail::CallbackWrapper> onReject_;
   
   Pimpl()
      : upstream_(nullptr)
      , value_(Unset()) {
      closed_.store(false, std::memory_order_relaxed);
      settled_.store(std::thread::id(), std::memory_order_relaxed);
      undeliveredException_.store(false, std::memory_order_relaxed);
   }

   Pimpl(const Pimpl& other) = delete;

   ~Pimpl() {
      // Pass undelivered exceptions to the handler.
      if (undeliveredException_.load(std::memory_order_relaxed)) {
         std::lock_guard<decltype(mutex_)> lock(gHandlerMutex);
         if (undeliveredExceptionHandler)
            undeliveredExceptionHandler(value_.cast<const std::exception_ptr&>());
      }
   }

   void close() {
      closed_ = true;
   }
   
   bool settled() const {
      return settled_.load(std::memory_order_relaxed) != std::thread::id();
   }

   bool closed() const {
      return closed_.load(std::memory_order_relaxed);
   }

   // Attach a downstream promise.
   void link(const std::shared_ptr<Pimpl>& next) {
      next->upstream_ = this;

      bool closed = false;
      std::thread::id settled = settled_;
      if (settled == std::thread::id()) {
         // The Promise is probably not settled (it could have settled
         // immediately after the check). We need the lock to protect
         // against concurrent callers to this function and settle().
         std::lock_guard<decltype(mutex_)> lock(mutex_);

         // Check type match between upstream callback result and
         // downstream callback argument. This check is inconclusive
         // if:
         //
         // - Callbacks are not present.
         // - Upstream result type is Any or Promise.
         // - Downstream argument type is void.
         //
         // A mismatch would eventually be found in propagation but
         // it is much easier to debug when found during attachment.
         const std::type_info& oType =
            onFulfil_ ? onFulfil_->resultType() :
            (onReject_ ? onReject_->resultType() : typeid(detail::Any));
         const std::type_info& iType =
            next->onFulfil_ ? next->onFulfil_->argumentType() : typeid(void);
         if (oType != iType &&
             oType != typeid(detail::Any) && oType != typeid(Promise) &&
             iType != typeid(detail::Any) && iType != typeid(void)) {
            throw std::logic_error(std::string("type mismatch: ") + oType.name() + " -> "  + iType.name());
         }
         
         downstream_.push_back(next);
         if (value_.type() != typeid(Unset)) {
            // We can get here if the Promise is settled in between
            // testing settled_ and obtaining the lock. If it did
            // happen, settle() would have released the lock so
            // we can access value_.
            settled = settled_.load(std::memory_order_relaxed);
            if (value_.type() == typeid(std::exception_ptr))
               undeliveredException_.store(false, std::memory_order_relaxed);
         }

         // A Promise is closed once an onFulfil callback with an rvalue
         // reference argument has been added because that callback can
         // steal (i.e. move) the value.
         //
         // Storing to the atomic closed_ has release semantics so
         // settle() can access downstream_ without taking a lock as
         // long as it reads closed_ with acquire semantics.
         if (next->onFulfil_ && next->onFulfil_->hasRvalueArgument()) {
            closed_.store(true, std::memory_order_release);
            closed = true;
         }
      }
      else {
         // This Promise already has a value so next can immediately
         // be settled with it. A lock is not neccessary as downstream_
         // is not used and the previous access of settled_ acquires
         // value_.
         if (value_.type() == typeid(std::exception_ptr))
            undeliveredException_.store(false, std::memory_order_relaxed);

         // A Promise is closed once an onFulfil callback with an rvalue
         // reference argument has been added because that callback can
         // steal (i.e. move) the value.
         //
         // Release semantics for closed_ are unnecessary because
         // settle() has already been called and downstream_ is not
         // modified.
         if (next->onFulfil_ && next->onFulfil_->hasRvalueArgument()) {
            closed_.store(true, std::memory_order_relaxed);
            closed = true;
         }
      }

      if (settled != std::thread::id()) {
         std::unique_lock<decltype(mutex_)> lock(mutex_, std::defer_lock);
         if (closed && settled != std::this_thread::get_id()) {
            // This is the problem case where this call has added an
            // onResolve with an rvalue reference argument to a
            // settled Promise. We have to ensure that we wait until
            // any in-progress settlement completes, or else the value
            // could be stolen from it.
            lock.lock();
         }

         // It is possible for two threads to execute this statement
         // concurrently, which can cause a problem if (1) one of the
         // calls closed the Promise, and (2) the same call steals the
         // value before the other uses it. This would be a race bug
         // in user code even with synchronization, however, as it
         // would be arbitrary whether the closing call came first
         // (making the other call invalid), or second (making the
         // other call valid).
         next->settle(std::move(value_), false);
      }
   }

   // Set promise value.
   void settle(Value&& value, bool direct) {
      if (direct && value_.type() != typeid(Unset))
         throw std::logic_error("Promise already settled");

      // Pass value through appropriate callback if present.
      Value cbValue{Unset()};
      if (onFulfil_ && value.type() != typeid(std::exception_ptr)) {
         try {
            cbValue = (*onFulfil_)(std::move(value));
         }
         catch (const bad_cast& e) {
            // The type contained in the Value does not match the
            // type the onFulfil callback accepts, so this is a
            // user code error.
            if (badCastHandler)
               badCastHandler(e);
               
            cbValue = std::current_exception();
         }
         catch (...) {
            // All other exceptions are propagated downstream.
            cbValue = std::current_exception();
         }
      }
      else if (onReject_ && value.type() == typeid(std::exception_ptr)) {
         try {
            cbValue = (*onReject_)(std::move(value));
         }
         catch (...) {
            cbValue = std::current_exception();
         }
      }

      if (cbValue.type() != typeid(Promise)) {
         // Access to downstream_ is exclusive when closed so the
         // lock can be avoided in that state (testing closed_ has
         // acquire semantics so downstream_ is valid). When not
         // closed there is a possible race with link() so the lock
         // is required.
         const bool closed = closed_;
         std::unique_lock<decltype(mutex_)> lock(mutex_, std::defer_lock);
         if (!closed)
            lock.lock();

         if (direct && upstream_)
            throw std::logic_error("invalid operation on dependent Promise");
         upstream_ = nullptr;

         // If a callback transformed the value, move it.
         // If the value came from the user, move it.
         // If the value came from upstream, copy it.
         if (cbValue.type() != typeid(Unset))
            value_.swap(cbValue);
         else if (direct)
            value_.swap(value);
         else
            value_ = value;

         // Local update is complete. This store has release semantics
         // so threads that acquire settled_ can access updates to value_.
         settled_ = std::this_thread::get_id();

         if (!downstream_.empty()) {
            // Propagate settlement to dependent Promises.
            for (const auto& child : downstream_)
               child->settle(std::move(value_), false);
         }
         else if (value_.type() == typeid(std::exception_ptr)) {
            // The value contains an undelivered exception. If it
            // remains undelivered at destruction then the handler
            // will be called, potentially in a different thread. We
            // ensure that the value will be valid there by specifying
            // release semantics if the lock was not used.
            undeliveredException_.store(
               true,
               closed ? std::memory_order_release : std::memory_order_relaxed);
         }
      }
      else {
         // Discard callbacks so they cannot be used again.
         onFulfil_.reset();
         onReject_.reset();
      
         // Make a returned Promise the new upstream.
         auto& p = cbValue.cast<Promise&>();
         p.pimpl->link(shared_from_this());
      }
   }
};

poolqueue::Promise::Promise()
   : pimpl(std::make_shared<Pimpl>()) {
   // STL containers will copy instead of move if they can't guarantee
   // strong exception safety. These checks are sufficient for that.
   static_assert(std::is_nothrow_move_constructible<Promise>::value, "noexcept move");
   static_assert(std::is_nothrow_move_assignable<Promise>::value, "noexcept assign");
}

poolqueue::Promise::Promise(Promise&& other) noexcept
   : pimpl(std::make_shared<Pimpl>()) {
   pimpl.swap(other.pimpl);
}

poolqueue::Promise::Promise(detail::CallbackWrapper *onFulfil, detail::CallbackWrapper *onReject)
   : pimpl(std::make_shared<Pimpl>()) {
   pimpl->onFulfil_.reset(onFulfil);
   pimpl->onReject_.reset(onReject);
}

poolqueue::Promise::~Promise() noexcept {
}

Promise&
poolqueue::Promise::close() {
   pimpl->close();
   return *this;
}

const Promise&
poolqueue::Promise::close() const {
   pimpl->close();
   return *this;
}

bool
poolqueue::Promise::settled() const {
   return pimpl->settled();
}

bool
poolqueue::Promise::closed() const {
   return pimpl->closed();
}

Promise::ExceptionHandler
poolqueue::Promise::setUndeliveredExceptionHandler(const ExceptionHandler& handler) {
   std::lock_guard<std::mutex> lock(gHandlerMutex);
   ExceptionHandler previous = std::move(undeliveredExceptionHandler);
   undeliveredExceptionHandler = handler;
   return previous;
}

Promise::BadCastHandler
poolqueue::Promise::setBadCastExceptionHandler(const BadCastHandler& handler) {
   std::lock_guard<std::mutex> lock(gHandlerMutex);
   BadCastHandler previous = std::move(badCastHandler);
   badCastHandler = handler;
   return previous;
}

void
poolqueue::Promise::settle(Value&& value) const {
   pimpl->settle(std::move(value), true);
}

void
poolqueue::Promise::attach(const Promise& next) const {
   pimpl->link(next.pimpl);
}
