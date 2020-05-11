#include <memory>
#include <atomic>
#include <system_error>
#include <iostream>
#include <random>
#include <cstdint>
#include <string>

//#include "include/error.hpp"
//#include "error.cpp"
#include "all_in_one.hpp"

#ifdef NDEBUG
#error "Cannot compile test with NDEBUG"
#endif

struct counting_allocator_base
{
	static std::atomic<std::size_t> instance_count;
};

std::atomic<std::size_t> counting_allocator_base::instance_count(0);

template <class T>
struct counting_allocator : counting_allocator_base
{
	using value_type = T;

	constexpr counting_allocator() noexcept = default;

	template <class U>
	constexpr counting_allocator(const counting_allocator<U>) noexcept
	{ }

	T* allocate(std::size_t n)
	{
		counting_allocator_base::instance_count.fetch_add(n * sizeof(T), std::memory_order_release);
		return static_cast<T*>(::operator new(n * sizeof(T)));
	}

	void deallocate(T* v, std::size_t n) noexcept
	{
		counting_allocator_base::instance_count.fetch_sub(n * sizeof(T), std::memory_order_release);
		::operator delete(static_cast<void*>(v));
	}
};

template <class T>
constexpr bool operator == (const counting_allocator<T>& lhs, const counting_allocator<T>& rhs) noexcept
{
	return true;
}

template <class T>
constexpr bool operator != (const counting_allocator<T>& lhs, const counting_allocator<T>& rhs) noexcept
{
	return !(lhs == rhs);
}

namespace MyStuff {

	struct MyError { };

	stdx::error make_error(MyError e) noexcept
	{
		return std::errc::invalid_argument;
	}

} // end namespace MyStuff

struct ErrorData : stdx::enable_reference_count
{
	ErrorData() noexcept : code{}
	{ }

	ErrorData(const std::string& s, std::uint64_t c) : message(s), code(c)
	{ }

	std::string message;
	std::uint64_t code;
};

bool operator == (const ErrorData& lhs, const ErrorData& rhs) noexcept
{
	return (lhs.message == rhs.message) && (lhs.code == rhs.code);
}

bool operator != (const ErrorData& lhs, const ErrorData& rhs) noexcept
{
	return !(lhs == rhs);
}

struct random_device_initializer
{
	static std::size_t seed()
	{
		static std::random_device rd;
		return rd();
	}
};

std::string random_string(const std::size_t max_string_size = 128)
{
	constexpr char letters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

	static std::mt19937_64 rng(random_device_initializer::seed());
	
	const std::size_t str_size = std::max<std::size_t>(
		1, 
		rng() % std::max<std::size_t>(1, max_string_size)
	);

	std::string result;
	result.reserve(str_size);
	for (std::size_t i = 0; i != str_size; ++i)
		result.push_back(letters[rng() % (sizeof(letters) - 1)]);
	
	return result;
}

std::uint64_t random_integer()
{
	static std::mt19937_64 rng(random_device_initializer::seed());
	return rng();
}

stdx::intrusive_ptr<ErrorData> random_error_data()
{
	return stdx::intrusive_ptr<ErrorData>{
		new ErrorData{random_string(), random_integer()}
	};
}

static_assert(
	stdx::is_trivially_relocatable<stdx::intrusive_ptr<ErrorData>>::value,
	"FAILz"
);

struct MyErrorDomain : stdx::error_domain
{
	using value_type = stdx::intrusive_ptr<ErrorData>;
 
	constexpr MyErrorDomain() noexcept
		:
		stdx::error_domain{
			{0x4725020cb3ca41e5ULL, 0xa335dafe21e65f8cULL},
			stdx::default_error_resource_management_t<value_type>{}
		}
	{ }

	virtual stdx::string_ref name() const noexcept override 
	{
		return "MyErrorDomain";
	}

	bool equivalent(const stdx::error& lhs, const stdx::error& rhs) const noexcept override
	{
		assert(lhs.domain() == *this);
		if (lhs.domain() == rhs.domain())
		{
			return stdx::error_cast<value_type>(lhs)->code == stdx::error_cast<value_type>(rhs)->code;
		}

		return false;
	}

	stdx::string_ref message(const stdx::error& e) const noexcept override
	{
		value_type edata = stdx::error_cast<value_type>(e);
		return stdx::string_ref{edata->message.data(), edata->message.data() + edata->message.size()};
	}
};

constexpr MyErrorDomain my_error_domain {};

// custom std::error_category to test interoperation with std::error_code
//
namespace MyLib {

enum class errc
{
	ragtime_error,
	invalid_jazz,
	missing_pants
};

struct MyErrorCategory : std::error_category
{
	const char* name() const noexcept override
	{
		return "MyErrorCategory";
	}
	
	std::string message(int code) const override
	{
		switch (static_cast<errc>(code))
		{
			case errc::ragtime_error: return "Ragtime error";
			case errc::invalid_jazz: return "Invalid jazz";
			case errc::missing_pants: return "Missing pants";
			default:;
			return "Unknown error";
		}
	}
};

MyErrorCategory my_error_category_instance;

std::error_code make_error_code(errc e)
{
	return std::error_code(static_cast<int>(e), my_error_category_instance);
}

stdx::error make_error(errc e)
{
	return make_error_code(e);
}

} // end namespace MyLib

namespace std {

	template <>
	struct is_error_code_enum<MyLib::errc> : std::true_type
	{ };

} // end namespace std

void string_ref_test()
{
	{
		stdx::string_ref s{"abc"};
		assert(s == "abc");
		assert(s.size() == 3);
		assert(!s.empty());
		assert(s < "abcd");
		assert(s < "abd");
		assert(s > "abb");
		assert(s > "ab");
		assert(s < "ad");
		assert(s < "b");
		static_assert(std::is_standard_layout<stdx::string_ref>::value, "FAILZ");
		static_assert(std::is_standard_layout<stdx::shared_string_ref>::value, "FAILZ");
	}

	{
		stdx::shared_string_ref s = "xyz";
		assert(s == "xyz");
		assert(s.use_count() == 1);

		{
			stdx::shared_string_ref s2 = s;
			assert(s.use_count() == 2);
			assert(s2.use_count() == 2);
			assert(s2 == "xyz");

			stdx::shared_string_ref s3 = s;
			assert(s.use_count() == 3);
			assert(s2.use_count() == 3);
			assert(s3.use_count() == 3);
			assert(s3 == "xyz");
		}

		assert(s.use_count() == 1);
	}

	{
		stdx::shared_string_ref s = "bEEf";
		assert(s == "bEEf");
		assert(s.use_count() == 1);

		{
			stdx::shared_string_ref s1 = s;
			assert(s.use_count() == 2);
			assert(s1.use_count() == 2);
			assert(s1 == "bEEf");

			stdx::shared_string_ref s2 = s;
			assert(s.use_count() == 3);
			assert(s2.use_count() == 3);
			assert(s2 == "bEEf");

			stdx::shared_string_ref s3 = std::move(s1);
			assert(s1.use_count() == 0);
			assert(s1.empty());
			assert(s1 == stdx::string_ref{});
			assert(s1 == "");
			assert(s2.use_count() == 3);
			assert(s3.use_count() == 3);
			assert(s3 == "bEEf");
			assert(s3 == s2);
			assert(s3 == s);
			assert(s3 != s1);
			assert(!s3.empty());
			assert(s3.size() == 4);
		}

		assert(s.use_count() == 1);
	}

	{
		stdx::shared_string_ref s = "ccc";
		assert(s == "ccc");
		assert(s.use_count() == 1);

		{
			stdx::shared_string_ref s2 = s;
			assert(s.use_count() == 2);
			assert(s2.use_count() == 2);
			assert(s2 == "ccc");

			stdx::shared_string_ref s3 = "xxx";
			assert(s.use_count() == 2);
			assert(s2.use_count() == 2);
			assert(s3.use_count() == 1);
			assert(s3 == "xxx");

			s3 = s2;
			assert(s3 == s2);
			assert(s3.use_count() == 3);
			assert(s2.use_count() == 3);
			assert(s3.data() == s2.data());
			assert(s3 == "ccc");

			stdx::shared_string_ref s4 = "qqq";
			assert(s4 == "qqq");
			assert(s4.use_count() == 1);

			s4 = std::move(s3);
			assert(s4.use_count() == 3);
			assert(s4 == "ccc");
			assert(s4.data() == s2.data());
			assert(s4 == s2);
			assert(s3.empty());
			assert(s3 == stdx::string_ref{});
			assert(s4 != s3);
		}

		assert(s.use_count() == 1);
	}

	{
		stdx::shared_string_ref s{std::allocator<void>{}, "xyz"};
		assert(s == "xyz");
		assert(s.use_count() == 1);

		{
			stdx::shared_string_ref s2 = s;
			assert(s.use_count() == 2);
			assert(s2.use_count() == 2);
			assert(s2 == "xyz");

			stdx::shared_string_ref s3 = s;
			assert(s.use_count() == 3);
			assert(s2.use_count() == 3);
			assert(s3.use_count() == 3);
			assert(s3 == "xyz");
		}

		assert(s.use_count() == 1);
	}

	{
		stdx::shared_string_ref s{counting_allocator<int>{}, "DFF"};
		assert(s == "DFF");
		assert(s.use_count() == 1);

		assert(counting_allocator_base::instance_count.load() != 0);

		{
			stdx::shared_string_ref s2 = s;
			assert(s.use_count() == 2);
			assert(s2.use_count() == 2);
			assert(s2 == "DFF");
			assert(s2 == s);
			assert(*std::begin(s2) == 'D');
			assert(*std::prev(std::end(s2)) == 'F');

			stdx::shared_string_ref s3 = "ZZZ";
			assert(s.use_count() == 2);
			assert(s2.use_count() == 2);
			assert(s3.use_count() == 1);
			assert(s3 == "ZZZ");
			assert(*std::begin(s3) == 'Z');

			s3 = s2;
			assert(s3 == s2);
			assert(s3.use_count() == 3);
			assert(s2.use_count() == 3);
			assert(s3.data() == s2.data());
			assert(s3 == "DFF");

			stdx::shared_string_ref s4 = "QQQX";
			assert(s4 == "QQQX");
			assert(s4.use_count() == 1);

			s4 = std::move(s3);
			assert(s4.use_count() == 3);
			assert(s4 == "DFF");
			assert(s4.data() == s2.data());
			assert(s4 == s2);
			assert(s3.empty());
			assert(s3 == stdx::string_ref{});
			assert(s4 != s3);
		}

		assert(s.use_count() == 1);
	}

	assert(counting_allocator_base::instance_count.load() == 0);

	std::cout << "string_ref_test: PASSED!" << std::endl;
}

void error_test()
{
	static_assert(sizeof(stdx::error) == sizeof(void*) * 2, "FAILz");
	static_assert(std::is_standard_layout<stdx::error>::value, "FAILz");

	// error constructed from std::errc
	{
		stdx::error e = std::errc::bad_file_descriptor;
		assert(e.domain() == stdx::generic_domain);
		assert(e.domain().name() == "generic domain");
		assert(e == std::errc::bad_file_descriptor);
		assert(e.message() == "Bad file descriptor");

		stdx::error e2 = MyStuff::MyError{};
		assert(e.domain() == stdx::generic_domain);

		stdx::error e3 = e;
		assert(e3.domain() == stdx::generic_domain);
		assert(e3.domain().name() == "generic domain");
		assert(e3 == std::errc::bad_file_descriptor);
		assert(e3 == e);
		assert(e == e3);
		assert(e3 != e2);
		assert(e3.message() == "Bad file descriptor");
	}

	// error constructed from custom error_domain and error value type
	{
		using shared_pointer = MyErrorDomain::value_type;

		shared_pointer p = random_error_data();
		assert(p.use_count() == 1);

		{
			stdx::error e{p, my_error_domain};
			assert(p.use_count() == 2);
			assert(e.domain() == my_error_domain);
		}

		assert(p.use_count() == 1);

		{
			stdx::error e1{p, my_error_domain};
			assert(p.use_count() == 2);
			assert(e1.message() == p->message.data());

			stdx::error e2 = e1;
			assert(e2.domain() == my_error_domain);
			assert(p.use_count() == 3);
			stdx::error e3 = e2;
			assert(e3.domain() == my_error_domain);
			assert(p.use_count() == 4);
		}

		assert(p.use_count() == 1);

		{
			stdx::error e1{p, my_error_domain};
			assert(p.use_count() == 2);

			stdx::error e2 = e1;
			assert(e2.domain() == my_error_domain);
			assert(p.use_count() == 3);
			stdx::error e3 = std::move(e2);
			assert(e3.domain() == my_error_domain);
			assert(p.use_count() == 3);
			stdx::error e4 = std::move(e3);
			assert(e4.domain() == my_error_domain);
			assert(p.use_count() == 3);
			stdx::error e5 = std::move(e2);
			assert(e5.domain() == my_error_domain);
			assert(p.use_count() == 3);
		}

		assert(p.use_count() == 1);

		{
			stdx::error e1{p, my_error_domain};
			assert(p.use_count() == 2);

			stdx::error e2 = e1;
			assert(p.use_count() == 3);
			stdx::error e3 = std::move(e2);
			assert(p.use_count() == 3);
			stdx::error e4 = std::move(e3);
			assert(p.use_count() == 3);

			e2 = std::errc::invalid_argument;
			assert(e2.domain() == stdx::generic_domain);
			assert(e2 == std::errc::invalid_argument);
			assert(p.use_count() == 3);

			e4 = std::errc::invalid_argument;
			assert(e4.domain() == stdx::generic_domain);
			assert(e4 == std::errc::invalid_argument);
			assert(e4 == e2);
			assert(p.use_count() == 2);
		}

		{
			stdx::error e1{p, my_error_domain};
			assert(p.use_count() == 2);

			stdx::error e2 = e1;
			assert(p.use_count() == 3);
			stdx::error e3 = std::move(e2);
			assert(p.use_count() == 3);
			stdx::error e4 = std::move(e3);
			assert(p.use_count() == 3);

			e2 = std::errc::invalid_argument;
			assert(e2.domain() == stdx::generic_domain);
			assert(e2 == std::errc::invalid_argument);
			assert(p.use_count() == 3);

			e2 = stdx::error{p, my_error_domain};
			assert(e2.domain() == my_error_domain);
			assert(e2 != std::errc::invalid_argument);
			assert(p.use_count() == 4);

			e3 = e2;
			assert(p.use_count() == 5);
			e3 = e1;
			assert(p.use_count() == 5);
			e3 = e3;
			assert(p.use_count() == 5);
			e3 = std::move(e3);
			assert(p.use_count() == 5);

			e3 = std::move(e1);
			assert(p.use_count() == 4);
		}

		assert(p.use_count() == 1);
	}

	// error constructed from std::error_code
	{
		stdx::error e = std::make_error_code(std::errc::file_too_large);
		assert(e.domain() == stdx::generic_domain);
		assert(e == std::errc::file_too_large);
		assert(e == std::make_error_code(std::errc::file_too_large));

		e = std::errc::invalid_argument;	
		assert(e.domain() == stdx::generic_domain);
		assert(e == std::errc::invalid_argument);
		assert(e == std::make_error_code(std::errc::invalid_argument));
	}

	// error constructed from std::error_code using custom error category
	{
		stdx::error e = MyLib::make_error_code(MyLib::errc::missing_pants);
		assert(e.domain() == stdx::error_code_domain);
		assert(e == MyLib::make_error_code(MyLib::errc::missing_pants));
		assert(e == MyLib::errc::missing_pants);
		assert(e != MyLib::make_error_code(MyLib::errc::invalid_jazz));
		assert(e != std::errc::invalid_argument);
		assert(e != MyLib::errc::ragtime_error);
		assert(e.message() == "Missing pants");

		stdx::error e2 = e;
		assert(e2 == e);
		assert(e2.domain() == stdx::error_code_domain);
		assert(e2 == MyLib::make_error_code(MyLib::errc::missing_pants));
		assert(e2 == MyLib::errc::missing_pants);
		assert(e2.message() == "Missing pants");

		stdx::error e3 = std::move(e2);
		assert(e3 == e);
		assert(e3.domain() == stdx::error_code_domain);
		assert(e3 == MyLib::make_error_code(MyLib::errc::missing_pants));
		assert(e3 == MyLib::errc::missing_pants);
		assert(e3.message() == "Missing pants");

		assert(e2.domain() == stdx::error_code_domain);
		assert(e2 != e3);
		assert(e2 != MyLib::make_error_code(MyLib::errc::missing_pants));

		e2 = e3;
		assert(e2 == e);
		assert(e2.domain() == stdx::error_code_domain);
		assert(e2 == MyLib::make_error_code(MyLib::errc::missing_pants));
		assert(e2 == MyLib::errc::missing_pants);
		assert(e2.message() == "Missing pants");

		e2 = MyLib::errc::invalid_jazz;
		assert(e2.domain() == stdx::error_code_domain);
		assert(e2 == MyLib::make_error_code(MyLib::errc::invalid_jazz));
		assert(e2 == MyLib::errc::invalid_jazz);
		assert(e2.message() == "Invalid jazz");

		e2 = MyLib::errc::ragtime_error;
		assert(e2.domain() == stdx::error_code_domain);
		assert(e2 == MyLib::make_error_code(MyLib::errc::ragtime_error));
		assert(e2 == MyLib::errc::ragtime_error);
		assert(e2.message() == "Ragtime error");

		e2 = std::make_error_code(std::errc::file_too_large);
		assert(e2.domain() == stdx::generic_domain);
		assert(e2 == std::errc::file_too_large);
		assert(e2 == std::make_error_code(std::errc::file_too_large));
	}
	
	// error constructed from std::exception_ptr
	{
		std::exception_ptr eptr = std::make_exception_ptr(std::logic_error{"Invalid pants selection"});
		stdx::error e = eptr;
		assert(e.domain() == stdx::dynamic_exception_domain);
		assert(e.message() == "Invalid pants selection");

		try { e.throw_exception(); }
		catch (const std::logic_error& ex)
		{
			assert(stdx::string_ref{ex.what()} == "Invalid pants selection");
		}

		stdx::error e2 = e;
		try { e2.throw_exception(); }
		catch (const std::logic_error& ex)
		{
			assert(stdx::string_ref{ex.what()} == "Invalid pants selection");
		}

		assert(e2 == stdx::dynamic_exception_errc::logic_error);
		
		e2 = std::make_exception_ptr(std::invalid_argument{"Erroneous reticulum"});
		try { e2.throw_exception(); }
		catch (const std::invalid_argument& ex)
		{
			assert(stdx::string_ref{ex.what()} == "Erroneous reticulum");
		}

		assert(e2 == stdx::dynamic_exception_errc::invalid_argument);
		assert(e2 == std::errc::invalid_argument);
		assert(e2 != stdx::dynamic_exception_errc::domain_error);
		assert(e2 != std::errc::bad_file_descriptor);
		assert(stdx::dynamic_exception_errc::invalid_argument == e2);
		assert(std::errc::invalid_argument == e2);

		e2 = std::make_exception_ptr(
			std::system_error{std::make_error_code(std::errc::bad_file_descriptor)}
		);
		try { e2.throw_exception(); }
		catch (const std::system_error& ex)
		{
			assert(ex.code() == std::errc::bad_file_descriptor);
			assert(stdx::error{std::errc::bad_file_descriptor} == ex.code());
		}

		assert(e2 == std::errc::bad_file_descriptor);
		assert(std::errc::bad_file_descriptor == e2);
		assert(e2.message() == std::make_error_code(std::errc::bad_file_descriptor).message().c_str());
	}

	std::cout << "stdx::error test: PASSED!" << std::endl;
}

int main()
{
	string_ref_test();
	error_test();
}

