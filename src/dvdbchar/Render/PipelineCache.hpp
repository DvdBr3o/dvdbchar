#pragma once

#include "dvdbchar/Render/Context.hpp"
#include "dvdbchar/Render/Pipeline.hpp"
#include "dvdbchar/Utils.hpp"
#include "webgpu/webgpu_cpp.h"

#include <filesystem>

namespace dvdbchar::Render {
	struct AggregateShader {
		std::string		   source;
		std::string		   reflection;

		inline static auto from_path(std::filesystem::path& src, std::filesystem::path& refl) {
			AggregateShader shader;
			shader.source	  = *read_text_from(src);
			shader.reflection = *read_text_from(refl);
			return shader;
		}
	};

	using ManagedShaderId = size_t;

	struct ManagedShader {
		ManagedShaderId id;
		AggregateShader shader;

		using Id = ManagedShaderId;
	};

	class ShaderManager {
	public:
		inline static auto global() -> ShaderManager& {
			static ShaderManager manager;
			return manager;
		}

		auto shader(const AggregateShader& source) -> ManagedShader {
			return { _count++, source };
		};

		auto shader(std::string_view name, const AggregateShader& source) -> const ManagedShader& {
			if (auto it = _shaders.find(std::string { name }); it != _shaders.end())
				return it->second;

			auto [it, success] =
				_shaders.try_emplace(std::string { name }, ManagedShader { _count++, source });
			if (success)
				return it->second;
			else [[unlikely]] {
				panic(std::format("failed to create shader named `{}`", name));
				throw;
			}
		};

		[[nodiscard]] auto shader(std::string_view name) const -> std::optional<ManagedShader> {
			if (auto it = _shaders.find(std::string { name }); it != _shaders.end())
				return it->second;
			else
				return std::nullopt;
		}

	private:
		ShaderManager() = default;

	private:
		size_t										   _count = 0;
		std::unordered_map<std::string, ManagedShader> _shaders;
	};

	struct PipelineCacheKey {
		ManagedShader				 shader;
		wgpu::TextureFormat			 format;

		inline friend constexpr auto operator==(
			const PipelineCacheKey& lhs, const PipelineCacheKey& rhs
		) -> bool {
			return lhs.shader.id == rhs.shader.id && lhs.format == rhs.format;
		}
	};
}  // namespace dvdbchar::Render

template<>
struct std::hash<dvdbchar::Render::PipelineCacheKey> {
	auto operator()(const dvdbchar::Render::PipelineCacheKey& s) const -> size_t {
		using dvdbchar::hash_combine;

		size_t res = 0;

		hash_combine(res, s.shader.id);
		hash_combine(res, static_cast<uint32_t>(s.format));

		return res;
	}
};

namespace dvdbchar::Render {
	class PipelineCache {
	public:
		using Key = PipelineCacheKey;

		auto pipeline(const WgpuContext& ctx, const Key& key) -> const Pipeline& {
			if (auto it = _cache.find(key); it != _cache.end()) {
				return it->second;
			} else {
				auto [kv, success] = _cache.try_emplace(
					key,
					Pipeline {
						ctx,
						{
							.shader		= key.shader.shader.source,
							.reflection = key.shader.shader.reflection,
							.format		= key.format,
						  }
				  }
				);
				if (success)
					return kv->second;
				else [[unlikely]] {
					panic("failed to insert pipeline cache!");
					throw;
				}
			}
		}

		auto pipeline(const Key& key) -> const Pipeline& {
			return pipeline(WgpuContext::global(), key);
		}

	private:
		std::unordered_map<Key, Pipeline> _cache;
	};
}  // namespace dvdbchar::Render
