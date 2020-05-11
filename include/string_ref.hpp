#ifndef STDX_STRING_REF_HPP
#define STDX_STRING_REF_HPP

#include <cstring>
#include <cstddef>
#include <atomic>

namespace stdx {

class string_ref;

namespace detail {

	constexpr const char* cstring_null_scan(const char* s) noexcept
	{
		return *s ? cstring_null_scan(s + 1) : s;
	}

} // end namespace detail

class string_ref
{
	protected:

	class state_type;

	public:

	using value_type = const char;
	using size_type = std::size_t;
	using pointer = const char*;
	using const_pointer = const char*;
	using iterator = const char*;
	using const_iterator = const char*;

	struct resource_management
	{
		using copy_constructor = state_type(*)(const string_ref&);
		using move_constructor = state_type(*)(string_ref&&);
		using destructor = void(*)(string_ref&);

		constexpr resource_management() noexcept
			: copy{nullptr}, move{nullptr}, destroy{nullptr}
		{ }

		constexpr resource_management(
			copy_constructor cctor,
			move_constructor mctor,
			destructor dtor
		) noexcept
			: copy{cctor}, move{mctor}, destroy{dtor}
		{ }

		copy_constructor copy;
		move_constructor move;
		destructor destroy;
	};

	constexpr string_ref() noexcept : m_begin(nullptr), m_end(nullptr), context{}
	{ }

	constexpr string_ref(const char* beg) noexcept 
		: m_begin(beg), m_end(detail::cstring_null_scan(beg)), context{}
	{ }

	constexpr string_ref(const char* beg, const char* e) noexcept
		: m_begin(beg), m_end(e), context{}
	{ }

	constexpr string_ref(const char* beg, resource_management rm) noexcept 
		:
		m_begin(beg),
		m_end(detail::cstring_null_scan(beg)),
		m_resource_management(rm),
		context{}
	{ }

	constexpr string_ref(const char* beg, const char* e, resource_management rm) noexcept
		: m_begin(beg), m_end(e), m_resource_management(rm), context{}
	{ }

	constexpr string_ref(
		const char* beg, 
		const char* e, 
		resource_management rm, 
		void* ctx
	) noexcept
		: m_begin(beg), m_end(e), m_resource_management(rm), context{ctx}
	{ }

	STDX_GCC7_WORKAROUND_CONSTEXPR string_ref(const string_ref& s)
		: string_ref{s.m_resource_management.copy ? s.m_resource_management.copy(s) : s.state()}
	{ }

	STDX_GCC7_WORKAROUND_CONSTEXPR string_ref(string_ref&& s)
		: 
		string_ref{
			s.m_resource_management.move ? 
				s.m_resource_management.move(std::move(s)) : s.state()
		}
	{ }

	string_ref& operator = (const string_ref& s)
	{
		string_ref tmp = s;
		*this = std::move(tmp);
		return *this;
	}

	string_ref& operator = (string_ref&& s)
	{
		if (this != &s)
		{
			if (m_resource_management.destroy) m_resource_management.destroy(*this);

			// This is legal because of the common initial sequence and the fact
			// that any type erased object must be trivially relocatable.
			*this = string_ref_state_union{std::move(s)}.state;
		}

		return *this;
	}

	~string_ref() noexcept
	{
		if (m_resource_management.destroy) m_resource_management.destroy(*this);
	}

	bool empty() const noexcept { return m_begin == m_end; }

	size_type size() const noexcept { return m_end - m_begin; }

	const_pointer data() const noexcept { return m_begin; }

	iterator begin() noexcept { return m_begin; }

	iterator end() noexcept { return m_end; }

	const_iterator begin() const noexcept { return m_begin; }

	const_iterator end() const noexcept { return m_end; }

	const_iterator cbegin() const noexcept { return m_begin; }

	const_iterator cend() const noexcept { return m_end; }

	protected:

	struct state_type
	{
		pointer m_begin;
		pointer m_end;
		resource_management m_resource_management;
		void* context;
	};

	state_type state() const noexcept
	{
		return state_type{m_begin, m_end, m_resource_management, context};
	}

	constexpr explicit string_ref(const state_type& s) noexcept
		:
		m_begin(s.m_begin),
		m_end(s.m_end),
		m_resource_management(s.m_resource_management),
		context(s.context)
	{ }

	void clear() noexcept
	{
		m_begin = nullptr;
		m_end = nullptr;
	}

	template <class StringRef>
	union string_ref_state_union_type
	{
		explicit string_ref_state_union_type(StringRef&& s) : str(std::move(s))
		{ }

		~string_ref_state_union_type() noexcept {}

		StringRef str;
		state_type state;
	};

	using string_ref_state_union = string_ref_state_union_type<string_ref>;

	void operator = (const state_type& s) noexcept
	{
		m_begin = s.m_begin;
		m_end = s.m_end;
		m_resource_management = s.m_resource_management;
		context = s.context;
	}

	pointer m_begin;
	pointer m_end;
	resource_management m_resource_management;
	void* context;
};

inline bool operator == (const string_ref& lhs, const string_ref& rhs) noexcept
{
	return (lhs.size() == rhs.size()) && (std::memcmp(lhs.data(), rhs.data(), lhs.size()) == 0);
}

inline bool operator != (const string_ref& lhs, const string_ref& rhs) noexcept
{
	return !(lhs == rhs);
}

inline bool operator < (const string_ref& lhs, const string_ref& rhs) noexcept
{
	const std::size_t sz = (lhs.size() < rhs.size()) ? lhs.size() : rhs.size();
	int result = std::memcmp(lhs.data(), rhs.data(), sz);
	if (result == 0) return lhs.size() < rhs.size();
	return result < 0;
}

inline bool operator > (const string_ref& lhs, const string_ref& rhs) noexcept
{
	return rhs < lhs;
}

inline bool operator <= (const string_ref& lhs, const string_ref& rhs) noexcept
{
	return !(lhs > rhs);
}

inline bool operator >= (const string_ref& lhs, const string_ref& rhs) noexcept
{
	return !(lhs < rhs);
}

// Reference-counted allocated string
//
class shared_string_ref : public string_ref
{
	struct string_arena_base
	{
		mutable std::atomic<std::size_t> ref_count;
		std::size_t length;
	};

	struct string_arena : string_arena_base
	{
		constexpr explicit string_arena(std::size_t length) noexcept
			: string_arena_base{{1}, length}
		{ }

		constexpr static std::size_t header_size() noexcept
		{
			return sizeof(string_arena);
		}

		char* data() noexcept
		{
			return reinterpret_cast<char*>(this) + header_size();
		}

		const char* data() const noexcept
		{
			return reinterpret_cast<const char*>(this) + header_size();
		}

		const char* begin() const noexcept { return data(); }

		const char* end() const noexcept { return data() + length; }
	};

	string_ref allocate_string_ref(const char* s, std::size_t length)
	{
		const std::size_t arena_size = string_arena::header_size() + length;
		char* buf = static_cast<char*>(::operator new(arena_size));
		string_arena* a = new (buf) string_arena{length};
		std::memcpy(a->data(), s, length);
		return shared_string_ref{a};
	}

	explicit shared_string_ref(string_arena* a) noexcept
		: 
		string_ref{
			a->begin(), 
			a->end(), 
			string_ref::resource_management{&copy_construct, &move_construct, &destroy},
			a
		}
	{ }

	const string_arena_base* get_arena() const noexcept
	{
		return static_cast<string_arena_base*>(this->context);
	}

	string_arena_base* get_arena() noexcept
	{
		return static_cast<string_arena_base*>(this->context);
	}

	static string_ref::state_type copy_construct(const string_ref& base) noexcept
	{
		const shared_string_ref& s = static_cast<const shared_string_ref&>(base);
		const string_arena_base* a = s.get_arena();
		if (a) a->ref_count.fetch_add(1, std::memory_order_relaxed);
		return s.state();
	}

	static string_ref::state_type move_construct(string_ref&& base) noexcept
	{
		shared_string_ref& s = static_cast<shared_string_ref&>(base);
		auto st = s.state();
		s.context = nullptr;
		s.clear();
		return st;	
	}

	static void destroy(string_ref& base) noexcept
	{
		shared_string_ref& s = static_cast<shared_string_ref&>(base);
		string_arena* a = static_cast<string_arena*>(s.get_arena());
		if (a && (a->ref_count.fetch_sub(1, std::memory_order_release) == 1))
		{
			std::atomic_thread_fence(std::memory_order_acquire);
			::operator delete(a);
		}
	}

	template <class Allocator>
	struct allocated_string_arena : string_arena_base
	{
		constexpr allocated_string_arena(const Allocator& alloc, std::size_t length) noexcept
			: string_arena_base{{1}, length}, allocator(alloc)
		{ }

		constexpr static std::size_t header_size() noexcept
		{
			return sizeof(allocated_string_arena);
		}

		char* data() noexcept
		{
			return reinterpret_cast<char*>(this) + header_size();
		}

		const char* data() const noexcept
		{
			return reinterpret_cast<const char*>(this) + header_size();
		}

		const char* begin() const noexcept { return data(); }

		const char* end() const noexcept { return data() + length; }

		std::size_t allocated_size() const noexcept { return header_size() + length; }

		Allocator allocator;
	};

	template <class Allocator>
	explicit shared_string_ref(allocated_string_arena<Allocator>* a) noexcept
		: 
		string_ref{
			a->begin(), 
			a->end(), 
			string_ref::resource_management{
				&copy_construct,
				&move_construct,
				&allocator_destroy<Allocator>
			},
			a
		}
	{ }

	template <class Allocator>
	string_ref allocate_string_ref(
		const Allocator& allocator, 
		const char* s,
		std::size_t length
	)
	{
		using allocator_type = typename std::allocator_traits<
			Allocator
		>::template rebind_alloc<char>;
		using arena_type = allocated_string_arena<allocator_type>;

		allocator_type alloc{allocator};
		const std::size_t arena_size = arena_type::header_size() + length;
		char* buf = alloc.allocate(arena_size);
		arena_type* a = new (buf) arena_type{alloc, length};
		std::memcpy(a->data(), s, length);
		return shared_string_ref{a};
	}

	template <class Allocator>
	static void allocator_destroy(string_ref& base) noexcept
	{
		using arena_type = allocated_string_arena<Allocator>;

		shared_string_ref& s = static_cast<shared_string_ref&>(base);
		arena_type* a = static_cast<arena_type*>(s.get_arena());
		if (a && (a->ref_count.fetch_sub(1, std::memory_order_release) == 1))
		{
			std::atomic_thread_fence(std::memory_order_acquire);

			Allocator alloc = std::move(a->allocator);
			const std::size_t allocated_size = a->allocated_size();
			a->~arena_type();
			alloc.deallocate(reinterpret_cast<char*>(a), allocated_size);
		}
	}

	public:

	shared_string_ref(const char* beg)
		: string_ref{allocate_string_ref(beg, detail::cstring_null_scan(beg) - beg)}
	{ }

	shared_string_ref(const char* beg, const char* end)
		: string_ref{allocate_string_ref(beg, end - beg)}
	{ }

	template <class Allocator>
	shared_string_ref(const Allocator& alloc, const char* beg)
		: string_ref{allocate_string_ref(alloc, beg, detail::cstring_null_scan(beg) - beg)}
	{ }

	template <class Allocator>
	shared_string_ref(const Allocator& alloc, const char* beg, const char* end)
		: string_ref{allocate_string_ref(alloc, beg, end - beg)}
	{ }

	std::size_t use_count() const noexcept
	{
		const string_arena_base* a = get_arena();
		return a ? a->ref_count.load(std::memory_order_acquire) : 0;
	}
};

} // end namespace stdx

#endif


