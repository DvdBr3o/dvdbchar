#pragma once

#include <dawn/webgpu_cpp.h>
#include <stdexec/execution.hpp>
#include <exec/async_scope.hpp>
#include <exec/env.hpp>
#include <spdlog/spdlog.h>

#include <stdexcept>
#include <functional>
#include <exception>
#include <stop_token>
#include <thread>
#include <utility>
#include <span>
#include "stdexec/__detail/__execution_fwd.hpp"
#include "stdexec/__detail/__receivers.hpp"

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
			const wgpu::Instance& instance;
			std::stop_token		  st;

			using sender_concept = stdexec::sender_t;
			using completion_signatures =
				stdexec::completion_signatures<stdexec::set_value_t(), stdexec::set_stopped_t()>;

			template<stdexec::receiver R>
			struct Opstate {
				R					  r;
				const wgpu::Instance& instance;
				std::stop_token		  st;

				//
				auto start() noexcept {
					using namespace stdexec;

					spdlog::info("lauching...");

					while (!st.stop_requested()) {
						instance.ProcessEvents();
						// std::this_thread::yield();
					}

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

		[[nodiscard]] auto launch(const std::stop_token& st) const -> LaunchSender {
			return { instance, st };
		}
	};

	template<typename T, wgpu::BufferUsage usage = wgpu::BufferUsage::None>
	class Buffer : public wgpu::Buffer {
	public:
		Buffer(wgpu::Device device, const wgpu::BufferDescriptor& desc) :
			_device(std::move(device)) {
			static_cast<wgpu::Buffer&>(*this) = _device.CreateBuffer(&desc);
		}

		Buffer(wgpu::Device device, size_t size) :
			Buffer {
				device,
				wgpu::BufferDescriptor {
										.usage			  = usage,
										.size			  = sizeof(T) * size,
										.mappedAtCreation = false,
										}
		} {}

	private:
		struct WriteBufferSender {
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

					auto sch = get_scheduler(get_env(r));

					// const auto& queue = sch.ctx.get().queue;
					const wgpu::Queue& queue = WgpuContext::query_queue(sch).get();

					queue.WriteBuffer(buffer, 0, data.data(), data.size() * sizeof(T));

					set_value(r, buffer);
				}
			};

			template<stdexec::receiver R>
			constexpr auto connect(R&& r) const {
				return Opstate<R> { std::forward<R>(r), buffer, data };
			}
		};

		struct AsyncWriteSender {
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
		auto write_buffer(std::span<const T> data) const {
			static_assert(usage & wgpu::BufferUsage::CopyDst);
			return WriteBufferSender { *this, data };
		}

		auto async_write(std::span<const T> data) const {
			static_assert(usage & wgpu::BufferUsage::MapWrite);
			return AsyncWriteSender { *this, data };
		}

		auto sync_write_buffer() {}

	private:
		wgpu::Device _device;
	};

}  // namespace dvdbchar::Render