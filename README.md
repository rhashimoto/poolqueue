PoolQueue
=========

PoolQueue is a C++ library for asynchronous operations. Its primary
feature is asynchronous promises, inspired by the [Javascript
Promises/A+ specification](https://promisesaplus.com/). The library
also contains a thread pool and a timer, both built using asynchronous
promises. The library implementation makes use of C++11 features but
has no external dependencies (the test suite and some examples require
Boost).
