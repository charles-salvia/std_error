#ifndef STDX_TYPE_TRAITS_HPP
#define STDX_TYPE_TRAITS_HPP

#include <type_traits>
#include "compiler.hpp"

namespace stdx {

// Implementation of std::void_t for use with pre-C++17 compilers.
//
namespace detail {

	template <class... T> 
	struct void_t_impl 
	{ 
		using type = void;
	};

} // end namespace detail

template <class... T>
using void_t = typename detail::void_t_impl<T...>::type;

template <class T, class U = T>
struct dependent_type
{
	using type = U;
};

template <class T, class U = T>
using dependent_type_t = typename dependent_type<T, U>::type;

template <class... B> 
struct disjunction : std::false_type 
{ };

template <class B1>
struct disjunction<B1> : B1
{ };

template<class B1, class... Bn>
struct disjunction<B1, Bn...> 
	: 
	std::conditional_t<
		bool(B1::value), 
		B1, 
		disjunction<Bn...>
	>
{ };

struct sentinel_type
{
	static constexpr bool value = true;
	using type = void;
};

template <bool B>
struct bool_constant : std::integral_constant<bool, B>
{ };

// Implementation of std::remove_cvref for use with pre-C++20 compilers.
//
template <class T>
struct remove_cvref 
{
	using type = std::remove_cv_t<std::remove_reference_t<T>>;
};

template <class T>
using remove_cvref_t = typename remove_cvref<T>::type;

#if defined(STDX_GCC_COMPILER)
	// Support missing functionality on older compilers (<= gcc 4.7)
	//
	#if (__GNUC__ == 4) && (__GNUC_MINOR__ <= 7)
	// Map the old incorrect type-trait names to the newer correct ones
	template <class T>
	using is_trivially_copyable = std::is_trivial<T>;

	template <class T>
	using is_trivially_copy_constructible = std::has_trivial_copy_constructor<T>;

	template <class T>
	using is_trivially_destructible = std::has_trivial_destructor<T>;
	#elif (__GNUC__ < 5)
	template <class T>
	using is_trivially_copyable = std::is_trivial<T>;

	template <class T>
	using is_trivially_copy_constructible = std::has_trivial_copy_constructor<T>;

	template <class T>
	using is_trivially_destructible = std::is_trivially_destructible<T>;
	#else
	using std::is_trivially_destructible;
	using std::is_trivially_copyable;
	using std::is_trivially_copy_constructible;
	#endif
#else
using std::is_trivially_destructible;
using std::is_trivially_copyable;
using std::is_trivially_copy_constructible;
#endif

#if defined(STDX_TRIVIALLY_MOVE_CONSTRUCTIBLE)
template <class T>
using is_trivially_move_constructible = std::is_trivially_move_constructible<T>;
#else
template <class T>
struct is_trivially_move_constructible : is_trivially_copyable<T>
{ };
#endif

template <class T>
struct is_trivially_relocatable
	:
	bool_constant<
		is_trivially_move_constructible<T>::value
		&& std::is_trivially_destructible<T>::value
	>
{ };

#if defined(STDX_VARIABLE_TEMPLATES)
template <class T>
inline constexpr bool is_trivially_relocatable_v = is_trivially_relocatable<T>::value;
#endif

#if __cplusplus >= 201703L
	#define STDX_LEGACY_INLINE_CONSTEXPR inline constexpr
#else
	#define STDX_LEGACY_INLINE_CONSTEXPR constexpr
#endif

} // end namespace stdx

#endif


