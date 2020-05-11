#ifndef STDX_BIT_CAST_HPP
#define STDX_BIT_CAST_HPP

#include <cstdint>
#include "compiler.hpp"
#include "type_traits.hpp"

namespace stdx {

	namespace detail {

		template <class To, class From> 
		using use_static_cast = bool_constant<
			((std::is_integral<To>::value || std::is_enum<To>::value)
			&& (std::is_integral<From>::value || std::is_enum<From>::value))
			|| (std::is_same<To, From>::value && std::is_copy_constructible<To>::value)
		>;

		template <class T>
		using is_integral_ptr_t = bool_constant<
			std::is_same<T, std::intptr_t>::value
			|| std::is_same<T, std::uintptr_t>::value
		>;

		template <class To, class From>
		using use_reinterpret_cast = bool_constant<
			!std::is_same<To, From>::value
			&& ((
					std::is_pointer<To>::value 
					&& std::is_pointer<From>::value 
					&& std::is_convertible<From, To>::value
				)
				|| (std::is_pointer<To>::value && is_integral_ptr_t<From>::value)
				|| (std::is_pointer<From>::value && is_integral_ptr_t<To>::value)
			)
		>;

		#if defined(STDX_GCC_COMPILER)
		template <class To, class From> 
		using use_union_type_punning = bool_constant<
			!use_static_cast<To, From>::value
			&& !use_reinterpret_cast<To, From>::value
			&& !std::is_array<To>::value
			&& !std::is_array<From>::value
		>;

		template <class To, class From> 
		union bit_cast_union
		{
			From from;
			To to;
		};
		#else
		template <class To, class From> 
		using use_union_type_punning = std::false_type;
		#endif

		template <class To, class From>
		using can_bit_cast = bool_constant<
			(sizeof(To) == sizeof(From))
			&& is_trivially_copyable<From>::value
			&& is_trivially_copyable<To>::value
		>;

	} // end namespace detail

	template <
		class To,
		class From,
		class = std::enable_if_t<
			detail::can_bit_cast<To, From>::value
			&& detail::use_static_cast<To, From>::value
		>
	>
	constexpr To bit_cast(const From& from) noexcept
	{
		return static_cast<To>(from);
	}

	template <
		class To,
		class From,
		class = std::enable_if_t<
			detail::can_bit_cast<To, From>::value
			&& detail::use_reinterpret_cast<To, From>::value
		>,
		int = 0
	>
	constexpr To bit_cast(const From& from) noexcept
	{
		return reinterpret_cast<To>(from);
	}

	#if defined(STDX_GCC_COMPILER) // GCC allows union type punning
	template <
		class To,
		class From,
		class = std::enable_if_t<
			detail::can_bit_cast<To, From>::value
			&& detail::use_union_type_punning<To, From>::value
		>,
		class = void
	>
	constexpr To bit_cast(const From& from) noexcept
	{
		return detail::bit_cast_union<To, From>{from}.to;
	}
	#elif defined(STDX_CLANG_COMPILER)
		#if __has_builtin(__builtin_bit_cast)
		template <
			class To,
			class From,
			class = std::enable_if_t<
				detail::can_bit_cast<To, From>::value
				&& !detail::use_static_cast<To, From>::value
				&& !detail::use_reinterpret_cast<To, From>::value
			>,
			class = void
		>
		constexpr To bit_cast(const From& from) noexcept
		{
			return __builtin_bit_cast(To, from);
		}
		#else
		template <
			class To,
			class From,
			class = std::enable_if_t<
				detail::can_bit_cast<To, From>::value
				&& !detail::use_static_cast<To, From>::value
				&& !detail::use_reinterpret_cast<To, From>::value
			>,
			class = void
		>
		To bit_cast(const From& from) noexcept
		{
			To to;
			std::memcpy(&to, &from, sizeof(To));
			return to;
		}
		#endif
	#endif // STDX_CLANG_COMPILER

	template <class To, class From>
	struct is_bit_castable
		:
		bool_constant<
			(sizeof(To) == sizeof(From))
			&& is_trivially_copyable<From>::value
			&& is_trivially_copyable<To>::value
		>
	{ };

} // end namespace stdx

#endif


