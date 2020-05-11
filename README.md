# std_error
std::error implementation (zero-overhead deterministic exceptions) P0709

This is an implementation of `std::error`, the proposed error type (p0709r4) to implement deterministic exceptions.

See: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p0709r4.pdf

Requires at least C++14.  Tested on GCC-4.9.2 to GCC 10, Clang 4 to Clang 10, and MSVC 19.14 to 19.24
