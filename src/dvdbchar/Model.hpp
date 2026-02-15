#pragma once

#include "Render/Buffer.hpp"
#include "dvdbchar/Render/Mesh.hpp"
#include "dvdbchar/Render/Pipeline.hpp"
#include "dvdbchar/Render/Primitives.hpp"
#include "dvdbchar/Render/ShaderReflection.hpp"
#include "dvdbchar/Render/Texture.hpp"
#include "dvdbchar/Stb.hpp"
#include "fastgltf/types.hpp"

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

		[[nodiscard]] auto _test_prmitive() const {
			return primitive(_asset.meshes[0].primitives[0]);
		}

		[[nodiscard]] auto primitive(const fastgltf::Primitive& primitive) const
			-> Render::MeshPrimitive {
			Render::MeshPrimitive out_primitive;

			// material
			if (primitive.materialIndex.has_value()) {
				auto& material = _asset.materials[primitive.materialIndex.value()];

				if (material.pbrData.baseColorTexture.has_value()) {
					auto  texIndex = material.pbrData.baseColorTexture->textureIndex;
					auto& texture  = _asset.textures[texIndex];
					if (texture.imageIndex.has_value()) {
						auto& image				 = _asset.images[texture.imageIndex.value()];
						out_primitive.tex_albedo = _texture_from_image(image);
					} else [[unlikely]] {
						panic("texture does not have imageIndex!");
					}
				}
			}

			{	// pbr bg
				// clang-format off
				out_primitive.bg_pbr = Render::Bindgroup { Render::Bindgroup::Spec {
					.layout = Render::parsed::bindgroup_layout_from_path("pbr", "shaders/Uniform.layout.json"),
					.entries = std::array { 
						wgpu::BindGroupEntry {
							.binding = 0,
							.textureView = out_primitive.tex_albedo.CreateView(),
						},
						wgpu::BindGroupEntry {
							.binding = 1,
							// .sampler = Render::linear_repeat_sampler(),
							.sampler = Render::isotropic_sampler(wgpu::AddressMode::Repeat, wgpu::FilterMode::Nearest),
						},
					},
				} };
				// clang-format on
			}

			{  // Vertices
				std::vector<Render::Vertice> vertices;

				if (auto it = primitive.findAttribute("POSITION");
					it != primitive.attributes.end()) {
					auto& accessor = _asset.accessors[it->accessorIndex];
					vertices.resize(accessor.count);

					fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
						_asset,
						_asset.accessors[it->accessorIndex],
						[&](fastgltf::math::fvec3 v, size_t idx) {
							vertices[idx].pos[0] = v.x();
							vertices[idx].pos[1] = v.y();
							vertices[idx].pos[2] = v.z();
						}
					);
				} else [[unlikely]] {
					panic("wtf this gltf has no attribute `POSITION`?");
				}

				if (auto it = primitive.findAttribute("TEXCOORD_0");
					it != primitive.attributes.end()) {
					fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
						_asset,
						_asset.accessors[it->accessorIndex],
						[&](fastgltf::math::fvec2 v, size_t idx) {
							vertices[idx].uv[0] = v.x();
							vertices[idx].uv[1] = v.y();
						}
					);
				} else [[unlikely]] {
					panic("wtf this gltf has no attribute `TEXCOORD_0`?");
				}

				out_primitive.buf_vertex =
					Render::array_vertex_buffer<Render::Vertice>(std::move(vertices));
			}

			{  // Indices
				auto& index_accessor = _asset.accessors[*primitive.indicesAccessor];
				std::vector<std::size_t> indices;
				if (primitive.indicesAccessor.has_value()) {
					switch (index_accessor.componentType) {
						case fastgltf::ComponentType::UnsignedByte:
							// indices.resize(index_accessor.count * sizeof(std::byte));
							// fastgltf::copyFromAccessor<std::uint8_t>(
							// 	_asset,
							// 	index_accessor,
							// 	indices.data()
							// );
							panic("wgpu does not support uint8_t index yet!");
							break;
						case fastgltf::ComponentType::UnsignedShort:
							indices.resize(index_accessor.count);
							fastgltf::copyFromAccessor<std::uint16_t>(
								_asset,
								index_accessor,
								indices.data()
							);
							out_primitive.buf_index_format = wgpu::IndexFormat::Uint16;
							break;
						case fastgltf::ComponentType::UnsignedInt:
							indices.resize(index_accessor.count);
							fastgltf::copyFromAccessor<std::uint32_t>(
								_asset,
								index_accessor,
								indices.data()
							);
							out_primitive.buf_index_format = wgpu::IndexFormat::Uint32;
							break;
						default: panic("unexpected index type!"); break;
					}
					out_primitive.buf_index		  = Render::array_index_buffer(to_span(indices));
					out_primitive.buf_index_count = indices.size();
				} else [[unlikely]] {
					panic("wtf this gltf has no indice accessor?");
				}
			}

			return out_primitive;
		}

	private:
		[[nodiscard]] auto _texture_from_image(const fastgltf::Image& image) const
			-> wgpu::Texture {
			return _texture_from_image(Render::WgpuContext::global(), image);
		}

		[[nodiscard]] auto _texture_from_image(
			const Render::WgpuContext& ctx, const fastgltf::Image& image
		) const -> wgpu::Texture {
			wgpu::Texture texture;
			// spdlog::info("image name: {}", image.name);
			// spdlog::info("image data type: {}", image.data.index());
			std::visit(
				fastgltf::visitor {
					[&](const fastgltf::sources::URI& filePath) {
						assert(
							filePath.fileByteOffset == 0
						);	// We don't support offsets with stbi.
						assert(
							filePath.uri.isLocalPath()
						);	// We're only capable of loading local files.
						int				  width, height, nrChannels;

						const std::string path(
							filePath.uri.path().begin(),
							filePath.uri.path().end()
						);	// Thanks C++.
						unsigned char* data =
							stbi_load(path.c_str(), &width, &height, &nrChannels, 4);

						texture = texture_from_image(
							ctx,
							Render::ImageInfo {
								{ reinterpret_cast<char*>(data),
								  std::strlen(reinterpret_cast<char*>(data)) },
								static_cast<uint32_t>(width),
								static_cast<uint32_t>(height),
						}
						);

						stbi_image_free(data);
					},
					[&](const fastgltf::sources::Array& vector) {
						int			   width, height, nrChannels;
						unsigned char* data = stbi_load_from_memory(
							reinterpret_cast<const stbi_uc*>(vector.bytes.data()),
							static_cast<int>(vector.bytes.size()),
							&width,
							&height,
							&nrChannels,
							4
						);
						texture = texture_from_image(
							ctx,
							Render::ImageInfo {
								{ reinterpret_cast<char*>(data),
								  std::strlen(reinterpret_cast<char*>(data)) },
								static_cast<uint32_t>(width),
								static_cast<uint32_t>(height),
						}
						);
						stbi_image_free(data);
					},
					[&](const fastgltf::sources::BufferView& view) {
						auto& bufferView = _asset.bufferViews[view.bufferViewIndex];
						auto& buffer	 = _asset.buffers[bufferView.bufferIndex];
						// Yes, we've already loaded every buffer into some GL buffer. However, with
						// GL it's simpler to just copy the buffer data again for the texture.
						// Besides, this is just an example.
						std::visit(
							fastgltf::visitor {
								// We only care about VectorWithMime here, because we specify
								// LoadExternalBuffers, meaning
								// all buffers are already loaded into a vector.
								[&](const fastgltf::sources::Array& vector) {
									int			   width, height, nrChannels;
									unsigned char* data = stbi_load_from_memory(
										reinterpret_cast<const stbi_uc*>(
											vector.bytes.data() + bufferView.byteOffset
										),
										static_cast<int>(bufferView.byteLength),
										&width,
										&height,
										&nrChannels,
										4
									);
									// spdlog::info(
									// 	"image info: {}, {}, {}",
									// 	width,
									// 	height,
									// 	nrChannels
									// );
									texture = texture_from_image(
										ctx,
										Render::ImageInfo {
											.data	= { reinterpret_cast<char*>(data),
														//   std::strlen(reinterpret_cast<char*>(data))
														static_cast<size_t>(width * height * 4) },
											.width	= static_cast<uint32_t>(width),
											.height = static_cast<uint32_t>(height),
									}
									);
									stbi_image_free(data);
								},
								[](auto&& arg) { panic("unexpected image buffer view!"); },
							},
							buffer.data
						);
					},
					[](auto&& arg) { panic("unexpected image type!"); },
				},
				image.data
			);
			return texture;
		}

	private:
		fastgltf::Asset _asset;
	};
}  // namespace dvdbchar