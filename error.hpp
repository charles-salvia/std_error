#ifndef STDX_ERROR_HPP
#define STDX_ERROR_HPP

#include <stdexcept>
#include <system_error>
#include <memory>
#include <cassert>

#include "compiler.hpp"
#include "bit_cast.hpp"
#include "launder.hpp"
#include "string_ref.hpp"
#include "intrusive_ptr.hpp"

namespace stdx {

class error;

namespace detail {

	template <class... Args>
	struct error_ctor_args;

} // end namespace detail

} // end namespace stdx

namespace stdx_adl {

	namespace detail {

		template <class Args, class Enable = void>
		struct can_use_adl_to_call_make_error : std::false_type
		{ };

		inline void make_error() noexcept { }

		template <class... Args>
		struct can_use_adl_to_call_make_error<
			stdx::detail::error_ctor_args<Args...>,
			std::enable_if_t<
				std::is_same<
					decltype(make_error(std::declval<Args>()...)),
					stdx::error
				>::value
			>
		>
			: std::true_type
		{ };

		template <class... Args>
		constexpr auto construct_error_from_adl(Args&&... args) noexcept(
			noexcept(make_error(std::forward<Args>(args)...))
		)
		{
			return make_error(std::forward<Args>(args)...);
		}
	}

} // end namespace stdx_adl

namespace stdx { 

enum class dynamic_exception_errc
{
	runtime_error = 1,
	domain_error,
	invalid_argument,
	length_error,
	out_of_range,
	logic_error,
	range_error,
	overflow_error,
	underflow_error,
	bad_alloc,
	bad_array_new_length,
	bad_optional_access,
	bad_typeid,
	bad_any_cast,
	bad_cast,
	bad_weak_ptr,
	bad_function_call,
	bad_exception,
	bad_variant_access,
	unspecified_exception
};

const std::error_category& dynamic_exception_category() noexcept;

inline std::error_code make_error_code(dynamic_exception_errc code) noexcept
{
	return std::error_code(static_cast<int>(code), dynamic_exception_category());
}

std::error_code error_code_from_exception(
	std::exception_ptr eptr = std::current_exception(),
	std::error_code not_matched = make_error_code(dynamic_exception_errc::unspecified_exception)
) noexcept;

// -------------------- error_traits
//
template <class E>
struct error_traits
{ };

namespace detail {

	template <class E, class Enable = void>
	struct is_convertible_from_exception_using_traits : std::false_type
	{ };
		
	template <class E>
	struct is_convertible_from_exception_using_traits<
		E,
		std::enable_if_t<
			std::is_convertible<
				decltype(error_traits<E>::from_exception(std::declval<std::exception_ptr>())),
				E
			>::value
		>
	>
		: std::true_type
	{ };

	template <class E, class Enable = void>
	struct is_convertible_to_exception_using_traits : std::false_type
	{ };
		
	template <class E>
	struct is_convertible_to_exception_using_traits<
		E,
		std::enable_if_t<
			std::is_convertible<
				decltype(error_traits<E>::to_exception(std::declval<E>())),
				std::exception_ptr
			>::value
		>
	>
		: std::true_type
	{ };

	template <class E>
	E from_exception_impl(std::exception_ptr e, std::is_convertible<std::exception_ptr, E>) noexcept
	{
		return e;
	}

	template <class E>
	E from_exception_impl(
		std::exception_ptr e, 
		is_convertible_from_exception_using_traits<E>
	) noexcept
	{
		return error_traits<E>::from_exception(std::move(e));
	}

	void from_exception_impl(std::exception_ptr, sentinel_type) = delete;

	template <class E, class G>
	std::exception_ptr to_exception_impl(E&& e, std::is_convertible<G, std::exception_ptr>) noexcept
	{
		return std::forward<E>(e);
	}

	template <class E, class G>
	std::exception_ptr to_exception_impl(
		E&& e, 
		is_convertible_to_exception_using_traits<G>
	) noexcept
	{
		return error_traits<E>::to_exception(std::forward<E>(e));
	}

	template <class E>
	void to_exception_impl(const E&, sentinel_type) = delete;

} // end namespace detail

template <class E>
E from_exception(std::exception_ptr e) noexcept
{
	return detail::from_exception_impl<E>(
		std::move(e),
		disjunction<
			std::is_convertible<std::exception_ptr, E>,
			detail::is_convertible_from_exception_using_traits<E>,
			sentinel_type
		>{}
	);
}

template <class E>
std::exception_ptr to_exception(E&& e) noexcept
{
	return detail::to_exception_impl(
		std::forward<E>(e),
		disjunction<
			std::is_convertible<std::decay_t<E>, std::exception_ptr>,
			detail::is_convertible_to_exception_using_traits<std::decay_t<E>>,
			sentinel_type
		>{}
	);
}

struct error_domain_id
{
	constexpr error_domain_id(std::uint64_t l, std::uint64_t h) noexcept
		: lo(l), hi(h)
	{ }

	private:

	friend constexpr bool operator == (const error_domain_id&, const error_domain_id&) noexcept;

	std::uint64_t lo;
	std::uint64_t hi;
};

constexpr bool operator == (const error_domain_id& lhs, const error_domain_id& rhs) noexcept
{
	return (lhs.lo == rhs.lo) && (lhs.hi == rhs.hi);
}

constexpr bool operator != (const error_domain_id& lhs, const error_domain_id& rhs) noexcept
{
	return !(lhs == rhs);
}

template <class T = void>
struct error_value;

namespace detail {

	template <class T>
	struct is_error_value : std::false_type
	{ };

	template <class T>
	struct is_error_value<error_value<T>> : std::true_type
	{ };

} // end namespace detail

struct error_resource_management
{
	using copy_constructor = error_value<void>(*)(const error&);
	using move_constructor = error_value<void>(*)(error&&);
	using destructor = void(*)(error&);

	constexpr error_resource_management() noexcept
		: copy{nullptr}, move{nullptr}, destroy{nullptr}
	{ }

	constexpr error_resource_management(
		copy_constructor cctor,
		move_constructor mctor,
		destructor dtor
	) noexcept
		: copy(cctor), move(mctor), destroy(dtor)
	{ }

	copy_constructor copy;
	move_constructor move;
	destructor destroy;
};

class error_domain
{
	public:

	virtual string_ref name() const noexcept = 0;
	virtual bool equivalent(const error& lhs, const error& rhs) const noexcept = 0;
	virtual string_ref message(const error&) const noexcept = 0;

	virtual void throw_exception(const error& e) const;

	friend class error;
	friend constexpr bool operator == (const error_domain&, const error_domain&) noexcept;
	friend constexpr bool operator != (const error_domain&, const error_domain&) noexcept;

	constexpr error_domain_id id() const noexcept
	{
		return m_id;
	}

	protected:

	constexpr explicit error_domain(error_domain_id id) noexcept 
		: 
		m_id{id},
		m_resource_management{}
	{ }

	constexpr error_domain(error_domain_id id, error_resource_management erm) noexcept 
		: 
		m_id{id},
		m_resource_management{erm}
	{ }

	error_domain(const error_domain &) = default;
	error_domain(error_domain &&) = default;
	error_domain &operator = (const error_domain &) = default;
	error_domain &operator = (error_domain&&) = default;
	~error_domain() = default;

	template <
		class E = error,
		class = std::enable_if_t<
			std::is_convertible<const E&, error>::value
		>
	>
	constexpr dependent_type_t<E, error_value<>> copy(const E& e) const
	{
		return m_resource_management.copy ? m_resource_management.copy(e) : error_value<>{e.m_value};
	}

	template <
		class E = error,
		class = std::enable_if_t<
			std::is_rvalue_reference<E&&>::value
			&& std::is_convertible<E&&, error>::value
		>
	>
	constexpr dependent_type_t<remove_cvref_t<E>, error_value<>> move(E&& e) const
	{
		return m_resource_management.move ?
			m_resource_management.move(static_cast<E&&>(e)) : error_value<>{e.m_value};
	}

	void destroy(error& e) const noexcept
	{
		if (m_resource_management.destroy) m_resource_management.destroy(e);
	}

	private:

	error_domain_id m_id;
	error_resource_management m_resource_management;
};

constexpr bool operator == (const error_domain& lhs, const error_domain& rhs) noexcept
{
	return lhs.id() == rhs.id();
}

constexpr bool operator != (const error_domain& lhs, const error_domain& rhs) noexcept
{
	return lhs.id() != rhs.id();
}

namespace detail {

	template <class ErasedType, class T>
	struct error_type_is_erasable
		: 
		bool_constant<
			is_trivially_relocatable<T>::value 
			&& (sizeof(T) <= sizeof(ErasedType))
			&& (alignof(T) <= alignof(ErasedType))
		>
	{ };

	template <class T>
	using can_use_static_cast = bool_constant<
		std::is_integral<T>::value
		|| std::is_enum<T>::value
	>;

	struct erased_error
	{
		using integral_type = std::intptr_t;
		using storage_type = std::aligned_storage_t<sizeof(integral_type), alignof(integral_type)>;

		constexpr erased_error() noexcept : code{}
		{ }

		template <
			class T,
			class = std::enable_if_t<
				error_type_is_erasable<integral_type, T>::value
				&& can_use_static_cast<T>::value
			>
		>
		constexpr erased_error(T value) noexcept 
			: code{static_cast<integral_type>(value)}
		{ }

		template <
			class T,
			class = std::enable_if_t<
				error_type_is_erasable<integral_type, T>::value
				&& !can_use_static_cast<T>::value
				&& is_bit_castable<T, integral_type>::value
			>,
			class = void
		>
		constexpr erased_error(T value) noexcept 
			: code{bit_cast<integral_type>(value)}
		{ }

		template <
			class T,
			class = std::enable_if_t<
				error_type_is_erasable<integral_type, T>::value
				&& !can_use_static_cast<T>::value
				&& !is_bit_castable<T, integral_type>::value
			>,
			int = 0
		>
		erased_error(T value) noexcept(std::is_nothrow_move_constructible<T>::value)
		{
			new (&storage) T(std::move(value));
		}

		union
		{
			integral_type code;
			storage_type storage;
		};
	};

	template <
		class T,
		class = std::enable_if_t<
			error_type_is_erasable<erased_error::integral_type, T>::value
			&& can_use_static_cast<T>::value
		>
	>
	constexpr T error_cast_impl(erased_error e) noexcept
	{
		return static_cast<T>(e.code);
	}

	template <
		class T,
		class = std::enable_if_t<
			error_type_is_erasable<erased_error::integral_type, T>::value
			&& !can_use_static_cast<T>::value
			&& is_bit_castable<erased_error::integral_type, T>::value
		>,
		class = void
	>
	constexpr T error_cast_impl(erased_error e) noexcept
	{
		return bit_cast<T>(e.code);
	}

	template <
		class T,
		class = std::enable_if_t<
			error_type_is_erasable<erased_error::integral_type, T>::value
			&& !can_use_static_cast<T>::value
			&& !is_bit_castable<erased_error::integral_type, T>::value
		>
	>
	constexpr T error_cast_impl(erased_error&& e) noexcept(
		std::is_nothrow_move_constructible<T>::value
	)
	{
		return std::move(*stdx::launder(reinterpret_cast<T*>(&e.storage)));
	}

	template <
		class T,
		class = std::enable_if_t<
			error_type_is_erasable<erased_error::integral_type, T>::value
			&& !can_use_static_cast<T>::value
			&& !is_bit_castable<erased_error::integral_type, T>::value
		>
	>
	constexpr T error_cast_impl(const erased_error& e) noexcept(
		std::is_nothrow_copy_constructible<T>::value
	)
	{
		return *stdx::launder(reinterpret_cast<const T*>(&e.storage));
	}

} // end namespace detail

template <class T>
struct error_value
{
	using value_type = T;

	constexpr error_value(const T& v) noexcept(
		std::is_nothrow_copy_constructible<T>::value
	)
		: m_value(v)
	{ }

	constexpr error_value(T&& v) noexcept(
		std::is_nothrow_move_constructible<T>::value
	)
		: m_value(std::move(v))
	{ }

	constexpr const T& value() const & noexcept
	{ 
		return m_value;
	}

	STDX_LEGACY_CONSTEXPR T& value() & noexcept
	{
		return m_value;
	}

	constexpr const T&& value() const && noexcept
	{
		return static_cast<const T&&>(m_value);
	}

	STDX_LEGACY_CONSTEXPR T&& value() && noexcept
	{
		return static_cast<T&&>(m_value);
	}

	T m_value;
};

template <>
struct error_value<void>
{
	template <
		class T,
		class = std::enable_if_t<
			!std::is_same<remove_cvref_t<T>, error_value>::value
			&& !detail::is_error_value<remove_cvref_t<T>>::value
			&& std::is_constructible<detail::erased_error, T&&>::value
		>
	>
	constexpr error_value(T&& v) noexcept(
		std::is_nothrow_constructible<detail::erased_error, T&&>::value
	)
		: m_value(std::forward<T>(v))
	{ }

	template <
		class T,
		class = std::enable_if_t<
			std::is_constructible<detail::erased_error, const T&>::value
		>
	>
	constexpr error_value(const error_value<T>& v) noexcept(
		std::is_nothrow_constructible<detail::erased_error, const T&>::value
	)
		: error_value(v.value())
	{ }

	template <
		class T,
		class = std::enable_if_t<
			std::is_constructible<detail::erased_error, T&&>::value
		>
	>
	constexpr error_value(error_value<T>&& v) noexcept(
		std::is_nothrow_constructible<detail::erased_error, T&&>::value
	)
		: error_value(std::move(v.value()))
	{ }

	friend class error;

	private:

	detail::erased_error m_value;
};

class error;

namespace detail {

	struct error_copy_construct_t {};
	struct error_move_construct_t {};

	template <class... Args>
	struct error_ctor_args {};

	template <class Args, class DecayedArgs>
	struct can_construct_error_from_adl
	{ };

	template <class... Args, class... DecayedArgs>
	struct can_construct_error_from_adl<error_ctor_args<Args...>, error_ctor_args<DecayedArgs...>>
		: stdx_adl::detail::can_use_adl_to_call_make_error<error_ctor_args<Args...>>
	{ };

	template <>
	struct can_construct_error_from_adl<error_ctor_args<>, error_ctor_args<>> : std::false_type
	{ };

	template <class Error>
	struct can_construct_error_from_adl<error_ctor_args<Error>, error_ctor_args<error>>
		: std::false_type
	{ };

	template <class E, class D, class T, class ErrorDomain>
	struct can_construct_error_from_adl<
		error_ctor_args<E, D>,
		error_ctor_args<error_value<T>, ErrorDomain>
	>
		: std::false_type
	{ };

	template <class CC, class V, class ED, class ErrorDomain>
	struct can_construct_error_from_adl<
		error_ctor_args<CC, V, ED>,
		error_ctor_args<error_copy_construct_t, error_value<>, ErrorDomain>
	>
		: std::false_type
	{ };

	template <class MC, class V, class ED, class ErrorDomain>
	struct can_construct_error_from_adl<
		error_ctor_args<MC, V, ED>,
		error_ctor_args<error_move_construct_t, error_value<>, ErrorDomain>
	>
		: std::false_type
	{ };

	template <class E, class Enable = void>
	struct is_convertible_to_error_using_traits : std::false_type
	{ };
		
	template <>
	struct is_convertible_to_error_using_traits<error_ctor_args<error>>
		: std::false_type
	{ };

	template <class E>
	struct is_convertible_to_error_using_traits<
		error_ctor_args<E>,
		std::enable_if_t<
			std::is_same<
				decltype(error_traits<E>::to_error(std::declval<E>())),
				error
			>::value
		>
	>
		: std::true_type
	{ };

	template <class E, class... Args>
	constexpr auto construct_error_impl(
		is_convertible_to_error_using_traits<error_ctor_args<E>>,
		Args&&... args
	) noexcept(noexcept(error_traits<E>::to_error(std::declval<Args&&>()...)))
	{
		return error_traits<E>::to_error(std::forward<Args>(args)...);
	}

	template <class A1, class A2, class... Args>
	constexpr auto construct_error_impl(
		can_construct_error_from_adl<A1, A2>,
		Args&&... args
	) noexcept(noexcept(stdx_adl::detail::construct_error_from_adl(std::declval<Args&&>()...)))
	{
		return stdx_adl::detail::construct_error_from_adl(std::forward<Args>(args)...);
	}

	struct cannot_construct_error
	{
		static constexpr bool value = false;
		using type = void;
	};

	template <class... Args>
	void construct_error_impl(cannot_construct_error, Args&&...) = delete;

	template <class Args, class DecayedArgs>
	using construct_error_disjunction_impl_t = disjunction<
		is_convertible_to_error_using_traits<DecayedArgs>,
		can_construct_error_from_adl<Args, DecayedArgs>,
		cannot_construct_error
	>;

	template <class... Args>
	using construct_error_disjunction_t = construct_error_disjunction_impl_t<
		error_ctor_args<Args...>,
		error_ctor_args<remove_cvref_t<Args>...>
	>;

	struct error_move_access;
	struct error_ref_access;
	struct error_cref_access;

} // end namespace detail

// Generic domain for std::errc codes
//
class generic_error_domain : public error_domain
{
	public:

	constexpr generic_error_domain() noexcept
		: error_domain{{0x574ce0d940b64a2bULL, 0xa7c4438dd858c9cfULL}}
	{ }

	virtual string_ref name() const noexcept override 
	{
		return "generic domain";
	}

	virtual bool equivalent(const error& lhs, const error& rhs) const noexcept override;

	virtual string_ref message(const error&) const noexcept override;
};

STDX_LEGACY_INLINE_CONSTEXPR generic_error_domain generic_domain {};

class error
{
	using erased_type = detail::erased_error;

	constexpr error(detail::error_copy_construct_t, error_value<> v, const error_domain* d) noexcept
		: m_domain(d), m_value(v.m_value)
	{ }

	constexpr error(detail::error_move_construct_t, error_value<> v, const error_domain* d) noexcept
		: m_domain(d), m_value(std::move(v.m_value))
	{ }

	public:

	constexpr error() noexcept : m_domain(&generic_domain), m_value{}
	{ }

	constexpr error(const error& e)
		: error(detail::error_copy_construct_t{}, e.m_domain->copy(e), e.m_domain)
	{ }

	constexpr error(error&& e)
		: error(detail::error_move_construct_t{}, e.m_domain->move(std::move(e)), e.m_domain)
	{ }

	template <
		class T,
		class = std::enable_if_t<
			detail::error_type_is_erasable<erased_type, T>::value
		>
	>
	constexpr error(const error_value<T>& v, const error_domain& d) noexcept
		: m_domain(&d), m_value(v.value())
	{ }

	template <
		class T,
		class = std::enable_if_t<
			detail::error_type_is_erasable<erased_type, T>::value
		>
	>
	constexpr error(error_value<T>&& v, const error_domain& d) noexcept
		: m_domain(&d), m_value(static_cast<T&&>(v.value()))
	{ }

	constexpr error(error_value<> v, const error_domain& d) noexcept
		: m_domain(&d), m_value(v.m_value)
	{ }

	template <
		class A,
		class... Args,
		class = std::enable_if_t<
			detail::construct_error_disjunction_t<A&&, Args&&...>::value
		>
	>
	constexpr error(A&& a, Args&&... args) noexcept(
		noexcept(
			detail::construct_error_impl(
				std::declval<detail::construct_error_disjunction_t<A&&, Args&&...>>(),
				std::forward<A>(a),
				std::forward<Args>(args)...
			)
		)
	)
		: 
		error(
			detail::construct_error_impl(
				detail::construct_error_disjunction_t<A&&, Args&&...>{},
				std::forward<A>(a),
				std::forward<Args>(args)...
			)
		)
	{ }

	error& operator = (const error& e)
	{
		error_value<> v = e.domain().copy(e);
		domain().destroy(*this);
		m_domain = e.m_domain;
		m_value = v.m_value;
		return *this;
	}

	error& operator = (error&& e) noexcept
	{
		if (this != &e)
		{
			error_value<> v = e.domain().move(std::move(e));
			domain().destroy(*this);
			m_domain = e.m_domain;
			m_value = v.m_value;
		}

		return *this;
	}

	~error() noexcept
	{
		m_domain->destroy(*this);
	}

	const error_domain& domain() const noexcept
	{
		return *m_domain;
	}

	string_ref message() const noexcept
	{
		return domain().message(*this);
	}

	[[noreturn]] void throw_exception() const
	{
		domain().throw_exception(*this);
		abort();
	}

	friend class error_domain;
	friend struct detail::error_move_access;
	friend struct detail::error_ref_access;
	friend struct detail::error_cref_access;

	private:

	const error_domain* m_domain;
	erased_type m_value;
};

inline bool operator == (const error& lhs, const error& rhs) noexcept
{
	if (lhs.domain().equivalent(lhs, rhs)) return true;
	if (rhs.domain().equivalent(rhs, lhs)) return true;
	return false;
}

inline bool operator != (const error& lhs, const error& rhs) noexcept
{
	return !(lhs == rhs);
}

namespace detail {

	struct error_move_access
	{
		constexpr explicit error_move_access(error&& e) noexcept : m_value(&e.m_value)
		{ }

		STDX_LEGACY_CONSTEXPR detail::erased_error&& rvalue_ref() noexcept
		{
			return std::move(*m_value);
		}

		detail::erased_error* m_value;
	};

	struct error_ref_access
	{
		constexpr explicit error_ref_access(error& e) noexcept : m_ptr(&e.m_value)
		{ }

		detail::erased_error& ref() noexcept { return *m_ptr; }

		detail::erased_error* m_ptr;
	};

	struct error_cref_access
	{
		constexpr explicit error_cref_access(const error& e) noexcept : m_ptr(&e.m_value)
		{ }

		constexpr const detail::erased_error& ref() const noexcept { return *m_ptr; }

		const detail::erased_error* m_ptr;
	};

} // end namespace detail

template <
	class T, 
	class = void_t<
		decltype(detail::error_cast_impl<T>(std::declval<const detail::erased_error&>()))
	>
>
constexpr T error_cast(const error& e) noexcept(
	noexcept(detail::error_cast_impl<T>(std::declval<const detail::erased_error&>()))
)
{
	return detail::error_cast_impl<T>(detail::error_cref_access{e}.ref());
}

template <
	class T, 
	class = void_t<decltype(detail::error_cast_impl<T>(std::declval<detail::erased_error>()))>
>
constexpr T error_cast(error&& e) noexcept(
	noexcept(detail::error_cast_impl<T>(std::declval<detail::erased_error>()))
)
{
	return detail::error_cast_impl<T>(detail::error_move_access{std::move(e)}.rvalue_ref());
}

namespace detail {

	struct default_error_constructors
	{
		template <class T>
		static error_value<> copy_constructor(const error& e) noexcept(
			std::is_nothrow_copy_constructible<T>::value
			&& std::is_nothrow_move_constructible<T>::value
		)
		{
			T value = error_cast<T>(e);
			return error_value<>{std::move(value)};
		}

		template <class T>
		static error_value<> move_constructor(error&& e) noexcept(
			std::is_nothrow_move_constructible<T>::value
		)
		{
			return error_value<>{error_cast<T>(std::move(e))};
		}

		template <class T>
		static void destructor(error& e) noexcept
		{
			detail::erased_error& value = error_ref_access{e}.ref();
			stdx::launder(reinterpret_cast<T*>(&value.storage))->~T();
		}

		template <class T>
		constexpr static error_resource_management::copy_constructor copy() noexcept
		{
			return is_trivially_copy_constructible<T>::value ?
				nullptr : &copy_constructor<T>;
		}

		template <class T>
		constexpr static error_resource_management::move_constructor move() noexcept
		{
			return is_trivially_move_constructible<T>::value ?
				nullptr : &move_constructor<T>;
		}

		template <class T>
		constexpr static error_resource_management::destructor destroy() noexcept
		{
			return is_trivially_destructible<T>::value ?
				nullptr : &destructor<T>;
		}
	};

} // end namespace detail

template <class T>
struct default_error_resource_management_t : error_resource_management
{
	constexpr default_error_resource_management_t() noexcept
		: 
		error_resource_management{
			detail::default_error_constructors::copy<T>(),
			detail::default_error_constructors::move<T>(),
			detail::default_error_constructors::destroy<T>()
		}
	{}
};

#if defined(STDX_VARIABLE_TEMPLATES)
template <class T>
inline constexpr default_error_resource_management = default_error_resource_management_t<T>{};
#endif

template <>
struct error_traits<std::errc>
{
	static std::exception_ptr to_exception(std::errc ec) noexcept
	{
		return std::make_exception_ptr(std::make_error_code(ec));
	}

	static error to_error(std::errc ec) noexcept
	{
		return error{error_value<std::errc>{ec}, generic_domain};
	}
};

namespace detail {

	struct error_code_wrapper : enable_reference_count
	{
		explicit error_code_wrapper(std::error_code ec) noexcept : code(ec)
		{ }

		std::error_code code;
	};

} // end namespace detail

// Error domain mapping to std::error_code
//
class error_code_error_domain : public error_domain
{
	using internal_value_type = intrusive_ptr<detail::error_code_wrapper>;

	friend class error_traits<std::error_code>;

	public:

	constexpr error_code_error_domain() noexcept
		:
		error_domain{
			{0x84e99cdcecae4443ULL, 0x9050179b713fd2afULL},
			default_error_resource_management_t<internal_value_type>{}
		}
	{ }

	virtual string_ref name() const noexcept override 
	{ 
		return "std::error_code error domain";
	}

	virtual bool equivalent(const error& lhs, const error& rhs) const noexcept override;

	virtual string_ref message(const error& e) const noexcept override;

	[[noreturn]] virtual void throw_exception(const error& e) const override;
};

STDX_LEGACY_INLINE_CONSTEXPR error_code_error_domain error_code_domain {};

template <>
struct error_traits<std::error_code>
{
	static std::error_code from_exception(std::exception_ptr e) noexcept
	{
		return error_code_from_exception(std::move(e));
	}

	static std::exception_ptr to_exception(std::error_code ec) noexcept
	{
		return std::make_exception_ptr(std::system_error{ec});
	}

	static std::error_code out_of_memory_error() noexcept
	{
		return std::make_error_code(std::errc::not_enough_memory);
	}

	static error to_error(std::error_code ec) noexcept;
};

namespace detail {

	template <class Ptr, bool = (sizeof(Ptr) <= sizeof(std::intptr_t))>
	struct exception_ptr_wrapper_impl
	{
		struct control_block : enable_reference_count
		{
			constexpr explicit control_block(Ptr p) noexcept : ptr_(std::move(p))
			{ }

			Ptr ptr_;
		};

		explicit exception_ptr_wrapper_impl(Ptr p) : ptr{new control_block{std::move(p)}}
		{ }

		Ptr get() noexcept { return ptr ? ptr->ptr_ : Ptr{}; }

		intrusive_ptr<control_block> ptr;
	};

	template <class Ptr>
	struct exception_ptr_wrapper_impl<Ptr, true>
	{
		explicit exception_ptr_wrapper_impl(Ptr p) : ptr{std::move(p)}
		{ }

		Ptr get() noexcept { return ptr; }

		Ptr ptr;
	};

	using exception_ptr_wrapper = exception_ptr_wrapper_impl<std::exception_ptr>;

	static_assert(sizeof(exception_ptr_wrapper) == sizeof(std::intptr_t), "Internal library error");

} // end namespace detail

template <>
struct is_trivially_relocatable<detail::exception_ptr_wrapper> : std::true_type
{ };

// Error domain mapping to std::exception_ptr
//
class dynamic_exception_error_domain : public error_domain
{
	public:

	constexpr dynamic_exception_error_domain() noexcept
		:
		error_domain{
			{0x3c223c0aa3cf45e5ULL, 0x80dac24345cfb9fcULL},
			default_error_resource_management_t<detail::exception_ptr_wrapper>{}
		}
	{ }

	virtual string_ref name() const noexcept override 
	{ 
		return "dynamic exception domain";
	}

	virtual bool equivalent(const error& lhs, const error& rhs) const noexcept override;

	virtual string_ref message(const error&) const noexcept override;

	[[noreturn]] virtual void throw_exception(const error& e) const override
	{
		assert(e.domain() == *this);
		std::rethrow_exception(error_cast<detail::exception_ptr_wrapper>(e).get());
	}
};

STDX_LEGACY_INLINE_CONSTEXPR dynamic_exception_error_domain dynamic_exception_domain {};

// Error domain mapping to dynamic_exception_errc
//
class dynamic_exception_code_error_domain : public error_domain
{
	public:

	constexpr dynamic_exception_code_error_domain() noexcept
		: error_domain{{0xa242506c26484677ULL, 0x82365303df25e338ULL}}
	{ }

	virtual string_ref name() const noexcept override 
	{
		return "dynamic exception code domain";
	}

	virtual bool equivalent(const error& lhs, const error& rhs) const noexcept override;

	virtual string_ref message(const error&) const noexcept override;
};

STDX_LEGACY_INLINE_CONSTEXPR dynamic_exception_code_error_domain dynamic_exception_code_domain {};

inline error make_error(dynamic_exception_errc code) noexcept
{
	return error{error_value<dynamic_exception_errc>{code}, dynamic_exception_code_domain};
}

struct thrown_dynamic_exception : std::exception
{
	explicit thrown_dynamic_exception(stdx::error e) noexcept : m_error(e)
	{ }

	stdx::error error() const noexcept
	{
		return m_error;
	}

	private:

	stdx::error m_error;
};

template <>
struct error_traits<std::exception_ptr>
{
	static std::exception_ptr from_exception(std::exception_ptr e) noexcept
	{
		return e;
	}

	static std::exception_ptr to_exception(std::exception_ptr e) noexcept
	{
		return e;
	}

	static std::exception_ptr out_of_memory_error() noexcept
	{
		return std::make_exception_ptr(std::bad_alloc{});
	}

	static error to_error(std::exception_ptr e) noexcept
	{
		return error{
			error_value<detail::exception_ptr_wrapper>{detail::exception_ptr_wrapper{e}},
			dynamic_exception_domain
		};
	}
};

} // end namespace stdx

namespace std {

template<> 
struct is_error_code_enum<stdx::dynamic_exception_errc> : std::true_type 
{ };

} // end namespace std

#endif

