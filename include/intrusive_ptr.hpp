#ifndef STDX_INTRUSIVE_POINTER_HPP
#define STDX_INTRUSIVE_POINTER_HPP

#include "compiler.hpp"
#include "type_traits.hpp"

#include <cstdint>
#include <atomic>
#include <memory>

namespace stdx {

class default_intrusive_reference_count;
class default_intrusive_reference_control;

template <
	class T, 
	class RefCountAccessor = default_intrusive_reference_count,
	class Deleter = std::default_delete<T>,
	class Pointer = T*
>
class intrusive_ptr;

using ref_count_t = std::size_t;

struct enable_reference_count
{
	protected:

	constexpr enable_reference_count() noexcept : m_reference_count(1)
	{ }
	
	public:

	std::atomic<ref_count_t>& shared_reference_count() noexcept
	{
		return m_reference_count;
	}

	private:

	std::atomic<ref_count_t> m_reference_count;
};

struct default_intrusive_reference_count
{
	template <class Pointer>
	std::atomic<ref_count_t>& operator()(Pointer p) const noexcept
	{
		return p->shared_reference_count();
	}
};

namespace detail {

template <class Pointer>
struct pointer_wrapper
{
	constexpr pointer_wrapper() noexcept : shared_object(nullptr)
	{ }

	constexpr explicit pointer_wrapper(Pointer p) noexcept : shared_object(p)
	{ }

	constexpr pointer_wrapper(const pointer_wrapper& p) noexcept = default;

	pointer_wrapper(pointer_wrapper&& p) noexcept
		: shared_object(p.shared_object)
	{
		p.shared_object = nullptr;
	}

	pointer_wrapper& operator = (const pointer_wrapper&) noexcept = default;

	pointer_wrapper& operator = (pointer_wrapper&& p) noexcept
	{
		shared_object = p.shared_object;
		p.shared_object = nullptr;
		return *this;
	}

	void assign(Pointer ptr) noexcept
	{
		shared_object = ptr;
	}

	Pointer shared_object;
};

template <
	class T,
	class RefCountAccessor, 
	class Deleter,
	class Pointer,
	class PointerImplementation
>
class intrusive_ptr_base
{
	protected:

	using pointer = Pointer;
	using count_type = ref_count_t;

	template <class, class, class, class>
	friend class reference_count_base;

	constexpr intrusive_ptr_base() = default;

	constexpr intrusive_ptr_base(pointer p) noexcept
		: m_impl(p)
	{ }

	template <class RefCountAccessorForwardingReference>
	constexpr explicit intrusive_ptr_base(
		RefCountAccessorForwardingReference&& f,
		std::enable_if_t<
			std::is_constructible<
				RefCountAccessor,
				RefCountAccessorForwardingReference&&
			>::value
		>* = nullptr
	) 
		: m_impl(std::forward<RefCountAccessorForwardingReference>(f))
	{ }

	template <class RefCountAccessorForwardingReference>
	explicit intrusive_ptr_base(
		pointer ptr,
		RefCountAccessorForwardingReference&& f,
		std::enable_if_t<
			std::is_constructible<
				RefCountAccessor,
				RefCountAccessorForwardingReference&&
			>::value
		>* = nullptr
	) 
		: m_impl(ptr, std::forward<RefCountAccessorForwardingReference>(f))
	{ }

	template <
		class RefCountAccessorForwardingReference, 
		class DeleterForwardingReference
	>
	constexpr explicit intrusive_ptr_base(
		RefCountAccessorForwardingReference&& f, 
		DeleterForwardingReference&& d,
		typename std::enable_if<
			std::is_constructible<
				RefCountAccessor,
				RefCountAccessorForwardingReference&&
			>::value
			&& std::is_constructible<
				Deleter,
				DeleterForwardingReference&&
			>::value
		>::type* = nullptr
	)
		: 
		m_impl(
			std::forward<RefCountAccessorForwardingReference>(f), 
			std::forward<DeleterForwardingReference>(d)
		)
	{ }

	template <
		class RefCountAccessorForwardingReference, 
		class DeleterForwardingReference
	>
	explicit intrusive_ptr_base(
		pointer ptr,
		RefCountAccessorForwardingReference&& f, 
		DeleterForwardingReference&& d,
		std::enable_if_t<
			std::is_constructible<
				RefCountAccessor,
				RefCountAccessorForwardingReference&&
			>::value
			&& std::is_constructible<
				Deleter,
				DeleterForwardingReference&&
			>::value
		>* = nullptr
	)
		: 
		m_impl(
			ptr,
			std::forward<RefCountAccessorForwardingReference>(f), 
			std::forward<DeleterForwardingReference>(d)
		)
	{ }

	void assign(pointer ptr) noexcept
	{
		m_impl.assign(ptr);
	}
	
	template <
		class RefCountAccessorForwardingReference, 
		class DeleterForwardingReference
	>
	void assign(
		pointer ptr,
		RefCountAccessorForwardingReference&& f, 
		DeleterForwardingReference&& d
	)
	{
		m_impl.assign(
			ptr, 
			std::forward<RefCountAccessorForwardingReference>(f), 
			std::forward<DeleterForwardingReference>(d)
		);
	}

	void swap(intrusive_ptr_base& other)
	{
		m_impl.swap(other.m_impl);
	}

	struct STDX_MSVC_EMPTY_BASE_CLASSES impl
		:
		PointerImplementation,
		RefCountAccessor, 
		Deleter
	{
		constexpr impl() = default;

		explicit impl(pointer ptr) noexcept : PointerImplementation(ptr)
		{ }

		template <
			class RefCountAccess,
			class = std::enable_if_t<
				std::is_constructible<RefCountAccessor, RefCountAccess&&>::value
			>
		>
		constexpr explicit impl(RefCountAccess&& f) 
			: RefCountAccessor(std::forward<RefCountAccess>(f))
		{ }

		template <
			class RefCountAccess,
			class = std::enable_if_t<
				std::is_constructible<RefCountAccessor, RefCountAccess&&>::value
			>
		>
		impl(pointer ptr, RefCountAccess&& f) 
			: 
			PointerImplementation(ptr),
			RefCountAccessor(std::forward<RefCountAccessor>(f))
		{ }

		template <
			class RefCountAccess,
			class D,
			class = std::enable_if_t<
				std::is_constructible<RefCountAccessor, RefCountAccess&&>::value
				&& std::is_constructible<Deleter, D&&>::value
			>
		>
		constexpr impl(RefCountAccess&& f, D&& d) 
			: 
			RefCountAccessor(std::forward<RefCountAccess>(f)),
			Deleter(std::forward<D>(d))
		{ }

		template <
			class RefCountAccess, 
			class D,
			class = std::enable_if_t<
				std::is_constructible<RefCountAccessor, RefCountAccess&&>::value
				&& std::is_constructible<Deleter, D&&>::value
			>
		>
		impl(pointer ptr, RefCountAccess&& f, D&& d) 
			: 
			PointerImplementation(ptr),
			RefCountAccessor(std::forward<RefCountAccess>(f)),
			Deleter(std::forward<D>(d))
		{ }

		impl(const impl&) = default;
		impl& operator = (const impl&) = default;
		impl(impl&&) = default;
		impl& operator = (impl&&) = default;

		Deleter& get_deleter() noexcept
		{
			return static_cast<Deleter&>(*this);
		}

		const Deleter& get_deleter() const noexcept
		{
			return static_cast<const Deleter&>(*this);
		}

		void assign(pointer ptr) noexcept
		{
			static_cast<PointerImplementation&>(*this).assign(ptr);
		}

		void assign(std::nullptr_t) noexcept
		{
			static_cast<PointerImplementation&>(*this).assign(nullptr);
		}

		void swap(impl& other)
		{
			std::swap(
				static_cast<PointerImplementation&>(*this), 
				static_cast<PointerImplementation&>(other)
			);

			std::swap(static_cast<RefCountAccessor&>(*this), static_cast<RefCountAccessor&>(other));
			std::swap(
				static_cast<Deleter&>(this->get_deleter()),
				static_cast<Deleter&>(other.get_deleter())
			);
		}
	};

	impl m_impl;

	void increment_shared_reference_count(
		std::memory_order order = std::memory_order_relaxed
	) const noexcept
	{
		if (ptr()) ref_count_func()(ptr()).fetch_add(1, order);
	}

	void decrement_shared_reference_count() noexcept
	{
		if (ptr())
		{
			if (ref_count_func()(ptr()).fetch_sub(1, std::memory_order_release) == 1)
			{
				std::atomic_thread_fence(std::memory_order_acquire);
				invoke_deleter(ptr());
			}
		}
	}

	// ----- accessors and modifiers

	pointer& ptr() noexcept
	{
		return static_cast<PointerImplementation&>(m_impl).shared_object;
	}

	constexpr pointer ptr() const noexcept
	{
		return static_cast<const PointerImplementation&>(m_impl).shared_object;
	}

	RefCountAccessor& ref_count_func() noexcept
	{
		return static_cast<RefCountAccessor&>(m_impl);
	}

	const RefCountAccessor& ref_count_func() const noexcept
	{
		return static_cast<const RefCountAccessor&>(m_impl);
	}

	Deleter& deleter() noexcept
	{
		return m_impl.get_deleter();
	}

	const Deleter& deleter() const noexcept
	{
		return m_impl.get_deleter();
	}

	intrusive_ptr<
		T, 
		RefCountAccessor, 
		Deleter, 
		Pointer
	> make_intrusive_pointer(pointer p) const noexcept
	{
		return intrusive_ptr<T, RefCountAccessor, Deleter, Pointer>{
			p,
			ref_count_func(),
			deleter()
		};
	}

	private:

	void invoke_deleter(pointer p)
	{
		m_impl.get_deleter()(p);
	}

	void invoke_deleter(pointer p) const
	{
		m_impl.get_deleter()(p);
	}

	template <class WeakReferenceCountDescriptor>
	void invoke_deleter(pointer p, WeakReferenceCountDescriptor* d)
	{
		maybe_delete_shared_object(p, m_impl.get_deleter(), d);
	}

	template <class WeakReferenceCountDescriptor>
	void invoke_deleter(pointer p, WeakReferenceCountDescriptor* d) const
	{
		maybe_delete_shared_object(p, m_impl.get_deleter(), d);
	}
};

} // end namespace detail

template <
	class T, 
	class RefCountAccessor,
	class Deleter,
	class Pointer
>
class STDX_TRIVIALLY_RELOCATABLE intrusive_ptr
	: 
	public detail::intrusive_ptr_base<
		T, 
		RefCountAccessor, 
		Deleter, 
		Pointer, 
		detail::pointer_wrapper<Pointer>
	>
{
	using base_type = detail::intrusive_ptr_base<
		T, 
		RefCountAccessor, 
		Deleter, 
		Pointer,
		detail::pointer_wrapper<Pointer>
	>;

	public:

	using pointer = Pointer;
	using element_type = T;
	using ref_count_accessor = RefCountAccessor;
	using deleter_type = Deleter;
	using count_type = typename base_type::count_type;

	constexpr intrusive_ptr() noexcept : base_type()
	{ }

	constexpr intrusive_ptr(std::nullptr_t) noexcept : base_type()
	{ }

	template <class RefCountAccess>
	constexpr intrusive_ptr(std::nullptr_t, RefCountAccess&& f)
		: base_type(std::forward<RefCountAccess>(f))
	{ }

	template <class RefCountAccess, class D>
	constexpr intrusive_ptr(std::nullptr_t, RefCountAccess&& f,	D&& d)
		: base_type(std::forward<RefCountAccess>(f), std::forward<D>(d))
	{ }

	constexpr explicit intrusive_ptr(Pointer ptr) noexcept
		: base_type(ptr)
	{ 
		// reference count must initially be >= 1
	}

	template <class RefCountAccess>
	intrusive_ptr(Pointer ptr, RefCountAccess&& f) noexcept
		: base_type(ptr, std::forward<RefCountAccess>(f))
	{ 
		// reference count must initially be >= 1
	}

	template <class RefCountAccess, class D>
	intrusive_ptr(Pointer ptr, RefCountAccess&& f, D&& d) noexcept
		: 
		base_type(
			ptr,
			std::forward<RefCountAccess>(f), 
			std::forward<D>(d)
		)
	{ 
		// reference count must initially be >= 1
	}

	// Copy constructor
	//
	intrusive_ptr(const intrusive_ptr& rhs) noexcept
		: base_type(rhs)
	{
		this->increment_shared_reference_count();
	}

	// Converting copy-constructor
	//
	template <
		class Y, 
		class Ptr, 
		class = std::enable_if_t<std::is_convertible<Ptr, pointer>::value>
	>
	intrusive_ptr(const intrusive_ptr<Y, RefCountAccessor, Deleter, Ptr>& rhs) noexcept
		: 
		base_type(
			rhs.get(),
			rhs.ref_count_func(),
			rhs.get_deleter()
		)
	{
		this->increment_shared_reference_count();
	}

	// Move constructor
	//
	intrusive_ptr(intrusive_ptr&& rhs) noexcept
		: base_type(std::move(rhs))
	{ }

	// Copy assignment
	//
	intrusive_ptr& operator = (const intrusive_ptr& rhs) noexcept
	{
		rhs.increment_shared_reference_count();
		this->decrement_shared_reference_count();

		static_cast<base_type&>(*this) = static_cast<const base_type&>(rhs);
		return *this;
	}

	// Move assignment
	//
	intrusive_ptr& operator = (intrusive_ptr&& rhs) noexcept
	{
		if (this != std::addressof(rhs))
		{
			this->decrement_shared_reference_count();
			static_cast<base_type&>(*this) = std::move(static_cast<base_type&>(rhs));
		}

		return *this;
	}

	~intrusive_ptr() noexcept
	{
		this->decrement_shared_reference_count();
	}

	void reset() noexcept
	{
		this->decrement_shared_reference_count();
		this->assign(nullptr);
	}

	void reset(std::nullptr_t) noexcept
	{
		reset();
	}

	void reset(Pointer ptr) noexcept
	{
		if (this->ptr() != ptr)
		{
			this->decrement_shared_reference_count();
			this->assign(ptr);
			this->increment_shared_reference_count();
		}
	}

	void swap(intrusive_ptr& other) noexcept
	{
		if (this->get() != other.get())
		{
			base_type::swap(other);
		}
	}

	pointer get() const noexcept
	{
		return this->ptr();
	}

	element_type& operator * () const noexcept
	{
		return *this->get();
	}

	pointer operator -> () const noexcept
	{
		return this->get();
	}

	count_type use_count() const noexcept
	{
		return this->ref_count_func()(get()).load(std::memory_order_acquire);
	}

	explicit operator bool() const noexcept
	{
		return static_cast<bool>(this->get());
	}

	Deleter get_deleter() noexcept
	{
		return this->deleter();
	}

	const Deleter& get_deleter() const noexcept
	{
		return this->deleter();
	}

	RefCountAccessor ref_count_access() noexcept
	{
		return this->ref_count_func();
	}

	const RefCountAccessor& ref_count_access() const noexcept
	{
		return this->ref_count_func();
	}

	private:

	template <class Y, class G, class D, class P>
	friend class intrusive_ptr;
};

// -------------- Global equality operators
//
template <class T, class G1, class D1, class P1, class U, class G2, class D2, class P2>
bool operator == (
	const intrusive_ptr<T, G1, D1, P1>& lhs, 
	const intrusive_ptr<U, G2, D2, P2>& rhs
) noexcept
{
	return lhs.get() == rhs.get();
}

template <class T, class G1, class D1, class P1, class U, class G2, class D2, class P2> 
bool operator != (
	const intrusive_ptr<T, G1, D1, P1>& lhs, 
	const intrusive_ptr<U, G2, D2, P2>& rhs
) noexcept
{
	return !(lhs == rhs);
}

template <class T, class G1, class D1, class P1, class U, class G2, class D2, class P2> 
bool operator < (
	const intrusive_ptr<T, G1, D1, P1>& lhs, 
	const intrusive_ptr<U, G2, D2, P2>& rhs
) noexcept
{
	using pointer1 = typename intrusive_ptr<T, G1, D1, P1>::pointer;
	using pointer2 = typename intrusive_ptr<U, G2, D2, P2>::pointer;
	using common_type = typename std::common_type<pointer1, pointer2>::type;

	return std::less<common_type>{}(lhs.get(), rhs.get());
}

template <class T, class G1, class D1, class P1, class U, class G2, class D2, class P2> 
bool operator > (
	const intrusive_ptr<T, G1, D1, P1>& lhs, 
	const intrusive_ptr<U, G2, D2, P2>& rhs
) noexcept
{
	return rhs < lhs;
}

template <class T, class G1, class D1, class P1, class U, class G2, class D2, class P2> 
bool operator <= (
	const intrusive_ptr<T, G1, D1, P1>& lhs, 
	const intrusive_ptr<U, G2, D2, P2>& rhs
) noexcept
{
	return !(rhs < lhs);
}

template <class T, class G1, class D1, class P1, class U, class G2, class D2, class P2> 
bool operator >= (
	const intrusive_ptr<T, G1, D1, P1>& lhs, 
	const intrusive_ptr<U, G2, D2, P2>& rhs
) noexcept
{
	return !(lhs < rhs);
}

template <class T, class G1, class D1, class P1>
bool operator == (const intrusive_ptr<T, G1, D1, P1>& lhs, std::nullptr_t) noexcept
{
	return !lhs;
}

template <class T, class G1, class D1, class P1>
bool operator == (std::nullptr_t, const intrusive_ptr<T, G1, D1, P1>& rhs) noexcept
{
	return !rhs;
}

template <class T, class G1, class D1, class P1>
bool operator != (const intrusive_ptr<T, G1, D1, P1>& lhs, std::nullptr_t) noexcept
{
	return bool(lhs);
}

template <class T, class G1, class D1, class P1>
bool operator != (std::nullptr_t, const intrusive_ptr<T, G1, D1, P1>& rhs) noexcept
{
	return bool(rhs);
}

template <class T, class G1, class D1, class P1>
bool operator < (const intrusive_ptr<T, G1, D1, P1>& lhs, std::nullptr_t) noexcept
{
	using pointer = typename intrusive_ptr<T, G1, D1, P1>::pointer;
	return std::less<pointer>{}(lhs.get(), nullptr);
}

template <class T, class G1, class D1, class P1>
bool operator < (std::nullptr_t, const intrusive_ptr<T, G1, D1, P1>& rhs) noexcept
{
	using pointer = typename intrusive_ptr<T, G1, D1, P1>::pointer;
	return std::less<pointer>{}(nullptr, rhs.get());
}

template <class T, class G1, class D1, class P1>
bool operator > (const intrusive_ptr<T, G1, D1, P1>& lhs, std::nullptr_t) noexcept
{
	return (nullptr < lhs);
}

template <class T, class G1, class D1, class P1>
bool operator > (std::nullptr_t lhs, const intrusive_ptr<T, G1, D1, P1>& rhs) noexcept
{
	return (rhs < nullptr);
}

template <class T, class G1, class D1, class P1>
bool operator <= (const intrusive_ptr<T, G1, D1, P1>& lhs, std::nullptr_t rhs) noexcept
{
	return !(nullptr < lhs);	
}

template <class T, class G1, class D1, class P1>
bool operator <= (std::nullptr_t lhs, const intrusive_ptr<T, G1, D1, P1>& rhs) noexcept
{
	return !(rhs < nullptr);
}

template <class T, class G1, class D1, class P1>
bool operator >= (const intrusive_ptr<T, G1, D1, P1>& lhs, std::nullptr_t rhs) noexcept
{
	return !(lhs < nullptr);
}

template <class T, class G1, class D1, class P1>
bool operator >= (std::nullptr_t lhs, const intrusive_ptr<T, G1, D1, P1>& rhs) noexcept
{
	return !(nullptr < rhs);
}

template <class T, class G, class D, class P> 
void swap(intrusive_ptr<T, G, D, P>& lhs, intrusive_ptr<T, G, D, P>& rhs) noexcept
{
	lhs.swap(rhs);
}

#ifdef STDX_MUST_SPECIALIZE_IS_TRIVIALLY_RELOCATABLE
// -------- specialization for is_trivially_relocatable
//
template <class Y, class G, class D, class P>
struct is_trivially_relocatable<intrusive_ptr<Y,G,D,P>> : std::true_type
{ };
#endif
	
} // end namespace stdx

#endif // include guard


