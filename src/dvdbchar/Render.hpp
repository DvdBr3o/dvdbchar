#pragma once

#include "dvdbchar/Utils.hpp"
#include "dvdbchar/Glfw.hpp"
#include "webgpu/webgpu_cpp.h"

#include <dawn/webgpu_cpp.h>
#include <webgpu/webgpu_glfw.h>
#include <stdexec/execution.hpp>
#include <exec/async_scope.hpp>
#include <exec/env.hpp>
#include <exec/static_thread_pool.hpp>
#include <spdlog/spdlog.h>
#include <glm/glm.hpp>
#include <simdjson.h>
#include <slang.h>
#include <slang-com-helper.h>
#include <slang-com-ptr.h>

#include <stdexcept>
#include <functional>
#include <exception>
#include <filesystem>
#include <type_traits>
#include <utility>
#include <span>
#include <vector>

namespace dvdbchar::Render {
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

	class EnchancedDevice : public wgpu::Device {
	public:
	};

	struct WgpuContext {
		wgpu::Instance instance;
		wgpu::Adapter  adapter;
		wgpu::Device   device;
		wgpu::Queue	   queue;

		WgpuContext() noexcept					 = default;
		WgpuContext(WgpuContext&&) noexcept		 = default;
		WgpuContext(const WgpuContext&) noexcept = default;

		struct Spec {
			wgpu::InstanceDescriptor	instance_desc = { 
				.capabilities = {
                	.timedWaitAnyEnable = true,
            	}, 
			};
			wgpu::RequestAdapterOptions adapter_opts;
			wgpu::DeviceDescriptor		device_desc = []() {
				 wgpu::DeviceDescriptor desc;
				 desc.SetUncapturedErrorCallback(
					 [](const wgpu::Device&, wgpu::ErrorType errorType, wgpu::StringView message) {
						 spdlog::error("[WGPU/{}]: {}", (int)errorType, std::string_view(message));
					 }
				 );
				 return desc;
			}();
		};

		inline static auto create(const Spec& spec) {
			WgpuContext ctx;

			ctx.instance = wgpu::CreateInstance(&spec.instance_desc);
			ctx.instance.WaitAny(
				ctx.instance.RequestAdapter(
					&spec.adapter_opts,
					wgpu::CallbackMode::WaitAnyOnly,
					[&](wgpu::RequestAdapterStatus status,
						wgpu::Adapter			   a,
						wgpu::StringView		   message) {
						if (status == wgpu::RequestAdapterStatus::Success) {
							ctx.adapter = std::move(a);
						} else {
							spdlog::critical(std::string_view(message));
							throw std::runtime_error { std::string(message) };
						}
					}
				),
				std::numeric_limits<uint32_t>::max()
			);
			ctx.instance.WaitAny(
				ctx.adapter.RequestDevice(
					&spec.device_desc,
					wgpu::CallbackMode::WaitAnyOnly,
					[&](wgpu::RequestDeviceStatus status,
						wgpu::Device			  d,
						wgpu::StringView		  message) {
						if (status == wgpu::RequestDeviceStatus::Success) {
							ctx.device = std::move(d);
						} else {
							spdlog::critical(std::string_view(message));
							throw std::runtime_error { std::string(message) };
						}
					}
				),
				std::numeric_limits<uint32_t>::max()
			);
			ctx.device.SetLoggingCallback([](wgpu::LoggingType type, wgpu::StringView message) {
				switch (type) {
					case wgpu::LoggingType::Verbose:
						spdlog::trace(std::string_view(message));
						break;
					case wgpu::LoggingType::Info: spdlog::info(std::string_view(message)); break;
					case wgpu::LoggingType::Warning: spdlog::warn(std::string_view(message)); break;
					case wgpu::LoggingType::Error:
						spdlog::critical(std::string_view(message));
						break;
				}
			});

			ctx.queue = ctx.device.GetQueue();

			return ctx;
		}

		inline static auto create() { return create(Spec {}); }

		inline friend auto operator==(const WgpuContext& lhs, const WgpuContext& rhs) -> bool {
			return lhs.instance.Get() == rhs.instance.Get()	 //
				&& lhs.device.Get() == rhs.device.Get()		 //
				&& lhs.queue.Get() == rhs.queue.Get()		 //
				;
		}

		struct query_instance_t : Query<query_instance_t> {
			using Query<query_instance_t>::operator();

			template<class Env>
			constexpr auto operator()(const Env& e) const noexcept {
				return e.query(*this);
			}

			[[nodiscard]]
			constexpr auto query(stdexec::forwarding_query_t) const noexcept -> bool {
				return true;
			}
		};

		struct query_device_t : Query<query_device_t> {
			using Query<query_device_t>::operator();

			template<queryable_with<query_device_t> Env>
			constexpr auto operator()(const Env& e) const noexcept {
				return e.query(*this);
			}

			inline static consteval auto query(stdexec::forwarding_query_t) noexcept -> bool {
				return true;
			}
		};

		struct query_adapter_t : Query<query_adapter_t> {
			using Query<query_adapter_t>::operator();

			template<queryable_with<query_adapter_t> Env>
			constexpr auto operator()(const Env& e) const noexcept {
				return e.query(*this);
			}

			inline static consteval auto query(stdexec::forwarding_query_t) noexcept -> bool {
				return true;
			}
		};

		struct query_queue_t : Query<query_queue_t> {
			using Query<query_queue_t>::operator();

			template<class Env>
			constexpr auto operator()(const Env& e) const noexcept {
				return e.query(*this);
			}

			[[nodiscard]]
			constexpr auto query(stdexec::forwarding_query_t) const noexcept -> bool {
				return true;
			}
		};

		// static_assert(stdexec::__env::prop)

		inline static constexpr query_instance_t query_instance;
		inline static constexpr query_device_t	 query_device;
		inline static constexpr query_adapter_t	 query_adapter;
		inline static constexpr query_queue_t	 query_queue;

		template<stdexec::scheduler Sch>
		struct AdaptedScheduler {
			Sch										  sched;
			std::reference_wrapper<const WgpuContext> ctx;

			struct Sender {
				using sender_concept = stdexec::sender_t;
				using completion_signatures =
					stdexec::completion_signatures<stdexec::set_value_t()>;

				std::reference_wrapper<const AdaptedScheduler> self;

				template<class CPO>
				auto query(stdexec::get_completion_scheduler_t<CPO>, auto&&...) const noexcept {
					return self.get().sched;
				}

				template<stdexec::receiver R>
				auto connect(R&& r) const noexcept {
					return stdexec::connect(
						stdexec::schedule(self.get().sched),
						std::forward<R>(r)
					);
				}
			};

			[[nodiscard]] auto query(query_instance_t) const noexcept {
				return std::ref(ctx.get().instance);
			}

			[[nodiscard]] auto query(query_device_t) const noexcept {
				return std::ref(ctx.get().device);
			}

			[[nodiscard]] auto query(query_adapter_t) const noexcept {
				return std::ref(ctx.get().adapter);
			}

			[[nodiscard]] auto query(query_queue_t) const noexcept {
				return std::ref(ctx.get().queue);
			}

			inline friend auto operator==(const AdaptedScheduler& lhs, const AdaptedScheduler& rhs)
				-> bool {
				return std::addressof(lhs.ctx) == std::addressof(rhs.ctx) && lhs.sched == rhs.sched;
			}

			[[nodiscard]] auto schedule() const noexcept { return Sender { *this }; }
		};

		struct LaunchSender {
			const wgpu::Instance&		instance;
			stdexec::inplace_stop_token st;

			using sender_concept = stdexec::sender_t;
			using completion_signatures =
				stdexec::completion_signatures<stdexec::set_value_t(), stdexec::set_stopped_t()>;

			template<stdexec::receiver R>
			struct Opstate {
				R							r;
				const wgpu::Instance&		instance;
				stdexec::inplace_stop_token st;

				//
				auto start() noexcept {
					using namespace stdexec;

					while (!st.stop_requested()) instance.ProcessEvents();

					if (st.stop_requested())
						set_stopped(r);
					else
						set_value(r);
				}
			};

			template<stdexec::receiver R>
			constexpr auto connect(R&& r) && noexcept -> Opstate<R> {
				return { std::forward<R>(r), instance, std::move(st) };
			}

			template<stdexec::receiver R>
			constexpr auto connect(R&& r) & noexcept -> Opstate<R> {
				return { std::forward<R>(r), instance, st };
			}

			template<stdexec::receiver R>
			constexpr auto connect(R&& r) const& noexcept -> Opstate<R> {
				return { std::forward<R>(r), instance, st };
			}
		};

		[[nodiscard]] auto get_scheduler() const {
			return AdaptedScheduler { stdexec::inline_scheduler {}, *this };
		}

		template<stdexec::scheduler Sch>
		[[nodiscard]] auto get_scheduler_from(Sch&& sch) const -> AdaptedScheduler<Sch> {
			return { sch, *this };
		}

		[[nodiscard]] auto launch(const stdexec::inplace_stop_token& st) const -> LaunchSender {
			return { instance, st };
		}
	};

	struct Shader {
		struct Struct {};

		class SlangGlobalSession {
		public:
			inline static auto get() -> Slang::ComPtr<slang::IGlobalSession>& {
				static SlangGlobalSession instance;
				return instance._global;
			}

		private:
			SlangGlobalSession() { slang::createGlobalSession(_global.writeRef()); }

			SlangGlobalSession(const SlangGlobalSessionDesc& desc) {
				slang::createGlobalSession(&desc, _global.writeRef());
			}

		private:
			Slang::ComPtr<slang::IGlobalSession> _global;
		};

		struct SlangSessionSpec {
			std::vector<slang::TargetDesc>			  targets;
			SlangMatrixLayoutMode					  matrix_layout;
			std::vector<const char*>				  search_paths;
			std::vector<slang::PreprocessorMacroDesc> macros;
			bool									  glsl_syntax;

			//
			[[nodiscard]] auto to_desc() const -> slang::SessionDesc {
				return {
					.targets				 = targets.data(),
					.targetCount			 = static_cast<SlangInt>(targets.size()),
					.defaultMatrixLayoutMode = matrix_layout,
					.searchPaths			 = search_paths.data(),
					.searchPathCount		 = static_cast<SlangInt>(search_paths.size()),
					.preprocessorMacros		 = macros.data(),
					.preprocessorMacroCount	 = static_cast<SlangInt>(macros.size()),
					.allowGLSLSyntax		 = glsl_syntax,
				};
			}
		};

		struct SlangReflection {
		public:
			struct Spec {
				const std::filesystem::path& mod;
				const slang::SessionDesc&	 session_desc;
			};

		public:
			SlangReflection(const Spec& spec) {
				SlangGlobalSession::get()->createSession(spec.session_desc, _session.writeRef());
				Slang::ComPtr<slang::IBlob> diagnostics;
				_module = _session->loadModule(spec.mod.string().data(), diagnostics.writeRef());
				if (!_module) {
					spdlog::error((const char*)diagnostics->getBufferPointer());
					throw std::runtime_error { (const char*)diagnostics->getBufferPointer() };
				}
			}

		private:
			Slang::ComPtr<slang::ISession> _session;
			Slang::ComPtr<slang::IModule>  _module;
		};

		struct Parameter {
			std::string name;
			int64_t		binding;
		};

		struct EntryPoint {
			std::string name;
		};

		struct JsonReflection {
			JsonReflection(const std::filesystem::path& path) {
				simdjson::ondemand::parser parser;
				const auto				   json_str = simdjson::padded_string::load(path.string());
				auto					   json		= *parser.iterate(json_str);

				parameters.reserve(json["parameters"]->get_array()->count_elements());
				for (auto&& param : json["parameters"]) {
					spdlog::info("name: {}", *param["name"].get_string());
					parameters.emplace_back(
						Parameter {
							.name	 = std::string { *param["name"].take_value().get_string() },
							.binding = param["binding"]["index"].take_value().get_int64(),
						}
					);
				}
			}

			std::vector<Parameter> parameters;
		};
	};

	template<wgpu::BufferUsage usage = wgpu::BufferUsage::None>
	class DynBuffer : public wgpu::Buffer {
	public:
		DynBuffer(const WgpuContext& ctx) : _ctx(&ctx) {}

		DynBuffer(const WgpuContext& ctx, size_t size) : _ctx(&ctx) {
			const wgpu::BufferDescriptor buffer_desc = {
				.usage			  = usage,
				.size			  = size,
				.mappedAtCreation = false,
			};
			get() = _ctx->device.CreateBuffer(&buffer_desc);
		}

		template<typename T>
			requires std::is_trivially_copyable_v<T>
		DynBuffer(const WgpuContext& ctx, T&& data) : _ctx(&ctx) {
			constexpr wgpu::BufferDescriptor buffer_desc = {
				.usage			  = usage,
				.size			  = sizeof(T),
				.mappedAtCreation = true,
			};
			*this		 = _ctx->device.CreateBuffer(&buffer_desc);

			void* mapped = this->GetMappedRange();
			std::memcpy(mapped, &data, sizeof(T));
			this->Unmap();
		}

	public:
		inline static auto create(const WgpuContext& ctx) -> DynBuffer { return { ctx }; }

		template<typename T>
			requires std::is_trivially_copyable_v<T>
		inline static auto create(const WgpuContext& ctx, T&& data) -> DynBuffer {
			return { ctx, std::forward<T>(data) };
		}

		inline static auto create() -> DynBuffer {
			using namespace stdexec;
			return						   //
				read_env(get_scheduler) |  //
				then([](auto&& sched) -> DynBuffer {
					return { std::forward<decltype(sched)>(sched) };
				});
		}

		template<typename T>
			requires std::is_trivially_copyable_v<T>
		inline static auto create(T&& data) -> DynBuffer {
			using namespace stdexec;
			return						   //
				read_env(get_scheduler) |  //
				then([data = std::forward<T>(data)](auto&& sched) -> DynBuffer {
					return { std::forward<decltype(sched)>(sched), std::forward<T>(data) };
				});
		}

		[[nodiscard]] auto get() -> wgpu::Buffer& { return *this; }

		[[nodiscard]] auto get() const -> const wgpu::Buffer& { return *this; }

		template<auto member_ptr>
		auto sync_mod(const analyze_member_ptr<decltype(member_ptr)>::member_type& what) {
			using Type		 = analyze_member_ptr<decltype(member_ptr)>::type;
			using MemberType = analyze_member_ptr<decltype(member_ptr)>::member_type;

			static_assert(std::is_trivially_copyable_v<MemberType>);

			if (sizeof(MemberType) > GetSize())
				*this = { *_ctx, sizeof(MemberType) };

			_ctx->queue.WriteBuffer(
				get(),
				(char*)&((Type*)nullptr->*member_ptr) - (char*)nullptr,
				&what,
				sizeof(MemberType)
			);
		}

	private:
		const WgpuContext* _ctx;
	};

	template<typename T, wgpu::BufferUsage usage = wgpu::BufferUsage::None>
	class Buffer : public wgpu::Buffer {
	public:
		Buffer(wgpu::Device device, const wgpu::BufferDescriptor& desc) :
			_device(std::move(device)) {
			static_cast<wgpu::Buffer&>(*this) = _device.CreateBuffer(&desc);
		}

		Buffer(const wgpu::Device& device, size_t size) :
			Buffer {
				device,
				wgpu::BufferDescriptor {
										.usage			  = usage,
										.size			  = sizeof(T) * size,
										.mappedAtCreation = false,
										}
		} {}

	private:
		struct [[nodiscard]] WriteBufferSender {
			Buffer&			   buffer;
			std::span<const T> data;

			using sender_concept		= stdexec::sender_t;
			using completion_signatures = stdexec::completion_signatures<
				stdexec::set_value_t(const Buffer&),	  //
				stdexec::set_error_t(std::exception_ptr)  //
				>;

			template<stdexec::receiver R>
			struct Opstate {
				R				   r;
				Buffer&			   buffer;
				std::span<const T> data;

				//
				auto start() noexcept {
					using namespace stdexec;

					auto				sch	   = get_scheduler(get_env(r));
					const wgpu::Queue&	queue  = WgpuContext::query_queue(sch).get();
					const wgpu::Device& device = WgpuContext::query_device(sch).get();

					spdlog::info("{}, {}", buffer.GetSize(), data.size() * sizeof(T));

					if (buffer.GetSize() < data.size() * sizeof(T))
						buffer = { device, data.size() };

					queue.WriteBuffer(buffer, 0, data.data(), data.size() * sizeof(T));

					set_value(r, buffer);
				}
			};

			template<stdexec::receiver R>
			constexpr auto connect(R&& r) const noexcept -> Opstate<R> {
				return { std::forward<R>(r), buffer, data };
			}
		};

		struct [[nodiscard]] AsyncWriteSender {
			const Buffer&	   buffer;
			std::span<const T> data;

			using sender_concept		= stdexec::sender_t;
			using completion_signatures = stdexec::completion_signatures<
				stdexec::set_value_t(const Buffer&),	  //
				stdexec::set_error_t(std::exception_ptr)  //
				>;

			template<stdexec::receiver R>
			struct Opstate {
				R				   r;
				const Buffer&	   buffer;
				std::span<const T> data;

				//
				auto start() noexcept {
					using namespace stdexec;

					buffer.MapAsync(
						wgpu::MapMode::Write,
						0,
						sizeof(T) * data.size(),
						wgpu::CallbackMode::AllowProcessEvents,
						[&](wgpu::MapAsyncStatus status, wgpu::StringView msg) {
							if (status != wgpu::MapAsyncStatus::Success) {
								set_error(
									r,
									std::make_exception_ptr(std::runtime_error { msg.data })
								);
								return;
							}

							std::memcpy(
								buffer.GetMappedRange(),
								data.data(),
								data.size() * sizeof(T)
							);
							set_value(r, buffer);
						}
					);
				}
			};

			template<stdexec::receiver R>
			constexpr auto connect(R&& r) const noexcept -> Opstate<R> {
				return { std::forward<R>(r), buffer, data };
			}
		};

	public:
		auto get() -> wgpu::Buffer& { return *this; }

		auto get() const -> const wgpu::Buffer& { return *this; }

		auto write_buffer(std::span<const T> data) {
			static_assert(usage & wgpu::BufferUsage::CopyDst);
			return WriteBufferSender { *this, data };  // TODO: resize
		}

		auto async_write(std::span<const T> data) const {
			static_assert(usage & wgpu::BufferUsage::MapWrite);
			return AsyncWriteSender { *this, data };
		}

		template<typename DstT, wgpu::BufferUsage dst_usage>
		auto copy_to(
			const Buffer<DstT, dst_usage>& dst, size_t src_offset = 0, size_t dst_offset = 0,
			size_t size = 1
		) {
			using namespace stdexec;
			return	//
				read_env(get_scheduler) | then([&](auto&& sched) {
					const wgpu::Queue&	queue  = WgpuContext::query_queue(sched).get();
					const wgpu::Device& device = WgpuContext::query_device(sched).get();

					auto				cmd	   = device.CreateCommandEncoder();
					cmd.CopyBufferToBuffer(*this, src_offset, dst, dst_offset, sizeof(T) * size);
					auto cb = cmd.Finish();
					queue.Submit(1, &cb);
				});
		}

	private:
		wgpu::Device _device;
	};

	template<typename T>
	using VertexBuffer = Buffer<T, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst>;

	struct alignas(16) Camera {
		
	};

	class Bindgroup : public wgpu::BindGroup {
	public:
		Bindgroup(const WgpuContext& ctx) : _ctx(ctx) {
			wgpu::BindGroupEntry entry = {
				
			};
			
			const wgpu::BindGroupDescriptor bindgroup_desc = {
				// .layout =
				
			};

			get() = _ctx.device.CreateBindGroup(&bindgroup_desc);
		}

	public:
		[[nodiscard]] auto get() -> wgpu::BindGroup& { return *this; }

		[[nodiscard]] auto get() const -> const wgpu::BindGroup& { return *this; }

	private:
		const WgpuContext& _ctx;
	};

	struct WindowSpec {
		int			width  = 800;
		int			height = 600;
		std::string title;
	};

	class Pipeline {
	public:
		Pipeline() : Pipeline({}, {}) {}

		Pipeline(WindowSpec window_spec, const WgpuContext::Spec& context_spec) :

			_ctx(WgpuContext::create(context_spec)), _window_spec(std::move(window_spec)) {}

		~Pipeline() {
			glfwDestroyWindow(_window);
			_as.request_stop();
			stdexec::sync_wait(_as.on_empty());
		}

	public:
		struct Vertice {
			glm::vec3					 pos;
			glm::vec3					 normal;
			glm::vec2					 uv;
			size_t						 tex_id;

			inline static constexpr auto vertex_attrib() noexcept {
				return std::to_array<wgpu::VertexAttribute>({
					{
						.format			= wgpu::VertexFormat::Float32x3,
						.offset			= 0,
						.shaderLocation = 0,
					 },
					{
						.format			= wgpu::VertexFormat::Float32x3,
						.offset			= 0,
						.shaderLocation = 1,
					 },
					{
						.format			= wgpu::VertexFormat::Float32x2,
						.offset			= 0,
						.shaderLocation = 2,
					 },
					{
						.format			= wgpu::VertexFormat::Uint32,
						.offset			= 0,
						.shaderLocation = 3,
					 },
				});
			}
		};

	private:
		inline static constexpr char shader[] = {
#include "Pipeline.wgsl.h"
		};

		auto _init_window() {
			GlfwManager::init();

			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

			_window = glfwCreateWindow(
				_window_spec.width,
				_window_spec.height,
				_window_spec.title.data(),
				nullptr,
				nullptr
			);
			if (!_window) {
				constexpr auto err = "[GLFW]: failed to create window!";
				spdlog::error(err);
				throw std::runtime_error { err };
			}

			glfwSetWindowUserPointer(_window, this);

			_surface = wgpu::glfw::CreateSurfaceForWindow(_ctx.instance, _window);

			wgpu::SurfaceCapabilities capabilities;
			_surface.GetCapabilities(_ctx.adapter, &capabilities);
			_format = capabilities.formats[0];

			//
			const wgpu::SurfaceConfiguration surface_conf = {
				.device		 = _ctx.device,
				.format		 = _format,
				.width		 = static_cast<uint32_t>(_window_spec.width),
				.height		 = static_cast<uint32_t>(_window_spec.height),
				.presentMode = wgpu::PresentMode::Mailbox,
			};
			_surface.Configure(&surface_conf);
		}

		auto _init_pipeline() {
			const wgpu::ShaderSourceWGSL	   wgsl { { .code = shader } };

			const wgpu::ShaderModuleDescriptor shader_module_desc = { .nextInChain = &wgsl };
			const wgpu::ShaderModule		   shader_module =
				_ctx.device.CreateShaderModule(&shader_module_desc);

			const wgpu::ColorTargetState color_target_state = {
				.format = _format,
			};
			const wgpu::FragmentState fragment_state = {
				.module		 = shader_module,
				.targetCount = 1,
				.targets	 = &color_target_state,
			};

			constexpr auto				   vertex_attrib = Vertice::vertex_attrib();
			const wgpu::VertexBufferLayout vertex_layout = {
				.stepMode		= wgpu::VertexStepMode::Vertex,
				.arrayStride	= sizeof(Vertice),
				.attributeCount = vertex_attrib.size(),
				.attributes		= vertex_attrib.data(),
			};

			const wgpu::RenderPipelineDescriptor pipeline_desc = {
						.vertex	  = { 
							.module = shader_module, 
							.bufferCount = 1, 
							.buffers = &vertex_layout, 
						},
						.fragment = &fragment_state,
					};
			_pipeline						 = _ctx.device.CreateRenderPipeline(&pipeline_desc);

			_vertex							 = { _ctx.device, 3 };

			static std::vector<Vertice> data = {
				Vertice { .pos = { .0, -.5, .0 } },
				Vertice { .pos = { .5, .5, .0 } },
				Vertice { .pos = { -.5, .5, .0 } },
			};
			static std::vector<Vertice> data2 = {
				Vertice { .pos = { .0, .5, 0. } },
				Vertice { .pos = { .5, -.5, .0 } },
				Vertice { .pos = { -.5, -.5, .0 } },
			};

			stdexec::sync_wait(
				stdexec::starts_on(_ctx.get_scheduler(), _vertex->write_buffer(data))
			);
			stdexec::sync_wait(
				stdexec::starts_on(_ctx.get_scheduler(), _vertex->write_buffer(data2))
			);

			_dyn_buffer.emplace(_ctx);

			struct Foo {
				int	   a;
				double b;
				char   c;
			};

			_dyn_buffer->sync_mod<&Foo::a>(1);
		}

		void render() {
			wgpu::SurfaceTexture tex;
			_surface.GetCurrentTexture(&tex);

			// _vertex.async_write(
			// 	std::vector {
			// 		Vertice { .pos = { .0, -.5, .0 } },
			// 		Vertice { .pos = { .5, .5, .0 } },
			// 		Vertice { .pos = { -.5, .5, .0 } },
			// 	}
			// );

			// static auto data = std::array {
			// 	Vertice { .pos = { .0, -.5, .0 } },
			// 	Vertice { .pos = { .5, .5, .0 } },
			// 	Vertice { .pos = { -.5, .5, .0 } },
			// };
			// static auto time = std::chrono::high_resolution_clock::now();
			// data[0].pos.x =
			// 	((std::chrono::high_resolution_clock::now() - time).count() % 1000) * .001f;

			// _queue.WriteBuffer(_vertex.get(), 0, data.data(), data.size() * sizeof(Vertice));
			// _vertex.async_write(data);

			const wgpu::RenderPassColorAttachment attachment {
				.view	 = tex.texture.CreateView(),
				.loadOp	 = wgpu::LoadOp::Clear,
				.storeOp = wgpu::StoreOp::Store,
			};

			const wgpu::RenderPassDescriptor renderpass {
				.colorAttachmentCount = 1,
				.colorAttachments	  = &attachment,
			};

			wgpu::CommandEncoder	encoder = _ctx.device.CreateCommandEncoder();
			wgpu::RenderPassEncoder pass	= encoder.BeginRenderPass(&renderpass);
			pass.SetPipeline(_pipeline);
			pass.SetVertexBuffer(0, _vertex->get());
			pass.Draw(3);
			pass.End();
			wgpu::CommandBuffer commands = encoder.Finish();
			_ctx.device.GetQueue().Submit(1, &commands);
		}

	public:
		template<stdexec::scheduler Sch>
		auto get_scheduler_from(Sch&& sched) {
			return _ctx.get_scheduler_from(std::forward<Sch>(sched));
		}

		auto launch() noexcept {
			using namespace stdexec;
			return						   //
				read_env(get_scheduler) |  //
				then([&](auto&& sched) {
					_init_window();
					_init_pipeline();

					spdlog::info("into loop");

					while (!glfwWindowShouldClose(_window)) {
						glfwPollEvents();
						render();
						_surface.Present();
						_ctx.instance.ProcessEvents();
					}
				});
		}

	protected:
		WgpuContext							 _ctx;
		exec::async_scope					 _as;

		GLFWwindow*							 _window;
		WindowSpec							 _window_spec;

		wgpu::TextureFormat					 _format;
		wgpu::Surface						 _surface;
		wgpu::RenderPipeline				 _pipeline;

		std::optional<VertexBuffer<Vertice>> _vertex;
		std::optional<DynBuffer<wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst>>
			_dyn_buffer;
	};

}  // namespace dvdbchar::Render