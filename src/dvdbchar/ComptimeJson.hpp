#pragma once

#include <exception>
#include <optional>
#include <string_view>
#include <algorithm>
#include <type_traits>
#include <variant>
#include <utility>
#include <vector>
#include <array>

// Inspired from: https://medium.com/@abdulgh/compile-time-json-deserialization-in-c-1e3d41a73628

namespace dvdbchar {
	template<size_t N>
	struct FixedString {
		std::array<char, N>			 str;

		inline static constexpr auto size = N;

		constexpr FixedString(const char (&data)[N]) { std::copy(data, data + N, str.begin()); }

		constexpr	   operator std::string_view() const { return { str.data(), size - 1 }; }

		constexpr auto operator[](size_t i) const -> char { return str[i]; }

		inline friend constexpr auto operator==(const FixedString& l, const FixedString& r)
			-> bool {
			return [&]<size_t... Is>(std::index_sequence<Is...>) {
				return ((l[Is] == r[Is]) && ...);
			}(std::make_index_sequence<N>());
		}
	};

	template<size_t N>
	inline static constexpr auto to_fixed_string(const char (&str)[N]) -> FixedString<N> {
		return { str };
	}

	template<FixedString fs>
	inline static constexpr auto operator""_fs() {
		return fs;
	}

	struct StringView {
		const char* data {};
		std::size_t size {};

		StringView() noexcept = default;

		template<size_t N>
		constexpr StringView(const char (&data)[N]) noexcept : data(data), size(N) {}

		constexpr StringView(std::string_view view) noexcept :
			data(view.data()), size(view.size()) {}

		StringView(const StringView&) noexcept							   = default;
		StringView(StringView&&) noexcept								   = default;
		StringView&					 operator=(const StringView&) noexcept = default;
		StringView&					 operator=(StringView&&) noexcept	   = default;

		inline constexpr auto		 operator[](size_t i) const -> char { return data[i]; }

		explicit constexpr			 operator std::string_view() const { return { data, size }; }

		inline friend constexpr auto operator==(StringView l, StringView r) -> bool {
			if (l.size != r.size)
				return false;
			for (int i = 0; i < l.size; ++i)
				if (l[i] != r[i])
					return false;
			return true;
		}
	};

	template<typename... Ts>
	struct is_variant : std::false_type {};

	template<typename... Ts>
	struct is_variant<std::variant<Ts...>> : std::true_type {};

	template<typename T>
	constexpr bool is_variant_v = is_variant<T>::value;

	template<FixedString fs>
	struct CompileError : public std::exception {
		CompileError();

		auto what() { return (std::string_view)fs; }
	};

	template<FixedString fs>
	inline static constexpr auto operator""_ce() {
		return CompileError<fs> {};
	};

	inline static constexpr auto strip_space(std::string_view sv) -> std::string_view {
		while (!sv.empty() && (sv[0] == ' ' || sv[0] == '\t' || sv[0] == '\n')) sv.remove_prefix(1);
		return sv;
	}

	struct String {
		using value_type = StringView;

		StringView value;

		String() = default;

		constexpr String(std::string_view& sv) { sv = match(strip_space(sv)); }

		constexpr auto match(std::string_view sv) -> std::string_view {
			if (!sv.empty() && sv[0] != '\"')
				throw "expect string quote"_ce;
			sv.remove_prefix(1);
			if (const auto close = sv.find('\"'); close != std::string_view::npos) {
				value = StringView { sv.substr(0, close) };
				sv.remove_prefix(close + 1);
				return sv;
			} else
				throw "string not closed"_ce;
		}
	};

	struct Number {
		using value_type = double;

		double value	 = 0.;

		Number()		 = default;

		constexpr Number(std::string_view& sv) { sv = match(strip_space(sv)); }

		constexpr auto match(std::string_view sv) -> std::string_view {
			if (!sv.empty() && !('0' <= sv[0] && sv[0] <= '9' || sv[0] == '.'))
				throw "expect number!"_ce;
			while (!sv.empty() && '0' <= sv[0] && sv[0] <= '9') {
				value = value * 10 + sv[0] - '0';
				sv.remove_prefix(1);
			}
			if (!sv.empty() && sv[0] == '.') {
				sv.remove_prefix(1);
				double expo = .1;
				while (!sv.empty() && '0' <= sv[0] && sv[0] <= '9') {
					value += (sv[0] - '0') * expo;
					expo *= .1;
					sv.remove_prefix(1);
				}
			}
			return sv;
		}
	};

	struct Boolean {
		using value_type = bool;

		bool value;

		Boolean() = default;

		constexpr Boolean(std::string_view& sv) { sv = match(strip_space(sv)); }

		constexpr auto match(std::string_view sv) -> std::string_view {
			if (sv.substr(0, 4) == "true") {
				value = true;
				sv	  = sv.substr(4);
			} else if (sv.substr(0, 5) == "false") {
				value = false;
				sv	  = sv.substr(5);
			} else
				throw "expect boolean!"_ce;
			return sv;
		}
	};

	struct Null {
		using value_type = std::monostate;

		std::monostate value;

		constexpr Null(std::string_view& sv) { sv = match(strip_space(sv)); }

		constexpr auto match(std::string_view sv) -> std::string_view {
			if (sv.substr(0, 4) == "null")
				sv = sv.substr(4);
			else
				throw "expect null!"_ce;
			return sv;
		}
	};

	template<typename T>
	struct Nullable {
		using value_type = std::optional<typename T::value_type>;

		value_type value;

		constexpr Nullable(std::string_view& sv) { sv = match(strip_space(sv)); }

		constexpr auto match(std::string_view sv) -> std::string_view {
			if (sv.substr(0, 4) == "null") {
				sv	  = sv.substr(4);
				value = std::nullopt;
			} else {
				try {
					T t;
					sv = t.match(sv);
					value.emplace(std::move(t.value));
				} catch (...) { throw "expect nullable element!"_ce; }
			}
			return sv;
		}
	};

	template<typename T>
	struct Array {
		using value_type = struct LazyArray {
			StringView raw;

			//
			constexpr auto operator[](size_t i) const -> T {
				auto sv = std::string_view { raw };
				while (!sv.empty() && i)
					if (const auto comma = sv.find(','); comma != std::string_view::npos) {
						sv.remove_prefix(comma + 1);
						--i;
					} else
						throw "out of index!"_ce;

				return T { sv };
			}
		};

		value_type value;

		constexpr Array(std::string_view& sv) { sv = match(strip_space(sv)); }

		constexpr auto match(std::string_view sv) -> std::string_view {
			if (sv[0] != '[')
				throw "expected array!"_ce;

			std::vector<char> bs = { '[' };
			size_t			  i	 = 1;
			while (!sv.empty() && !bs.empty()) {
				switch (sv[i]) {
					case '[': bs.emplace_back('['); break;
					case ']': bs.pop_back(); break;
					default: break;
				}
				++i;
			}

			if (sv.empty() && !bs.empty())
				throw "brackets mismatched!"_ce;

			value = LazyArray { StringView { sv.substr(1, i - 2) } };

			sv.remove_prefix(i);

			return sv;
		}

		constexpr auto operator[](size_t i) const -> T { return value[i]; }
	};

	template<FixedString fs>
	struct Key {
		inline static constexpr auto key  = fs;
		inline static constexpr auto size = fs.size;

		constexpr Key()					  = default;

		constexpr operator std::string_view() const { return (std::string_view)fs; }
	};

	template<Key fs, typename V>
	struct Pair {
		inline static constexpr auto key = fs;

		using value_type				 = typename V::value_type;
		using value_matcher				 = V;
		using pair_type					 = Pair;

		value_type value;

		constexpr Pair(std::string_view& sv) { sv = match(strip_space(sv)); }

		constexpr auto match(std::string_view sv) -> std::string_view {
			const auto expect = [&sv](char c) {
				if (!sv.empty() && sv[0] != c)
					throw;
				else
					sv.remove_prefix(1);
			};
			auto k = String { sv }.value;
			if ((std::string_view)k != (std::string_view)key)
				throw;

			expect(':');
			value = std::move(V { sv }.value);
			if (!sv.empty() && sv[0] == ',')
				sv.remove_prefix(1);

			return sv;
		}
	};

	struct LazyPair {
		StringView key;
		StringView value;

		constexpr LazyPair(std::string_view& sv) { sv = match(strip_space(sv)); }

		constexpr auto match(std::string_view sv) -> std::string_view {
			const auto expect = [&sv](char c) {
				if (sv[0] != c)
					throw;
				else
					sv.remove_prefix(1);
			};

			key = String { sv }.value;

			expect(':');

			sv		 = strip_space(sv);

			size_t i = 0;
			while (i < sv.size() && sv[i] != ',') ++i;
			value = StringView { sv.substr(0, i) };

			sv.remove_prefix(i);

			if (!sv.empty() && sv[0] == ',')
				sv.remove_prefix(1);

			return sv;
		}
	};

	template<typename T>
	struct pair_like : std::false_type {};

	template<Key k, typename V>
	struct pair_like<Pair<k, V>> : std::true_type {};

	template<typename T>
	concept PairLike = pair_like<T>::value;

	template<FixedString fs>
	inline static constexpr auto operator""_key() {
		return Key<fs>();
	}

	template<FixedString k, typename Tp>
	struct find_key {
		static_assert(false, "type mismatched!");
	};

	template<FixedString k, PairLike P0, PairLike... Ps>
		requires((std::string_view)k == P0::key)
	struct find_key<k, std::tuple<P0, Ps...>> {
		inline static constexpr auto key = k;
		using value_type				 = P0::value_type;
		using value_matcher				 = P0::value_matcher;
		using pair_type					 = P0::pair_type;
	};

	template<FixedString k, PairLike P0, PairLike... Ps>
		requires((std::string_view)k != P0::key)
	struct find_key<k, std::tuple<P0, Ps...>> : find_key<k, std::tuple<Ps...>> {};

	template<FixedString k>
	struct find_key<k, std::tuple<>> {
		static_assert(false, "key not found!");
	};

	template<PairLike... ps>
	struct Dict {
		using value_type = struct LazyDict {
			StringView raw;

			//
			template<FixedString fs>
			constexpr auto operator[](Key<fs>) const
				-> find_key<fs, std::tuple<ps...>>::value_type {
				// using V = find_key<fs, std::tuple<ps...>>::pair_type;
				using V = find_key<fs, std::tuple<ps...>>::value_matcher;
				auto sv = std::string_view { raw };

				while (!sv.empty()) {
					sv				  = strip_space(sv);
					const auto [k, v] = LazyPair { sv };
					if (k == (std::string_view)fs) {
						auto vv = (std::string_view)v;
						return V { vv }.value;
					}
				}

				throw "key not found"_ce;
			}
		};

		template<FixedString fs>
		using value_query_of_key = find_key<fs, std::tuple<ps...>>;

		value_type value;

		Dict() = default;

		constexpr Dict(std::string_view& sv) { sv = match(strip_space(sv)); }

		constexpr auto match(std::string_view sv) -> std::string_view {
			if (sv[0] != '{')
				throw "expected dict!"_ce;

			std::vector<char> bs = { '{' };
			size_t			  i	 = 1;
			while (!sv.empty() && !bs.empty()) {
				switch (sv[i]) {
					case '{': bs.emplace_back('{'); break;
					case '}': bs.pop_back(); break;
					default: break;
				}
				++i;
			}

			if (sv.empty() && !bs.empty())
				throw "braces mismatched!"_ce;

			value = LazyDict { StringView { sv.substr(1, i - 2) } };

			sv.remove_prefix(i);

			return sv;
			return sv;
		}

		template<FixedString fs>
		constexpr auto operator[](Key<fs> key) const {
			return value[key];
		}
	};

	template<typename T>
	inline static constexpr auto parse(std::string_view sv) -> T {
		T t { sv };
		if (!strip_space(sv).empty())
			throw sv;
		return t;
	}

	template<typename T>
	inline static constexpr auto try_parse(std::string_view sv) {
		T t { sv };
		// return sv;
		return t;
	}

	static_assert((std::string_view)parse<String>("\"hello\"").value == "hello");
	static_assert(parse<Number>("12.3").value == 12.3);
	static_assert(parse<Number>(".3").value - .3 < .000001);
	static_assert(parse<Number>("13.").value == 13.);
	static_assert(parse<Boolean>("true").value == true);
	static_assert(parse<Boolean>("false").value == false);
	static_assert(parse<Null>("null").value == std::monostate {});
	static_assert(parse<Nullable<Number>>("null").value == std::nullopt);
	static_assert(parse<Nullable<Number>>("12").value == 12);
	static_assert(parse<Array<Number>>("[12, 24]")[0].value == 12);
	static_assert(parse<Array<Number>>("[12, 24]")[1].value == 24);
	static_assert(
		(std::string_view)parse<Array<String>>(R"(["hello", "world"])")[0].value == "hello"
	);
	static_assert(
		(std::string_view)parse<Array<String>>(R"(["hello", "world"])")[1].value == "world"
	);
	static_assert(parse<Pair<"hello"_key, Number>>(R"("hello": 1)").value == 1);
	static_assert(
		(std::string_view)parse<Pair<"gender"_key, String>>(R"("gender": "male")").value == "male"
	);
	static_assert((std::string_view)parse<String>(R"(   "age" )").value == "age");
	static_assert(parse<Dict<Pair<"age"_key, Number>>>(R"({ "age": 19 })")["age"_key] == 19);
	static_assert(
		(std::string_view)parse<Dict<Pair<"age"_key, Number>, Pair<"name"_key, String>>>(
			R"({ "age": 19, "name": "dvdbr3o" })"
		)["name"_key]
		== "dvdbr3o"
	);
	static_assert(
		(std::string_view)parse<Dict<Pair<"name"_key, String>>>(
			R"({ "name": "dvdbr3o", "age": [12, 13] })"
		)["name"_key]
		== "dvdbr3o"
	);
	static_assert(
		std::same_as<
			find_key<"name"_fs, std::tuple<Pair<"age"_key, Number>, Pair<"name"_key, String>>>::
				value_matcher,
			String>
	);

}  // namespace dvdbchar