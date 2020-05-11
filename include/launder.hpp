#ifndef STDX_LAUNDER_HPP
#define STDX_LAUNDER_HPP

#include <new>

#include "compiler.hpp"

namespace stdx {

#if __cplusplus >= 201703L
	#if defined(__cpp_lib_launder)
		#define STDX_HAVE_NATIVE_LAUNDER 1
		using std::launder;
	#elif defined(STDX_CLANG_COMPILER)
		#if __has_builtin(__builtin_launder)
			#define STDX_HAVE_NATIVE_LAUNDER 1
			template <class T>
			constexpr T* launder(T* p) noexcept
			{
				return __builtin_launder(p);
			}
		#endif	
	#endif
#endif

#if !defined(STDX_HAVE_NATIVE_LAUNDER)
template <class T>
constexpr T* launder(T* p) noexcept
{
	return p;
}
#endif

} // end namespace stdx

#endif


