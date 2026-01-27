#pragma once

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

	template<typename T, template<typename P> class Templ>
	struct Like : std::false_type {};

	template<typename... Ts, template<typename P> class Templ>
	struct Like<Templ<Ts...>, Templ> : std::true_type {};

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

}  // namespace dvdbchar