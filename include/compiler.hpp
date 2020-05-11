#ifndef STDX_COMPILER_HPP
#define STDX_COMPILER_HPP

// Check compiler macros.  Note that Clang defines __GNUC__ and other GNU macros as well,
// but GNU does not define Clang macros, so we must check for Clang first.

#if defined(__llvm__) || defined(__clang__)

	// -------- LLVM/Clang

	#define STDX_CLANG_COMPILER 1

	#if defined(__cpp_variable_templates) && (__cplusplus >= 201703L)
		#define STDX_VARIABLE_TEMPLATES 1
	#endif

#elif defined(__GNUC__) && defined(__GNUC_MINOR__) && defined(__GNUC_PATCHLEVEL__)

	// -------- GNU G++

	#include <features.h>

	#define STDX_GCC_COMPILER 1

	#if (__GNUC__ >= 5) && (__cplusplus >= 201703L)
		#define STDX_VARIABLE_TEMPLATES 1
	#endif

	#if ((__GNUC__ > 7) || ((__GNUC__ == 7) && (__GNUC_MINOR__ >= 1)))
		#define STDX_TRIVIALLY_MOVE_CONSTRUCTIBLE 1
	#endif

#endif

#if defined(STDX_GCC_COMPILER)
	#if (__GNUC__ == 7) && ((__GNUC_MINOR__ >= 1) && (__GNUC_MINOR__ <= 3))
		#define STDX_GCC7_WORKAROUND_CONSTEXPR
	#else
		#define STDX_GCC7_WORKAROUND_CONSTEXPR constexpr
	#endif
#else
	#define STDX_GCC7_WORKAROUND_CONSTEXPR constexpr
#endif

// Add a legacy constexpr macro for cases where GCC < 5 incorrectly applies the const
// qualifier to constexpr member functions or does not support relaxed constexpr functions
//
#if defined(STDX_GCC_COMPILER) && (__GNUC__ < 5)
	#define STDX_LEGACY_CONSTEXPR
#else
	#define STDX_LEGACY_CONSTEXPR constexpr
#endif

#endif // STDX_COMPILER_HPP

