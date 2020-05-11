#ifndef STDX_LAUNDER_HPP
#define STDX_LAUNDER_HPP

#include <memory>

namespace stdx {

#if __cplusplus < 201703L

template <class T>
constexpr T* launder(T* p) noexcept
{
	return p;
}

#else

using std::launder;

#endif

} // end namespace stdx

#endif


