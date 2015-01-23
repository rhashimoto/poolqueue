#include <iostream>
#include <cassert>

#include <poolqueue/Promise.hpp>

using poolqueue::Promise;

int main() {
   // Promises can chain then() and except() methods.
   Promise p0;
   p0.then( // then_A
         [](const std::string& s) {
            std::cout << "then_A onFulfil\n";

            // Any copyable type can be returned.
            return 42;
         },
         [](const std::exception_ptr& e) {
            std::cout << "then_A onReject\n";
            
            // Both callbacks must have the same return type.
            return 13;
         }) // <- no semi-colon

      .then( // then_B
         // Argument type must match upstream fulfil type.
         [](int i) {
            assert(i == 42);
            std::cout << "then_B onFulfil\n";
            return 11.0f;
         }) // onReject can be omitted.

      .then( // then_C
         [](float f) {
            assert(f == 11.0f);
            std::cout << "then_C onFulfil\n";
            
            // Exceptions in callbacks are captured.
            throw std::runtime_error("doesn't abort");
         })
      
      .then( // then_D
         []() {
            assert(false); // upstream rejected
            std::cout << "then_D onFulfil\n";
            return std::string("bar");
         },
         [](const std::exception_ptr& e) {
            std::cout << "then_D onReject\n";
            return std::string("baz");
         })

      .then( // then_E
         [](const std::string& s) {
            // The upstream callback returned a value (i.e. did not
            // throw) so it fulfilled. It doesn't matter which callback
            // it was.
            assert(s == "baz");
            std::cout << "then_E onFulfil\n";
            return 7;
         },
         [](const std::exception_ptr& e) {
            assert(false); // upstream fulfilled
            std::cout << "then_E onReject\n";
            return 9;
         })

      .except( // except_F
         [](const std::exception_ptr& e) {
            assert(false); // upstream fulfilled
            std::cout << "except_E onReject\n";
            return 321;
         })

      .then( // then_G
         [](int i) {
            // Will get the value from then_E.
            assert(i == 7);
            std::cout << "then_G onFulfil\n";

            // void return okay.
         })

      .then( // then_H
         []() {
            std::cout << "then_H onFulfil\n";
            if (true)
               throw std::runtime_error("");
            return 111;
         })

      .then( // then_I
         []() {
            // void argument is always okay, even if upstream is non-void.
            assert(false);
            std::cout << "then_H onFulfil\n";
         })

      .then( // then_J
         []() {
            assert(false);
            std::cout << "then_I onFulfil\n";
         })

      .except(
         [](const std::exception_ptr& e) {
            // Exception finally delivered here.
            std::cout << "then_I onFulfil\n";
         });

   // A Promise can have multiple dependent Promises.
   p0.then(
      []() {
         std::cout << "additional then()\n";
      });
   p0.except(
      [](const std::exception_ptr& e) {
         std::cout << "additional except()\n";
      });

   p0.settle(std::string(""));
   assert(p0.settled());

   // If dependent Promise is attached to a settled Promise, the
   // dependent Promise settles immediately.
   p0.then(
      []() {
         std::cout << "attached to settled Promise\n";
      });
   
   return 0;
}
