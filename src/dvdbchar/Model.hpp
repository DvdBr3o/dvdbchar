#pragma once

#include <glm/glm.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/util.hpp>
#include <fastgltf/tools.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <stdexcept>

namespace dvdbchar {
	template<typename T>
	concept ModelElement = requires(T t) { t.update(); };

	class Model {
	public:
		Model(const std::filesystem::path& path) {
			using namespace fastgltf;
			constexpr auto parser_opt = fastgltf::Extensions::KHR_mesh_quantization
									  | fastgltf::Extensions::KHR_texture_transform
									  | fastgltf::Extensions::KHR_materials_variants;
			constexpr auto asset_opt = fastgltf::Options::LoadExternalBuffers	//
									 | fastgltf::Options::LoadExternalImages	//
									 | fastgltf::Options::AllowDouble			//
									 | fastgltf::Options::GenerateMeshIndices;	//

			auto parser = Parser { parser_opt };
			auto file	= fastgltf::MappedGltfFile::FromPath(path);
			if (!file)
				throw std::runtime_error {
					std::format("Failed to read model file at `{}`.", path.string())
				};

			auto asset = parser.loadGltf(file.get(), path.parent_path(), asset_opt);
			if (asset)
				_asset = std::move(asset.get());
			else {
				throw std::runtime_error { std::format(
					"Failed to load model at `{}`: {}",
					path.string(),
					(int)asset.error()
				) };
			}
		}

	public:
		[[nodiscard]] auto&		  asset() { return _asset; }

		[[nodiscard]] const auto& asset() const { return _asset; }

	public:
		void introduce_self() const {
			spdlog::info("textures:");
			for (const auto& tex : _asset.textures) spdlog::info("{}", tex.name);
			spdlog::info("version: {}", _asset.assetInfo->gltfVersion);
			spdlog::info("mesh[{}]", _asset.meshes.size());
			spdlog::info("accessor[{}]", _asset.accessors.size());
			// for (const auto& acc : _asset.accessors) spdlog::info("acc: {}", acc.name);
			for (const auto& m : _asset.meshes) {
				for (const auto& p : m.primitives) {
					auto& acc = _asset.accessors[p.findAttribute("POSITION")->accessorIndex];
					fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
						_asset,
						acc,
						[](const fastgltf::math::fvec3& vec, size_t i) {
							// spdlog::info("{}, {}, {}", vec.x(), vec.y(), vec.z());
						}
					);
				}
			}
		}

	private:
		fastgltf::Asset _asset;
	};
}  // namespace dvdbchar