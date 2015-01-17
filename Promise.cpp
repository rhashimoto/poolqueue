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
#include <cstdlib>
#include <iostream>
#include <mutex>

#include "Promise.hpp"

using namespace poolqueue;

namespace {
   // Private type to signify an unset Promise value. This allows void
   // to be a valid value.
   struct Unset {
   };

   // Private type to signify a moved Promise value.
   struct Moved {
   };
   
   // This is the handler called when a Promise is destroyed and it
   // contains an undelivered exception.
   Promise::ExceptionHandler undeliveredExceptionHandler = [](const std::exception_ptr& e) {
      try {
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
      std::abort();
   };

   // This is the handler called on a bad callback argument cast.
   Promise::BadCastHandler badCastHandler = [](const Promise::bad_cast& e) {
      throw e;
   };
}

struct poolqueue::Promise::Pimpl : std::enable_shared_from_this<Pimpl> {
   std::mutex mutex_;
   std::weak_ptr<Pimpl> upstream_;
   typedef std::vector<std::shared_ptr<Pimpl> > Targets;
   Targets downstream_;

   Value value_;
   std::atomic<bool> propagated_;
   std::atomic<bool> closed_;
   
   std::unique_ptr<detail::CallbackWrapper> onResolve_;
   std::unique_ptr<detail::CallbackWrapper> onReject_;
   
   Pimpl()
      : value_(Unset())
      , propagated_(false)
      , closed_(false) {
   }

   Pimpl(const Pimpl& other) = delete;

   ~Pimpl() {
      // Handle undelivered exceptions.
      if (!propagated_ && value_.type() == typeid(std::exception_ptr) && undeliveredExceptionHandler) {
         // Protect the exception handler from concurrent execution.
         static std::mutex m;
         std::lock_guard<std::mutex> lock(m);
         undeliveredExceptionHandler(cast<const std::exception_ptr&>(value_));
      }
   }

   bool settled() {
      std::lock_guard<std::mutex> lock(mutex_);
      if (upstream_.lock()) {
         // Still waiting on upstream, not settled.
         return false;
      }
      return value_.type() != typeid(Unset);
   }

   bool closed() {
      return closed_;
   }

   const std::type_info& type() {
      std::lock_guard<std::mutex> lock(mutex_);
      return value_.type();
   }
   
   // Attach a downstream promise.
   void link(const std::shared_ptr<Pimpl>& next) {
      // A Promise is closed once an onResolve callback with an rvalue
      // reference argument has been added because that callback can
      // steal (i.e. move) the value.
      if (next->onResolve_ && next->onResolve_->hasRvalueArgument())
         closed_ = true;
      
      next->upstream_ = shared_from_this();

      Targets targets;
      {
         std::lock_guard<std::mutex> lock(mutex_);
         downstream_.push_back(next);
         if (value_.type() != typeid(Unset))
            targets.swap(downstream_);
      }

      if (!targets.empty())
         propagate(targets);
   }

   // Push value downstream.
   void propagate(const Targets& targets) {
      assert(value_.type() != typeid(Unset));
      for (const auto& target : targets)
         target->settle(std::move(value_));

      // The propagated_ flag is set if the value is sent to any
      // downstream target. Once set, it should never be unset.
      assert(!(propagated_ && targets.empty()));
      propagated_ = !targets.empty();;

      // Mark the value as invalid if it has been allowed to be
      // moved (it might not have actually been moved but we can't
      // tell).
      if (closed_)
         value_ = Moved();
   }

   // Set promise value.
   void settle(Value&& value) {
      // Pass value through appropriate callback if present.
      Value cbValue{Unset()};
      if (onResolve_ && value.type() != typeid(std::exception_ptr)) {
         try {
            cbValue = (*onResolve_)(std::move(value));
         }
         catch (const bad_cast& e) {
            // The type contained in the Value does not match the
            // type the onResolve callback accepts, so this is a
            // user code error.
            if (badCastHandler)
               badCastHandler(e);
               
            cbValue = std::current_exception();
         }
         catch (...) {
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

      // Discard callbacks so they cannot be used again.
      onResolve_.reset();
      onReject_.reset();
      
      if (cbValue.type() != typeid(Promise)) {
         Targets targets;
         {
            std::lock_guard<std::mutex> lock(mutex_);
            if (value_.type() != typeid(Unset))
               throw std::logic_error("Promise already settled");

            // If a callback transformed the value, move it.
            // If the value came from upstream, copy it.
            // If the value came from the user, move it.
            if (cbValue.type() != typeid(Unset))
               value_.swap(cbValue);
            else if (upstream_.lock())
               value_ = value;
            else
               value_.swap(value);

            // Ensure that subsequent calls to settled() return true.
            upstream_.reset();
            
            targets.swap(downstream_);
         }

         propagate(targets);
      }
      else {
         // Make a returned Promise the new upstream.
         auto& p = cbValue.cast<Promise&>();
         p.pimpl->link(shared_from_this());
      }
   }

   std::shared_ptr<Pimpl> attach(detail::CallbackWrapper *onResolve, detail::CallbackWrapper *onReject) {
      auto result = std::make_shared<Pimpl>();
      result->onResolve_.reset(onResolve);
      result->onReject_.reset(onReject);
      
      link(result);
      return result;;
   }
};

poolqueue::Promise::Promise()
   : pimpl(std::make_shared<Pimpl>()) {
   static_assert(std::is_nothrow_move_constructible<Promise>::value, "noexcept move");
   static_assert(std::is_nothrow_move_assignable<Promise>::value, "noexcept assign");
}

poolqueue::Promise::Promise(std::shared_ptr<Pimpl>&& other)
   : pimpl(std::move(other)) {
}

poolqueue::Promise::~Promise() noexcept {
}

bool
poolqueue::Promise::settled() const {
   return pimpl->settled();
}

bool
poolqueue::Promise::closed() const {
   return pimpl->closed();
}

const std::type_info&
poolqueue::Promise::type() const {
   if (!settled())
      throw std::runtime_error("Promise is not settled");
   if (closed())
      throw std::runtime_error("value has been moved");
   return pimpl->type();
}

Promise::ExceptionHandler
poolqueue::Promise::setUndeliveredExceptionHandler(const ExceptionHandler& handler) {
   ExceptionHandler previous = std::move(undeliveredExceptionHandler);
   undeliveredExceptionHandler = handler;
   return previous;
}

Promise::BadCastHandler
poolqueue::Promise::setBadCastExceptionHandler(const BadCastHandler& handler) {
   BadCastHandler previous = std::move(badCastHandler);
   badCastHandler = handler;
   return previous;
}

void
poolqueue::Promise::settle(Value&& value) const {
   if (pimpl->upstream_.lock())
      throw std::logic_error("invalid operation on dependent Promise");
   pimpl->settle(std::move(value));
}

Promise
poolqueue::Promise::attach(detail::CallbackWrapper *onResolve,
                           detail::CallbackWrapper *onReject) const {
   return Promise(pimpl->attach(onResolve, onReject));
}

const Promise::Value&
poolqueue::Promise::getValue() const {
   return pimpl->value_;
}
