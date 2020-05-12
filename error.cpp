#include "include/error.hpp"

#if __cplusplus >= 201703L
#include <any>
#include <variant>
#include <optional>
#endif

#include <functional>

namespace stdx {

namespace {

	inline const char* dynamic_exception_errc_str(unsigned ev) noexcept
	{
		constexpr const char* msg[] = 
		{
			"Success",
			"std::runtime_error",
			"std::domain_error",
			"std::invalid_argument",
			"std::length_error",
			"std::out_of_range",
			"std::logic_error",
			"std::range_error",
			"std::overflow_error",
			"std::underflow_error",
			"std::bad_alloc",
			"std::bad_array_new_length",
			"std::bad_optional_access",
			"std::bad_typeid",
			"std::bad_any_cast",
			"std::bad_cast",
			"std::bad_weak_ptr",
			"std::bad_function_call",
			"std::bad_exception",
			"std::bad_variant_access",
			"unspecified dynamic exception"
		};

		assert(ev < (sizeof(msg) / sizeof(const char*)));
		return msg[ev];
	}

	class dynamic_exception_error_category : public std::error_category
	{
		public:

		const char* name() const noexcept override
		{ 
			return "dynamic_exception"; 
		}
		
		std::string message(int code) const override
		{
			return dynamic_exception_errc_str(code);
		}

		bool equivalent(int code, const std::error_condition& cond) const noexcept override
		{
			switch (static_cast<dynamic_exception_errc>(code))
			{
				case dynamic_exception_errc::domain_error: 
					return (cond == std::errc::argument_out_of_domain);
				case dynamic_exception_errc::invalid_argument:
					return (cond == std::errc::invalid_argument);
				case dynamic_exception_errc::length_error:
					return (cond == std::errc::value_too_large);
				case dynamic_exception_errc::out_of_range:
				case dynamic_exception_errc::range_error:
				case dynamic_exception_errc::underflow_error:
					return (cond == std::errc::result_out_of_range);
				case dynamic_exception_errc::overflow_error:
					return (cond == std::errc::value_too_large);
				case dynamic_exception_errc::bad_alloc:
				case dynamic_exception_errc::bad_array_new_length:
					return (cond == std::errc::not_enough_memory);
				default:;
			}
			return false;
		}
	};

	const dynamic_exception_error_category dynamic_exception_error_category_instance;

} // end anonymous namespace

const std::error_category& dynamic_exception_category() noexcept
{
	return dynamic_exception_error_category_instance;
}

std::error_code error_code_from_exception(
	std::exception_ptr eptr, 
	std::error_code not_matched
) noexcept
{
	if (!eptr) return make_error_code(dynamic_exception_errc::bad_exception);

	try { std::rethrow_exception(eptr); }
	catch (const std::domain_error&) 
	{
		return make_error_code(dynamic_exception_errc::domain_error);
	}
	catch (const std::invalid_argument&) 
	{
		return make_error_code(dynamic_exception_errc::invalid_argument);
	}
	catch (const std::length_error&)
	{
		return make_error_code(dynamic_exception_errc::length_error);
	}
	catch (const std::out_of_range&) 
	{
		return make_error_code(dynamic_exception_errc::out_of_range);
	}
	catch (const std::logic_error&)
	{
		return make_error_code(dynamic_exception_errc::logic_error);
	}
	catch (const std::range_error&)
	{
		return make_error_code(dynamic_exception_errc::range_error);
	}
	catch (const std::overflow_error&) {
		return make_error_code(dynamic_exception_errc::overflow_error);
	}
	catch (const std::underflow_error&) {
		return make_error_code(dynamic_exception_errc::underflow_error);
	}
	catch (const std::system_error& e)
	{
		return e.code();
	}
	catch (const std::runtime_error&)
	{
		return make_error_code(dynamic_exception_errc::runtime_error);
	}
	catch (const std::bad_array_new_length&)
	{
		return make_error_code(dynamic_exception_errc::bad_array_new_length);
	}
	catch (const std::bad_alloc&)
	{
		return make_error_code(dynamic_exception_errc::bad_alloc);
	}
	catch (const std::bad_typeid&)
	{
		return make_error_code(dynamic_exception_errc::bad_typeid);
	}
	#if __cplusplus >= 201703L
	catch (const std::bad_optional_access&)
	{
		return make_error_code(dynamic_exception_errc::bad_optional_access);
	}
	catch (const std::bad_any_cast&)
	{
		return make_error_code(dynamic_exception_errc::bad_any_cast);
	}
	catch (const std::bad_variant_access&)
	{
		return make_error_code(dynamic_exception_errc::bad_variant_access);
	}
	#endif
	catch (const std::bad_cast&)
	{
		return make_error_code(dynamic_exception_errc::bad_cast);
	}
	catch (const std::bad_weak_ptr&)
	{
		return make_error_code(dynamic_exception_errc::bad_weak_ptr);
	}
	catch (const std::bad_function_call&)
	{
		return make_error_code(dynamic_exception_errc::bad_function_call);
	}
	catch (const std::bad_exception&)
	{
		return make_error_code(dynamic_exception_errc::bad_exception);
	}
	catch (...)
	{ }

	return not_matched;	
}

// ---------- ErrorDomain (abstract base class)
//
void error_domain::throw_exception(const error& e) const
{
	throw thrown_dynamic_exception{e};	
}

// ---------- GenericErrorDomain
//
bool generic_error_domain::equivalent(const error& lhs, const error& rhs) const noexcept
{
	assert(lhs.domain() == *this);
	if (lhs.domain() == rhs.domain())
	{
		return error_cast<std::errc>(lhs) == error_cast<std::errc>(rhs);
	}

	return false;
}

namespace {

	string_ref generic_error_code_message(std::errc code) noexcept
	{
		switch (code)
		{
			case std::errc::address_family_not_supported:
				return "Address family not supported by protocol";
			case std::errc::address_in_use:
				return "Address already in use";
			case std::errc::address_not_available:
				return "Cannot assign requested address";
			case std::errc::already_connected:
				return "Transport endpoint is already connected";
			case std::errc::argument_list_too_long:
				return "Argument list too long";
			case std::errc::argument_out_of_domain:
				return "Numerical argument out of domain";
			case std::errc::bad_address:
				return "Bad address";
			case std::errc::bad_file_descriptor:
				return "Bad file descriptor";
			case std::errc::bad_message:
				return "Bad message";
			case std::errc::broken_pipe:
				return "Broken pipe";
			case std::errc::connection_aborted:
				return "Software caused connection abort";
			case std::errc::connection_already_in_progress:
				return "Operation already in progress";
			case std::errc::connection_refused:
				return "Connection refused";
			case std::errc::connection_reset:
				return "Connection reset by peer";
			case std::errc::cross_device_link:
				return "Invalid cross-device link";
			case std::errc::destination_address_required:
				return "Destination address required";
			case std::errc::device_or_resource_busy:
				return "Device or resource busy";
			case std::errc::directory_not_empty:
				return "Directory not empty";
			case std::errc::executable_format_error:
				return "Exec format error";
			case std::errc::file_exists:
				return "File exists";
			case std::errc::file_too_large:
				return "File too large";
			case std::errc::filename_too_long:
				return "File name too long";
			case std::errc::function_not_supported:
				return "Function not implemented";
			case std::errc::host_unreachable:
				return "No route to host";
			case std::errc::identifier_removed:
				return "Identifier removed";
			case std::errc::illegal_byte_sequence:
				return "Invalid or incomplete multibyte or wide character";
			case std::errc::inappropriate_io_control_operation:
				return "Inappropriate ioctl for device";
			case std::errc::interrupted:
				return "Interrupted system call";
			case std::errc::invalid_argument:
				return "Invalid argument";
			case std::errc::invalid_seek:
				return "Illegal seek";
			case std::errc::io_error:
				return "Input/output error";
			case std::errc::is_a_directory:
				return "Is a directory";
			case std::errc::message_size:
				return "Message too long";
			case std::errc::network_down:
				return "Network is down";
			case std::errc::network_reset:
				return "Network dropped connection on reset";
			case std::errc::network_unreachable:
				return "Network is unreachable";
			case std::errc::no_buffer_space:
				return "No buffer space available";
			case std::errc::no_child_process:
				return "No child processes";
			case std::errc::no_link:
				return "Link has been severed";
			case std::errc::no_lock_available:
				return "No locks available";
			case std::errc::no_message:
				return "No message of desired type";
			case std::errc::no_protocol_option:
				return "Protocol not available";
			case std::errc::no_space_on_device:
				return "No space left on device";
			case std::errc::no_stream_resources:
				return "Out of streams resources";
			case std::errc::no_such_device_or_address:
				return "No such device or address";
			case std::errc::no_such_device:
				return "No such device";
			case std::errc::no_such_file_or_directory:
				return "No such file or directory";
			case std::errc::no_such_process:
				return "No such process";
			case std::errc::not_a_directory:
				return "Not a directory";
			case std::errc::not_a_socket:
				return "Socket operation on non-socket";
			case std::errc::not_a_stream:
				return "Device not a stream";
			case std::errc::not_connected:
				return "Transport endpoint is not connected";
			case std::errc::not_enough_memory:
				return "Cannot allocate memory";
			#if ENOTSUP != EOPNOTSUPP
			case std::errc::not_supported:
				return "Operation not supported";
			#endif
			case std::errc::operation_canceled:
				return "Operation canceled";
			case std::errc::operation_in_progress:
				return "Operation now in progress";
			case std::errc::operation_not_permitted:
				return "Operation not permitted";
			case std::errc::operation_not_supported:
				return "Operation not supported";
			#if EAGAIN != EWOULDBLOCK
			case std::errc::operation_would_block:
				return "Resource temporarily unavailable";
			#endif
			case std::errc::owner_dead:
				return "Owner died";
			case std::errc::permission_denied:
				return "Permission denied";
			case std::errc::protocol_error:
				return "Protocol error";
			case std::errc::protocol_not_supported:
				return "Protocol not supported";
			case std::errc::read_only_file_system:
				return "Read-only file system";
			case std::errc::resource_deadlock_would_occur:
				return "Resource deadlock avoided";
			case std::errc::resource_unavailable_try_again:
				return "Resource temporarily unavailable";
			case std::errc::result_out_of_range:
				return "Numerical result out of range";
			case std::errc::state_not_recoverable:
				return "State not recoverable";
			case std::errc::stream_timeout:
				return "Timer expired";
			case std::errc::text_file_busy:
				return "Text file busy";
			case std::errc::timed_out:
				return "Connection timed out";
			case std::errc::too_many_files_open_in_system:
				return "Too many open files in system";
			case std::errc::too_many_files_open:
				return "Too many open files";
			case std::errc::too_many_links:
				return "Too many links";
			case std::errc::too_many_symbolic_link_levels:
				return "Too many levels of symbolic links";
			case std::errc::value_too_large:
				return "Value too large for defined data type";
			case std::errc::wrong_protocol_type:
				return "Protocol wrong type for socket";
			default:
				return "Unspecified error";
		}
	}

} // end anonymous namespace

string_ref generic_error_domain::message(const error& e) const noexcept
{
	assert(e.domain() == *this);
	return generic_error_code_message(error_cast<std::errc>(e));
}

// ---------- ErrorCodeErrorDomain
//
string_ref error_code_error_domain::message(const error& e) const noexcept
{
	assert(e.domain() == *this);

	auto ptr = error_cast<internal_value_type>(e);
	if (ptr)
	{
		std::string msg = ptr->code.message();
		return shared_string_ref{msg.c_str(), msg.c_str() + msg.size()};
	}

	return string_ref{"Bad error code"};
}

void error_code_error_domain::throw_exception(const error& e) const
{
	assert(e.domain() == *this);

	std::error_code code;
	auto ptr = error_cast<internal_value_type>(e);
	if (ptr) code = ptr->code;
	throw std::system_error{code};
}

bool error_code_error_domain::equivalent(const error& lhs, const error& rhs) const noexcept
{
	assert(lhs.domain() == *this);

	if (lhs.domain() == rhs.domain())
	{
		auto ptr1 = error_cast<internal_value_type>(lhs);
		auto ptr2 = error_cast<internal_value_type>(rhs);
		if (ptr1 && ptr2) return ptr1->code == ptr2->code.default_error_condition();
		return false;
	}

	if (rhs.domain() == generic_domain)
	{
		auto ptr1 = error_cast<internal_value_type>(lhs);
		if (ptr1) return ptr1->code == error_cast<std::errc>(rhs);
	}

	return false;
}

stdx::error error_traits<std::error_code>::to_error(std::error_code ec) noexcept
{
	using internal_value_type = error_code_error_domain::internal_value_type;

	if (ec.category() == std::generic_category())
	{
		return error{
			error_value<std::errc>{static_cast<std::errc>(ec.default_error_condition().value())},
			generic_domain
		};
	}

	return error{
		error_value<internal_value_type>{internal_value_type{new detail::error_code_wrapper{ec}}},
		error_code_domain
	};
}

// ---------- DynamicExceptionErrorDomain
//
string_ref dynamic_exception_error_domain::message(const error& e) const noexcept
{
	assert(e.domain() == *this);

	std::exception_ptr eptr = error_cast<detail::exception_ptr_wrapper>(e).get();
	
	try
	{ 
		std::rethrow_exception(eptr);
	}
	catch (const std::exception& ex)
	{
		return shared_string_ref{ex.what()};
	}
	catch (...) {}

	return string_ref{"Unknown dynamic exception"};
}

namespace {

std::errc dynamic_exception_code_to_generic_code(dynamic_exception_errc code) noexcept
{
	switch (code)
	{
		case dynamic_exception_errc::domain_error: 
			return std::errc::argument_out_of_domain;
		case dynamic_exception_errc::invalid_argument:
			return std::errc::invalid_argument;
		case dynamic_exception_errc::length_error:
			return std::errc::value_too_large;
		case dynamic_exception_errc::out_of_range:
		case dynamic_exception_errc::range_error:
		case dynamic_exception_errc::underflow_error:
			return std::errc::result_out_of_range;
		case dynamic_exception_errc::overflow_error:
			return std::errc::value_too_large;
		case dynamic_exception_errc::bad_alloc:
		case dynamic_exception_errc::bad_array_new_length:
			return std::errc::not_enough_memory;
		default:;
	}
	return std::errc{};
}

} // end anonymous namespace

bool dynamic_exception_error_domain::equivalent(const error& lhs, const error& rhs) const noexcept
{
	assert(lhs.domain() == *this);

	std::exception_ptr eptr = error_cast<detail::exception_ptr_wrapper>(lhs).get();
	std::error_code ec = error_code_from_exception(eptr);
	if (ec == dynamic_exception_errc::unspecified_exception) return false;

	if (rhs.domain() == *this)
	{
		std::exception_ptr eptr2 = error_cast<detail::exception_ptr_wrapper>(rhs).get();
		if (eptr == eptr2) return true;
		std::error_code ec2 = error_code_from_exception(eptr2);
		return (ec == ec2.default_error_condition());
	}
	else if (rhs.domain() == dynamic_exception_code_domain)
	{
		if (ec.category() == dynamic_exception_category())
		{
			return dynamic_exception_code_domain.equivalent(
				rhs, 
				static_cast<dynamic_exception_errc>(ec.value())
			);
		}
		else if (ec.category() == std::generic_category())
		{
			return dynamic_exception_code_domain.equivalent(
				rhs,
				static_cast<std::errc>(ec.value())
			);
		}
	}
	else if (rhs.domain() == error_code_domain)
	{
		return error_code_domain.equivalent(rhs, error{ec});
	}
	else if (rhs.domain() == generic_domain)
	{
		if (ec.category() == std::generic_category())
		{
			return generic_domain.equivalent(rhs, error{static_cast<std::errc>(ec.value())});
		}
		else if (ec.category() == dynamic_exception_category())
		{
			std::errc generic_code = dynamic_exception_code_to_generic_code(
				static_cast<dynamic_exception_errc>(ec.value())
			);
			return generic_domain.equivalent(rhs, generic_code);
		}
	}

	return false;
}

// ---------- DynamicExceptionCodeErrorDomain
//
bool dynamic_exception_code_error_domain::equivalent(
	const error& lhs, 
	const error& rhs
) const noexcept
{
	assert(lhs.domain() == *this);

	const dynamic_exception_errc code = error_cast<dynamic_exception_errc>(lhs);
	
	if (rhs.domain() == *this)
	{
		return code == error_cast<dynamic_exception_errc>(rhs);
	}
	else if (rhs.domain() == error_code_domain)
	{
		return error_code_domain.equivalent(rhs, make_error_code(code));
	}
	else if (rhs.domain() == generic_domain)
	{
		std::errc generic_code = dynamic_exception_code_to_generic_code(code);
		return generic_domain.equivalent(rhs, generic_code);
	}

	return false;
}

string_ref dynamic_exception_code_error_domain::message(const error& e) const noexcept
{
	assert(e.domain() == *this);
	return string_ref{
		dynamic_exception_errc_str(static_cast<unsigned>(error_cast<dynamic_exception_errc>(e)))
	};
}

} // end namespace stdx


