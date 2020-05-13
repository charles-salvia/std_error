# std_error
Implementation of `std::error` as proposed by Herb Sutter in [Zero-Overhead Deterministic Exceptions](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0709r0.pdf)

Requires at least C++14.  Tested on GCC-4.9.2 to GCC 10, Clang 4 to Clang 10, and MSVC 19.14 to 19.24

All-in-one header-only link: https://github.com/charles-salvia/std_error/blob/master/all_in_one.hpp <br>
Example usage/godbolt link: https://godbolt.org/z/8o1Yg6

Full unit tests on godbolt: https://godbolt.org/z/TbuRHh

**Example**
```c++
// store an std::exception_ptr in an error
stdx::error e = std::make_exception_ptr(std::invalid_argument{"missing pants"});
assert(e == std::errc::invalid_argument);
assert(e == stdx::dynamic_exception_errc::invalid_argument);
assert(e.message() == "missing pants");
assert(e.domain() == stdx::dynamic_exception_domain);

try { e.throw_exception(); }
catch (const std::invalid_argument& a) { assert(stdx::string_ref{a.what()} == "missing pants"); };

e = std::make_exception_ptr(std::logic_error{"abc"});
assert(e == stdx::dynamic_exception_errc::logic_error);
assert(e.message() == "abc");

// store an std::errc enum
e = std::errc::bad_file_descriptor;
assert(e == std::errc::bad_file_descriptor);
assert(e.message() == std::make_error_code(std::errc::bad_file_descriptor).message().c_str());
assert(e.domain() == stdx::generic_domain);

// store an std::error_code
e = std::make_error_code(std::io_errc::stream);
assert(e == std::make_error_code(std::io_errc::stream));
assert(e.domain() == stdx::error_code_domain);

// store an std::exception_ptr, weak-equality comparison with std::errc enum
e = std::make_exception_ptr(std::system_error{std::make_error_code(std::errc::host_unreachable)});
assert(e.domain() == stdx::dynamic_exception_domain);
assert(e == std::errc::host_unreachable);
assert(e.message() == std::make_error_code(std::errc::host_unreachable).message().c_str());
```
