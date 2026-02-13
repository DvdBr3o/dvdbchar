#pragma once

#include <webgpu/webgpu_cpp.h>
#include <spdlog/spdlog.h>

#include <source_location>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <fstream>
#include <span>

namespace dvdbchar {
	template<typename T>
	struct analyze_member_ptr {};

	template<typename T, typename R>
	struct analyze_member_ptr<R T::*> {
		using type		  = T;
		using member_type = R;
	};

	template<typename T, template<typename... P> class Templ>
	struct like : std::false_type {};

	template<typename... Ts, template<typename... P> class Templ>
	struct like<Templ<Ts...>, Templ> : std::true_type {};

	template<typename T, template<typename... P> class Templ>
	concept Like = like<T, Templ>::value;

	inline static auto panic(
		std::string_view err, std::source_location src = std::source_location::current()
	) {
		spdlog::error("({}:{}): {}", src.file_name(), src.line(), err);
		throw std::runtime_error { std::string { err } };
	}

	template<typename Q>
	struct Query {
		template<class Qy = Q, class Env, class... Args>
			requires requires(Env env, Q q) { env.query(q); }
		[[nodiscard]] [[clang::always_inline]] constexpr auto operator()(
			const Env& env, Args&&... args
		) const noexcept {
			return env.query(Qy(), static_cast<Args&&>(args)...);
		}
	};

	template<typename E, typename Q>
	concept queryable_with = requires(E e, Q q) { e.query(q); };

	inline static auto read_text_from(const std::filesystem::path& path)
		-> std::optional<std::string> {
		std::ifstream in { path };

		if (!in.is_open())
			return std::nullopt;

		in.seekg(0);
		std::string content { std::istreambuf_iterator<char>(in),
							  std::istreambuf_iterator<char>() };
		in.close();

		return content;
	}

	inline static auto read_binary_from(const std::filesystem::path& path)
		-> std::optional<std::vector<uint8_t>> {
		std::ifstream in { path, std::ios::binary };

		if (!in.is_open())
			return std::nullopt;

		in.seekg(0, std::ios::end);
		std::vector<uint8_t> content(in.tellg());
		in.seekg(0, std::ios::beg);
		in.read(reinterpret_cast<char*>(content.data()), content.size());
		in.close();

		return content;
	}

	template<typename T, size_t N>
	inline static constexpr auto to_span(std::array<T, N>&& arr) -> std::span<const T> {
		return { arr.data(), arr.size() };
	}

	template<typename T, size_t N>
	inline static constexpr auto to_span(const std::array<T, N>& arr) -> std::span<const T> {
		return { arr.data(), arr.size() };
	}

	template<typename T>
	inline static constexpr auto to_span(std::vector<T>&& vec) -> std::span<const T> {
		return { vec.data(), vec.size() };
	}

	template<typename T>
	inline static constexpr auto to_span(const std::vector<T>& vec) -> std::span<const T> {
		return { vec.data(), vec.size() };
	}

	// template<typename T>
	// 	requires(!Like<T, std::vector>)
	// inline static constexpr auto to_span(T&& t) -> std::span<const T> {
	// 	return { &t, 1 };
	// }

	template<typename Tag, typename... Ts>
	struct TaggedTuple : public std::tuple<Ts...> {
	public:
		using std::tuple<Ts...>::tuple;

		using tag_type	 = Tag;
		using tuple_type = std::tuple<Ts...>;

	public:
		constexpr auto get() -> std::tuple<Ts...>& { return *this; }

		constexpr auto get() const -> std::tuple<Ts...>&& { return *this; }
	};

	template<typename Tag>
	struct TagGenTaggedTuple {
		template<typename... Ts>
		inline constexpr auto operator()(Ts&&... ts) const -> TaggedTuple<Tag, Ts...> {
			return { std::forward<Ts>(ts)... };
		}
	};

	template<typename T, typename... Cands>
	concept Among = (std::same_as<T, Cands> || ...);

	template<class T>
	inline constexpr void hash_combine(std::size_t& s, const T& v) {
		std::hash<T> h;
		s ^= h(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
	}

	template<typename T, typename BaseT>
	struct is_mem_ptr_of : std::false_type {};

	template<typename Mem, typename BaseT>
	struct is_mem_ptr_of<Mem BaseT::*, BaseT> : std::true_type {};

	template<typename T, typename BaseT>
	concept MemPtrOf = is_mem_ptr_of<T, BaseT>::value;

	template<typename T>
	struct analyze_mem_ptr {};

	template<typename Mem, typename Base>
	struct analyze_mem_ptr<Mem Base::*> {
		using member_type = Mem;
		using base_type	  = Base;
	};

	namespace details::mutex {
		template<typename T>
		class Mutex;

		template<typename T>
		struct is_mutex : std::false_type {};

		template<typename T>
		struct is_mutex<Mutex<T>> : std::true_type {};

		template<typename T>
		struct is_mutex<Mutex<T>&> : std::true_type {};

		template<typename T>
		struct is_mutex<const Mutex<T>&> : std::true_type {};

		template<typename T>
		struct is_mutex<Mutex<T>&&> : std::true_type {};

		template<typename T>
		concept MutexC = is_mutex<T>::value;

		template<typename T>
		class Mutex {
		private:
			class Proxy {
			public:
				explicit Proxy(Mutex& self) : _self(self) { _self._mutex.lock(); }

				Proxy(const Proxy&)			  = delete;
				Proxy(Proxy&&)				  = delete;
				auto& operator=(const Proxy&) = delete;
				auto& operator=(Proxy&&)	  = delete;

				~Proxy() { _self._mutex.unlock(); }

				auto  operator*() -> T& { return _self._self; }

				auto  operator->() -> T* { return &_self._self; }

				auto& operator++() {
					++_self._self;
					return _self;
				}

				auto operator++(int) {
					auto old = *this;
					this->operator++();
					return old;
				}

				auto& operator--() {
					--_self._self;
					return _self;
				}

				auto& operator--(int) {
					auto old = *this;
					this->operator--();
					return old;
				}

				friend auto operator<=>(const Proxy& lhs, const Proxy& rhs) {
					return lhs._self <=> rhs._self;
				}

				// TODO: more operator

			private:
				Mutex& _self;
			};

			class ConstProxy {
			public:
				explicit ConstProxy(const Mutex& self) : _self(self) { _self._mutex.lock(); }

				ConstProxy(const ConstProxy&)	   = delete;
				ConstProxy(ConstProxy&&)		   = delete;
				auto& operator=(const ConstProxy&) = delete;
				auto& operator=(ConstProxy&&)	   = delete;

				~ConstProxy() { _self._mutex.unlock(); }

				auto		operator*() const -> const T& { return _self._self; }

				auto		operator->() const -> const T* { return &_self._self; }

				friend auto operator<=>(const ConstProxy& lhs, const ConstProxy& rhs) {
					return lhs._self <=> rhs._self;
				}

			private:
				const Mutex& _self;
			};

		public:
			template<typename... Args>
			explicit Mutex(Args&&... args) : _self { std::forward<Args>(args)... } {}

		public:
			[[nodiscard]] auto lock() const -> ConstProxy { return ConstProxy { *this }; }

			[[nodiscard]] auto lock_mut() -> Proxy { return Proxy { *this }; }

			// TODO: Remove this. Use more safe wrappers.
			auto original() -> std::tuple<std::mutex&, T&> { return { _mutex, _self }; }

		private:
			mutable std::mutex _mutex;
			T				   _self;
		};

		template<typename T>
		class BoxMutex {
		private:
			class Proxy {
			public:
				explicit Proxy(BoxMutex& self) : _self(self) { _self._mutex->lock(); }

				Proxy(const Proxy&)			  = delete;
				Proxy(Proxy&&)				  = delete;
				auto& operator=(const Proxy&) = delete;
				auto& operator=(Proxy&&)	  = delete;

				~Proxy() { _self._mutex->unlock(); }

				auto  operator*() -> T& { return _self._self; }

				auto  operator->() -> T* { return &_self._self; }

				auto& operator++() {
					++_self._self;
					return _self;
				}

				auto operator++(int) {
					auto old = *this;
					this->operator++();
					return old;
				}

				auto& operator--() {
					--_self._self;
					return _self;
				}

				auto& operator--(int) {
					auto old = *this;
					this->operator--();
					return old;
				}

				friend auto operator<=>(const Proxy& lhs, const Proxy& rhs) {
					return lhs._self <=> rhs._self;
				}

				// TODO: more operator

			private:
				BoxMutex& _self;
			};

			class ConstProxy {
			public:
				explicit ConstProxy(const BoxMutex& self) : _self(self) { _self._mutex->lock(); }

				ConstProxy(const ConstProxy&)	   = delete;
				ConstProxy(ConstProxy&&)		   = delete;
				auto& operator=(const ConstProxy&) = delete;
				auto& operator=(ConstProxy&&)	   = delete;

				~ConstProxy() { _self._mutex->unlock(); }

				auto		operator*() -> const T& { return _self._self; }

				auto		operator->() -> const T* { return &_self._self; }

				friend auto operator<=>(const ConstProxy& lhs, const ConstProxy& rhs) {
					return lhs._self <=> rhs._self;
				}

			private:
				const BoxMutex& _self;
			};

		public:
			template<typename... Args>
			explicit BoxMutex(Args&&... args) : _self { std::forward<Args>(args)... } {}

			BoxMutex(BoxMutex&& another) noexcept :
				_mutex { std::move(another._mutex) }, _self { std::move(another._self) } {}

			auto& operator=(BoxMutex&& another) noexcept {
				if (this != std::addressof(another)) {
					this->_mutex = std::move(another._mutex);
					this->_self	 = std::move(another._self);
				}

				return *this;
			}

		public:
			[[nodiscard]] auto lock() const -> ConstProxy { return ConstProxy { *this }; }

			[[nodiscard]] auto lock_mut() -> Proxy { return Proxy { *this }; }

			// TODO: Remove this. Use more safe wrappers.
			[[deprecated]] auto get() -> std::unique_ptr<std::mutex>& { return _mutex; }

			// TODO: Remove this. Use more safe wrappers.
			[[deprecated]] auto val() -> T& { return _self; }

			[[deprecated]] auto val() const -> const T& { return _self; }

			// TODO: Remove this. Use more safe wrappers.
			auto original() -> std::tuple<std::mutex&, T&> { return { _mutex, _self }; }

		private:
			mutable std::unique_ptr<std::mutex> _mutex = std::make_unique<std::mutex>();
			T									_self;
		};

	}  // namespace details::mutex

	using details::mutex::BoxMutex;
	using details::mutex::Mutex;

	template<typename T>
	class ThreadsafeHandle : std::optional<BoxMutex<T>> {
	public:
		using std::optional<BoxMutex<T>>::optional;
		using std::optional<BoxMutex<T>>::operator bool;
		using std::optional<BoxMutex<T>>::emplace;
		using std::optional<BoxMutex<T>>::operator=;

	public:
		auto& operator=(const T& handle) {
			emplace(handle);
			return *this;
		}

		auto& operator=(T&& handle) {
			emplace(std::move(handle));
			return *this;
		}

		auto		operator->() { return this->value().lock_mut(); }

		auto		operator->() const { return this->value().lock(); }

		auto		lock() const { return this->value().lock(); }

		auto		lock_mute() { return this->value().lock_mute(); }

		auto&		get() { return this->value().val(); }

		const auto& get() const { return this->value().val(); }

		//
		explicit operator T&() { return get(); }

		explicit operator const T&() const { return get(); }
	};

	template<auto f>
	concept ConstEvaluated = requires { typename std::bool_constant<(f, true)>; };

	template<typename T>
	concept FatPointerAlike = requires(T t) {
		t.data();
		t.size();
	};
}  // namespace dvdbchar

template<>
struct std::hash<wgpu::TextureDimension> {
	std::size_t operator()(const wgpu::TextureDimension& s) const noexcept {
		return std::hash<uint32_t>()(static_cast<uint32_t>(s));
	}
};

template<>
struct std::hash<wgpu::Extent3D> {
	std::size_t operator()(const wgpu::Extent3D& s) const noexcept {
		using dvdbchar::hash_combine;

		size_t res = 0;
		hash_combine(res, s.depthOrArrayLayers);
		hash_combine(res, s.height);
		hash_combine(res, s.width);
		return res;
	}
};

template<>
struct std::hash<wgpu::TextureDescriptor> {
	std::size_t operator()(const wgpu::TextureDescriptor& s) const noexcept {
		using dvdbchar::hash_combine;

		size_t res = 0;
		hash_combine(res, static_cast<uint64_t>(s.usage));
		hash_combine(res, s.dimension);
		hash_combine(res, s.size);
		hash_combine(res, static_cast<uint32_t>(s.format));
		hash_combine(res, s.mipLevelCount);
		hash_combine(res, s.sampleCount);
		hash_combine(res, s.viewFormatCount);
		return res;
	}
};