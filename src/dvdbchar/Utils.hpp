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

	template<typename T>
	inline static constexpr auto to_span(T&& t) -> std::span<const T> {
		return { &t, 1 };
	}

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

}  // namespace dvdbchar

template<>
struct std::hash<wgpu::TextureDimension> {
	constexpr std::size_t operator()(const wgpu::TextureDimension& s) const noexcept {
		return std::hash<uint32_t>()(static_cast<uint32_t>(s));
	}
};

template<>
struct std::hash<wgpu::Extent3D> {
	constexpr std::size_t operator()(const wgpu::Extent3D& s) const noexcept {
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
	constexpr std::size_t operator()(const wgpu::TextureDescriptor& s) const noexcept {
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