#pragma once

#include "Render/Context.hpp"
#include "dvdbchar/Render/Context.hpp"
#include "dvdbchar/Utils.hpp"

#include <glm/glm.hpp>
#include <simdjson.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <variant>
#include <type_traits>

namespace dvdbchar::Render {
	struct LayoutEntry {
		size_t set;
		size_t binding;
	};

	struct UniformBufferEntry : LayoutEntry {
		size_t offset;
		size_t size;
	};

	struct TextureEntry : LayoutEntry {};

	struct SamplerEntry : LayoutEntry {};

	using VarReflEntry = std::variant<UniformBufferEntry, TextureEntry, SamplerEntry>;

	struct ReflectionEntry {
		size_t set;
		size_t binding;
		size_t offset;
		size_t size;
	};

	template<typename T>
	struct ReflectionRegistry {};

	template<typename T>
	concept ReflMapped = requires { ReflectionRegistry<T>::mapping(); };

	template<ReflMapped T>
	inline static consteval auto mapping() {
		return ReflectionRegistry<T>::mapping();
	}

	template<typename T>
	struct Field : public ReflectionEntry {
		inline static constexpr auto mapped = false;
	};

	template<ReflMapped T>
	struct Field<T> : public ReflectionEntry {
		inline static constexpr auto mapped = true;
	};

	template<typename T>
	concept FieldLike = Like<T, Field>::value;

	template<ReflMapped T>
	struct ReflectedParameter : public T {
		size_t				  set;
		size_t				  offset;
		size_t				  size;

		inline constexpr auto get() -> T& { return *this; }

		inline constexpr auto get() const -> const T& { return *this; }
	};

	template<typename T>
	struct remove_reflected_parameter {
		using type = T;
	};

	template<ReflMapped T>
	struct remove_reflected_parameter<ReflectedParameter<T>> {
		using type = T;
	};

	template<ReflMapped T>
	struct ReflectionRegistry<ReflectedParameter<T>> {
		inline static consteval auto mapping() { return ReflectionRegistry<T>::mapping(); }
	};

	template<typename T>
	struct ParameterRegistry {};

	template<typename T>
	concept ParamMapped = requires { ParameterRegistry<T>::mapping(); };

	template<ReflMapped T, typename F>
	inline static constexpr auto process_field(
		T& parent, std::string_view name, Field<F> T::* field, simdjson::ondemand::value&& json,
		uint32_t set = 0
	) {
		for (auto&& cur : json["type"]["fields"]) {
			if (cur["name"]->get_string() == name) {
				auto&	   info = parent.*field;

				const auto kind = *cur["binding"]["kind"]->get_string();
				if (kind == "uniform") {
					info.set	 = set;
					info.binding = 0;
					info.offset	 = cur["binding"]["offset"];
					info.size	 = cur["binding"]["size"];
				} else if (kind == "descriptorTableSlot") {
					info.set	 = set;
					info.binding = cur["binding"]["index"];
				}

				return;
			}
		}
		panic(std::format("{} not found!", name));
		throw;
	}

	template<ReflMapped T>
	inline static constexpr auto bind(T& t, simdjson::ondemand::value&& json) {
		constexpr auto mapping = ReflectionRegistry<T>::mapping();
		std::apply(
			[&](auto&&... fields) {
				(process_field(t, fields.first, fields.second, *json["type"]["elementVarLayout"]),
				 ...);
			},
			mapping
		);
	}

	template<ReflMapped T>
	inline static constexpr auto get_mapping(
		std::string_view name, simdjson::ondemand::value&& json
	) -> ReflectedParameter<T> {
		ReflectedParameter<T> t;
		for (auto&& param : json["parameters"])
			if (param["name"]->get_string() == name) {
				constexpr auto mapping = ReflectionRegistry<T>::mapping();
				std::apply(
					[&](auto&&... fields) {
						(process_field(
							 t.get(),
							 fields.first,
							 fields.second,
							 *param["type"]["elementVarLayout"],
							 param["binding"]["index"]->get_int64()
						 ),
						 ...);
					},
					mapping
				);
				// if (*param["type"]["elementVarLayout"]["binding"]["kind"]->get_string()
				// 	== "uniform")
				// 	t.size = param["type"]["elementVarLayout"]["binding"]["size"];
				// spdlog::info("{}.size = {}", name, t.size);

				t.offset = 0;
				if (param["type"]["elementVarLayout"]["bindings"].has_value()) {
					for (auto&& binding : param["type"]["elementVarLayout"]["bindings"])
						if (*binding["kind"]->get_string() == "uniform")
							t.size = *binding["size"]->get_number();
				} else if (param["type"]["elementVarLayout"]["binding"].has_value()) {
					t.size = *param["type"]["elementVarLayout"]["binding"]["size"]->get_number();
				}
				t.set = param["binding"]["index"];
				return t;
			}
		panic(std::format("No parameter named {}!", name));
		throw;
	}

	template<ParamMapped T>
	inline static constexpr auto get_mapping(simdjson::ondemand::value&& json) -> T {
		constexpr auto mapping = ParameterRegistry<T>::mapping();

		T			   t;
		std::apply(
			[&](auto&&... fields) {
				(
					[&]() {
						(t.*(fields.second)) = get_mapping<
							typename remove_reflected_parameter<typename analyze_member_ptr<
								decltype(fields.second)>::member_type>::type>(
							fields.first,
							std::move(json)
						);
					}(),
					...
				);
			},
			mapping
		);
		return t;
	}

	template<ReflMapped T>
	inline static auto get_mapping(std::string_view name, const std::filesystem::path& path) {
		auto parser = simdjson::ondemand::parser {};
		auto str	= simdjson::padded_string::load(path.string());
		auto json	= parser.iterate(str);
		return get_mapping<T>(name, std::move(*json));
	}

	template<ParamMapped T>
	inline static auto get_mapping(const std::filesystem::path& path) {
		auto parser = simdjson::ondemand::parser {};
		auto str	= simdjson::padded_string::load(path.string());
		auto json	= parser.iterate(str);
		return get_mapping<T>(*json);
	}

	inline static constexpr auto layout_entry(
		uint32_t index, simdjson::ondemand::value&& binding,
		wgpu::ShaderStage visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment
	) -> wgpu::BindGroupLayoutEntry {
		if (binding["kind"] == "uniform")
			return {
				.binding = index,
				.visibility = visibility,
				.buffer = {
					.type = wgpu::BufferBindingType::Uniform,
					.minBindingSize = binding["size"],
				},
			};
		else if (binding["kind"] == "descriptorTableSlot")
			return {
				.binding = index,
				.visibility = visibility,
				.texture = {
					.sampleType = wgpu::TextureSampleType::Float,
				},
			};
		else [[unlikely]] {
			panic("unexpected binding kind!");
			throw;
		}
	}

	template<ReflMapped T>
	inline static auto layout(const WgpuContext& context, simdjson::ondemand::value&& json)
		-> wgpu::BindGroupLayout {
		std::vector<wgpu::BindGroupLayoutEntry> entries;
		if (auto bindings = json["type"]["elementVarLayout"]["bindings"]; bindings.has_value()) {
			uint32_t index = 0;
			entries.reserve(bindings->count_elements());
			for (auto&& binding : bindings)
				entries.emplace_back(layout_entry(index++, std::move(*binding)));
		} else if (auto binding = json["type"]["elementVarLayout"]["binding"];
				   binding.has_value()) {
			entries.emplace_back(layout_entry(0, std::move(*binding)));
		} else [[unlikely]]
			panic(std::format("layout failed to parse!"));
		spdlog::info("layout size = {}", entries.size());
		for (const auto& entry : entries) spdlog::info("{}", (int)entry.binding);
		const wgpu::BindGroupLayoutDescriptor desc = {
			.entryCount = entries.size(),
			.entries	= entries.data(),
		};
		return context.device.CreateBindGroupLayout(&desc);
	}

	template<ReflMapped T>
	inline static auto layout(
		const WgpuContext& context, std::string_view name, const std::filesystem::path& path
	) -> wgpu::BindGroupLayout {
		auto parser = simdjson::ondemand::parser {};
		auto str	= simdjson::padded_string::load(path.string());
		auto json	= parser.iterate(str);
		for (auto&& param : json["parameters"])
			if (*param["name"]->get_string() == name)
				return layout<T>(context, std::move(*param));
		panic(std::format("no parameter named {}!", name));
		throw;
	}

	template<ReflMapped T>
	inline static auto layout(std::string_view name, const std::filesystem::path& path)
		-> wgpu::BindGroupLayout {
		return layout<T>(WgpuContext::global(), name, path);
	}

}  // namespace dvdbchar::Render