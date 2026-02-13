#pragma once

#include "dvdbchar/Render/Context.hpp"
#include "dvdbchar/Render/Pipeline.hpp"
#include "dvdbchar/Render/PipelineCache.hpp"
#include "dvdbchar/Utils.hpp"

#include <webgpu/webgpu_cpp.h>

#include <functional>
#include <type_traits>

namespace dvdbchar::Render {
	enum class ResourceLifetime {
		Managed,
		Imported,
	};

	using ResourceId = size_t;

	template<typename DescT, typename InnerT>
	struct ResourceRef {
		ResourceId id;
		DescT	   desc;

		using Desc	= DescT;
		using Inner = InnerT;
	};

	template<Like<ResourceRef> ResT, typename StrategyT>
	class SpecificResourceManager {
	public:
	private:
		std::vector<typename ResT::Inner> _pool;
	};

	using PersistentBufferRef  = ResourceRef<decltype(std::ignore), const wgpu::Buffer*>;
	using PersistentTextureRef = ResourceRef<decltype(std::ignore), const wgpu::Texture*>;
	using BufferRef			   = ResourceRef<wgpu::BufferDescriptor, wgpu::Buffer>;
	using TextureRef		   = ResourceRef<wgpu::TextureDescriptor, wgpu::Texture>;

	class TransientTextureManager {
	public:
		struct TextureStrategy {
			size_t		  tex_id;
			wgpu::LoadOp  load	= wgpu::LoadOp::Clear;
			wgpu::StoreOp store = wgpu::StoreOp::Store;
		};

	public:
		TransientTextureManager(
			std::vector<TextureStrategy>&& strategies, std::vector<wgpu::Texture>&& textures
		) :
			_strategies(strategies), _textures(textures) {}

	public:
		auto at(const TextureRef& ref) { return _strategies.at(ref.id); }

	private:
		std::vector<TextureStrategy> _strategies;
		std::vector<wgpu::Texture>	 _textures;
	};

	class TransientBufferManager {
	public:
	private:
		std::vector<wgpu::Buffer> _buffers;
	};

	template<Like<ResourceRef> ResT, typename SlotT>
		requires requires(SlotT slot) {
			{ slot.reflect() } -> Like<std::tuple>;
		}
	class SlotManager {
	public:
		using InnerT = typename ResT::Inner;
		static_assert(std::is_default_constructible_v<InnerT>);

	public:
		constexpr SlotManager(SlotT&& slot) {
			// clang-format off
			std::apply(
				[&](Like<ResourceRef> auto&&... args) {
					size_t size = 0;
					([&](){
						if constexpr (std::convertible_to<std::remove_cvref_t<decltype(args)>, ResT>) {
							++size;
						}
					}(), ...);
					_slots.resize(size);
				},
				slot.reflect()
			);
			// clang-format on
		}

	public:
		auto			   set(ResT ref, const InnerT& handle) { _slots[ref.id] = handle; }

		[[nodiscard]] auto get(ResT ref) const -> const InnerT& { return _slots.at(ref.id); }

	private:
		std::vector<InnerT> _slots;
	};

	template<typename SlotT>
	using BufferSlotManager = SlotManager<PersistentBufferRef, SlotT>;
	template<typename SlotT>
	using TextureSlotManager = SlotManager<PersistentTextureRef, SlotT>;

	template<Like<ResourceRef> RefT>
	class CountBuilder {
	public:
		template<typename... Args>
		[[nodiscard]] constexpr auto create(Args&&... args) -> RefT {
			return { count++, std::forward<Args>(args)... };
		}

		template<typename... Args>
		[[nodiscard]] constexpr auto create_with_id(size_t id, Args&&... args) const -> RefT {
			return { id, std::forward<Args>(args)... };
		}

	private:
		size_t count = 0;
	};

	struct EmptySlot {};

	using RenderGraphExecutionState = std::vector<size_t>;
	using RenderGraphExecutionPlan	= std::vector<RenderGraphExecutionState>;

	template<typename SlotT, typename... PassTs>
	class RuntimeRenderGraph {
	public:
		using Slot = std::remove_cvref_t<SlotT>;

	public:
		RuntimeRenderGraph(SlotT&& slot, std::tuple<PassTs...>&& passes) :
			_slot(std::move(slot)),
			_buffer_slots(slot),
			_texture_slots(slot),
			_passes(std::move(passes)) {}

	public:
		auto set_buffer_slot(const PersistentBufferRef& ref, const wgpu::Buffer& buffer) {
			_buffer_slots.set(ref, &buffer);
		}

		[[nodiscard]] auto buffer_slot(const PersistentBufferRef& ref) const {
			return _buffer_slots.get(ref);
		}

		auto set_texture_slot(const PersistentTextureRef& ref, const wgpu::Texture& texture) {
			_texture_slots.set(ref, &texture);
		}

		[[nodiscard]] auto texture_slot(const PersistentTextureRef& ref) const {
			return _texture_slots.get(ref);
		}

		[[nodiscard]] auto slot() const -> const Slot& { return _slot; }

		template<typename... Args>
		auto pipeline(Args&&... args) {
			return _pipelines.pipeline(std::forward<Args>(args)...);
		}

		//
		void execute(const WgpuContext& ctx) const {
			for (const auto& state : _plan) {
				std::vector<wgpu::CommandBuffer> cmds;
				cmds.reserve(state.size());
				for (const auto& pass : state) {
					auto cmd = ctx.device.CreateCommandEncoder();
					std::get<pass>(_passes).execute(cmd);
					cmds.emplace_back(cmd.Finish());
				}
				ctx.queue.Submit(cmds.size(), cmds.data());
			}
		}

		void execute() const { return execute(WgpuContext::global()); }

	private:
		Slot					  _slot;
		BufferSlotManager<SlotT>  _buffer_slots;
		TextureSlotManager<SlotT> _texture_slots;
		std::tuple<PassTs...>	  _passes;
		RenderGraphExecutionPlan  _plan;
		PipelineCache			  _pipelines;
	};

	class RenderGraphBuilder {
	public:
		auto buffer(wgpu::BufferDescriptor&& desc) { return _buf_builder.create(desc); }

		auto texture(wgpu::TextureDescriptor&& desc) { return _tex_builder.create(desc); }

		auto present_texture() { return _tex_builder.create_with_id(static_cast<size_t>(-1)); }

		auto import_buffer() { return _import_buf_builder.create(std::ignore); }

		auto import_texture() { return _import_tex_builder.create(std::ignore); }

		template<typename S, typename... Ps>
		auto build_runtime(S&& slots, std::tuple<Ps...>&& passes) const
			-> RuntimeRenderGraph<S, Ps...> {
			std::array<size_t, sizeof...(Ps)> topo {};

			std::apply(
				[&](auto&&... pass) {
					(
						[&]() {
							if constexpr (std::remove_cvref_t<
											  decltype(pass.reflect())>::Input::size()
										  == 0) {
								// start topo
							}
						}(),
						...
					);
				},
				passes
			);

			return { std::forward<S>(slots), std::move(passes) };
		}

	private:
		CountBuilder<BufferRef>			   _buf_builder;
		CountBuilder<TextureRef>		   _tex_builder;
		CountBuilder<PersistentBufferRef>  _import_buf_builder;
		CountBuilder<PersistentTextureRef> _import_tex_builder;
	};

	template<typename P, typename Mem>
	struct PassInput {
		Mem P::* mem;
	};

	template<typename P, typename Mem>
	struct PassOutput {
		Mem P::* mem;
	};

	template<Like<PassInput>... Ins>
	struct PassInputs : public std::tuple<Ins...> {
		using std::tuple<Ins...>::tuple;

		inline static consteval auto size() -> size_t { return sizeof...(Ins); }
	};

	using NoPassInputs = PassInputs<>;

	template<typename... Ps, typename... Mems>
	PassInputs(Mems Ps::*&&...) -> PassInputs<PassInput<Ps, Mems>...>;

	template<Like<PassOutput>... Outs>
	struct PassOutputs : public std::tuple<Outs...> {
		using std::tuple<Outs...>::tuple;

		inline static consteval auto size() -> size_t { return sizeof...(Outs); }
	};

	template<typename... Ps, typename... Mems>
	PassOutputs(Mems Ps::*&&...) -> PassOutputs<PassOutput<Ps, Mems>...>;

	using NoPassOutputs = PassOutputs<>;

	template<Like<PassInputs> InT, Like<PassOutputs> OutT>
	struct PassSlot {
		InT	 input;
		OutT output;

		using Input	 = InT;
		using Output = OutT;
	};

	struct BasePass {
		PersistentBufferRef vb;
		PersistentBufferRef ib;
		TextureRef			target;

		//
		static constexpr auto reflect() -> Like<PassSlot> auto {
			// clang-format off
			return PassSlot {
				// .input = PassInputs {
				// 	// &BasePass::vb,
				// 	// &BasePass::ib,
				// },
				.input = NoPassInputs {},
				.output = PassOutputs {
					&BasePass::target,
				},
			};
			// clang-format on
		}

		//
		auto execute(Like<RuntimeRenderGraph> auto&& graph, const wgpu::CommandEncoder& cmd) const {
			auto buf_vb		= graph.buffer_slot(vb);
			auto buf_ib		= graph.buffer_slot(ib);
			auto tex_target = graph.texture(target);

			auto& pipeline = graph.pipeline({
				.shader = ShaderManager::global().shader(
					"Pipeline", {
						.source 	= *read_text_from("shaders/Pipeline.wgsl"),
						.reflection = *read_text_from("shaders/Uniform.refl.json"),
					}
				),
				// .format = window.format(),
			});

			//
			const wgpu::RenderPassColorAttachment color_attachment {
				.view = tex_target.CreateView(),
			};
			const wgpu::RenderPassDescriptor desc {
				.colorAttachmentCount = 1,
				.colorAttachments	  = &color_attachment,
			};
			auto pass = cmd.BeginRenderPass(&desc);
			pass.SetVertexBuffer(0, buf_vb);
			pass.SetIndexBuffer(buf_ib, wgpu::IndexFormat::Uint32);
			pass.SetPipeline(pipeline);
		}
	};

	inline auto _test_render_graph() {
		auto builder = RenderGraphBuilder {};

		struct Slot {
			PersistentBufferRef			 vb;
			PersistentBufferRef			 ib;

			[[nodiscard]] constexpr auto reflect() const { return std::tuple { vb, ib }; }
		};

		Slot slot {
			.vb = builder.import_buffer(),
			.ib = builder.import_buffer(),
		};

		// clang-format off
		auto graph = builder.build_runtime(
			slot,
			std::tuple {
				BasePass {
					.vb 	= slot.vb,
					.ib 	= slot.ib,
					.target = builder.present_texture(),
				},
			}
		);
		// clang-format on
	}
}  // namespace dvdbchar::Render