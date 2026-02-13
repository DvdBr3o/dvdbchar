#pragma once

#include "dvdbchar/Render/Context.hpp"
#include "dvdbchar/Utils.hpp"
#include "webgpu/webgpu_cpp.h"

#include <glm/glm.hpp>
#include <simdjson.h>
#include <spdlog/spdlog.h>
#include <slang.h>
#include <range/v3/all.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <ranges>
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
	concept FieldLike = Like<T, Field>;

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
	inline constexpr auto process_field(
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
	inline constexpr auto bind(T& t, simdjson::ondemand::value&& json) {
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
	inline constexpr auto get_mapping(std::string_view name, simdjson::ondemand::value&& json)
		-> ReflectedParameter<T> {
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
	inline constexpr auto get_mapping(simdjson::ondemand::value&& json) -> T {
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
	inline auto get_mapping(std::string_view name, const std::filesystem::path& path) {
		auto parser = simdjson::ondemand::parser {};
		auto str	= simdjson::padded_string::load(path.string());
		auto json	= parser.iterate(str);
		return get_mapping<T>(name, std::move(*json));
	}

	template<ParamMapped T>
	inline auto get_mapping(const std::filesystem::path& path) {
		auto parser = simdjson::ondemand::parser {};
		auto str	= simdjson::padded_string::load(path.string());
		auto json	= parser.iterate(str);
		return get_mapping<T>(*json);
	}

	namespace details::bindgroup_reflection {
		class BindgroupLayoutMap {
		public:
			struct SingleBindgroupLayoutBuilder {
				const WgpuContext&						ctx;
				BindgroupLayoutMap&						layout_builder;
				std::vector<wgpu::BindGroupLayoutEntry> entries;
				const nlohmann::json&					parameter;
				std::string								pre_field;

			private:
				auto _auto_introduced_uniform_buffer() {
					auto var_layout = parameter["type"]["elementVarLayout"];

					if (auto bindings = var_layout.find("bindings"); bindings != var_layout.end()) {
						spdlog::info(
							"yes bindings for field `{}`",
							(std::string_view)parameter["name"]
						);
						for (auto&& binding : *bindings)
							if (binding["kind"] == "uniform") {
								entries.emplace_back(wgpu::BindGroupLayoutEntry{
								.binding = 0,
								.visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment, 
								.buffer = {
									.type = wgpu::BufferBindingType::Uniform,
									.minBindingSize = binding["size"],
								},
							});
							}
					} else if (auto binding = var_layout.find("binding");
							   binding != var_layout.end()) {
						spdlog::info(
							"yes binding for field `{}`",
							(std::string_view)parameter["name"]
						);
						if ((*binding)["kind"] == "uniform") {
							entries.emplace_back(wgpu::BindGroupLayoutEntry{
								.binding = 0,
								.visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment,
								.buffer = {
									.type = wgpu::BufferBindingType::Uniform,
									.minBindingSize = (*binding)["size"],
								},
							});
						}
					} else [[unlikely]] {
						panic(
							std::format(
								"no binding for field `{}`",
								(std::string_view)parameter["name"]
							)
						);
					}
				}

				auto _descriptor_table_slots() {
					for (auto&& field : parameter["type"]["elementVarLayout"]["type"]["fields"]) {
						if (field["type"]["kind"] == "resource") {
							wgpu::TextureViewDimension view_dimension;
							if (field["type"]["baseShape"] == "texture1D")
								view_dimension = wgpu::TextureViewDimension::e1D;
							else if (field["type"]["baseShape"] == "texture2D")
								view_dimension = wgpu::TextureViewDimension::e2D;
							else if (field["type"]["baseShape"] == "texture3D")
								view_dimension = wgpu::TextureViewDimension::e3D;
							else [[unlikely]]
								panic("unexpected texture view dimension!");

							entries.emplace_back(wgpu::BindGroupLayoutEntry{
								.binding = static_cast<uint32_t>(entries.size()),
								.visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment,
								.texture = {
									.sampleType = wgpu::TextureSampleType::Float,
									.viewDimension = view_dimension,
								},
							});
						} else if (field["type"]["kind"] == "samplerState") {
							entries.emplace_back(wgpu::BindGroupLayoutEntry{
								.binding = static_cast<uint32_t>(entries.size()),
								.visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment,
								.sampler = {
									.type = wgpu::SamplerBindingType::Filtering,
								},
							});
						}
					}
				}

				auto _nested_parameter_blocks() {
					for (auto&& field : parameter["type"]["elementVarLayout"]["type"]["fields"]) {
						if (field["type"]["kind"] == "parameterBlock") {
							spdlog::info(
								"nested parameter block found: {}",
								_cat_field(
									(std::string_view)parameter["name"],
									(std::string_view)field["name"]
								)
							);
							layout_builder._build_single_bindgroup_layout(
								ctx,
								field,
								_cat_field(pre_field, (std::string_view)parameter["name"])
							);
						}
					}
				}

				inline static auto _cat_field(std::string_view former, std::string_view latter)
					-> std::string {
					if (former.empty())
						return std::string { latter };
					else
						return std::format("{}.{}", former, latter);
				}

			public:
				auto build() {
					_auto_introduced_uniform_buffer();
					_descriptor_table_slots();

					const wgpu::BindGroupLayoutDescriptor desc {
						.entryCount = entries.size(),
						.entries	= entries.data(),
					};
					layout_builder._layout_map.insert_or_assign(
						_cat_field(pre_field, parameter["name"].get<std::string_view>()),
						layout_builder._layouts.size()
					);
					layout_builder._layouts.emplace_back(ctx.device.CreateBindGroupLayout(&desc));

					_nested_parameter_blocks();
				}
			};

		public:
			BindgroupLayoutMap(const WgpuContext& ctx, const nlohmann::json& json) {
				_build(ctx, json);
			}

			BindgroupLayoutMap(const nlohmann::json& json) :
				BindgroupLayoutMap(WgpuContext::global(), json) {}

		public:
			auto& operator[](const std::string& field) {
				spdlog::info("map[\"{}\"] = {}", field, _layout_map[field]);
				return _layouts[_layout_map[field]];
			}

			[[nodiscard]] auto all() const& -> std::span<const wgpu::BindGroupLayout> {
				return { _layouts };
			}

			[[nodiscard]] auto all() && -> std::vector<wgpu::BindGroupLayout> {
				return std::move(_layouts);
			}

		private:
			void _build_single_bindgroup_layout(
				const WgpuContext& ctx, const nlohmann::json& parameter, std::string_view pre = ""
			) {
				SingleBindgroupLayoutBuilder {
					.ctx			= ctx,
					.layout_builder = *this,
					.parameter		= parameter,
					.pre_field		= std::string { pre },
				}
					.build();
			}

			void _build(const WgpuContext& ctx, const nlohmann::json& root) {
				for (auto&& parameter : root["parameters"])
					_build_single_bindgroup_layout(ctx, parameter);
			}

		private:
			std::vector<wgpu::BindGroupLayout>		_layouts;
			std::unordered_map<std::string, size_t> _layout_map;
		};
	}  // namespace details::bindgroup_reflection

	using details::bindgroup_reflection::BindgroupLayoutMap;

	inline auto bindgroup_layout(const WgpuContext& ctx, simdjson::ondemand::value&& json) {}

	inline auto bindgroup_layouts(const WgpuContext& ctx, const nlohmann::json& json)
		-> BindgroupLayoutMap {
		return { ctx, json };
	}

	inline auto bindgroup_layouts(const nlohmann::json& json) -> BindgroupLayoutMap {
		return bindgroup_layouts(WgpuContext::global(), json);
	}

	inline auto bindgroup_layouts_from_string(const WgpuContext& ctx, std::string_view s)
		-> BindgroupLayoutMap {
		return bindgroup_layouts(ctx, nlohmann::json::parse(s));
	}

	inline auto bindgroup_layouts_from_string(std::string_view s) -> BindgroupLayoutMap {
		return bindgroup_layouts_from_string(WgpuContext::global(), s);
	}

	inline auto bindgroup_layouts_from_path(
		const WgpuContext& ctx, const std::filesystem::path& path
	) -> BindgroupLayoutMap {
		std::ifstream in { path };
		if (!in.is_open())
			throw std::runtime_error {
				std::format("failed to open json at path `{}`", path.string())
			};
		nlohmann::json json;
		in >> json;
		return bindgroup_layouts(ctx, json);
	}

	inline auto bindgroup_layouts_from_path(const std::filesystem::path& path)
		-> BindgroupLayoutMap {
		return bindgroup_layouts_from_path(WgpuContext::global(), path);
	}

}  // namespace dvdbchar::Render