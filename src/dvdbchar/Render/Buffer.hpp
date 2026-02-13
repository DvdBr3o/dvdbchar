#pragma once

#include "Context.hpp"
#include "dvdbchar/Render/Context.hpp"
#include "dvdbchar/Render/ShaderReflection.hpp"

#include <webgpu/webgpu_cpp.h>

#include <span>

namespace dvdbchar::Render {
	template<wgpu::BufferUsage usage = wgpu::BufferUsage::None>
	class Buffer : public wgpu::Buffer {
	public:
		Buffer(const WgpuContext& ctx) : _ctx(&ctx) {}

		Buffer(const WgpuContext& ctx, size_t size) : _ctx(&ctx) {
			const wgpu::BufferDescriptor buffer_desc = {
				.usage			  = usage,
				.size			  = size,
				.mappedAtCreation = false,
			};
			get() = _ctx->device.CreateBuffer(&buffer_desc);
		}

		template<typename T>
			requires std::is_trivially_copyable_v<T>
		Buffer(const WgpuContext& ctx, T&& data) : _ctx(&ctx) {
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
		inline static auto create(const WgpuContext& ctx) -> Buffer { return { ctx }; }

		template<typename T>
			requires std::is_trivially_copyable_v<T>
		inline static auto create(const WgpuContext& ctx, T&& data) -> Buffer {
			return { ctx, std::forward<T>(data) };
		}

		inline static auto create() -> Buffer {
			using namespace stdexec;
			return						   //
				read_env(get_scheduler) |  //
				then([](auto&& sched) -> Buffer {
					return { std::forward<decltype(sched)>(sched) };
				});
		}

		template<typename T>
			requires std::is_trivially_copyable_v<T>
		inline static auto create(T&& data) -> Buffer {
			using namespace stdexec;
			return						   //
				read_env(get_scheduler) |  //
				then([data = std::forward<T>(data)](auto&& sched) -> Buffer {
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

		template<typename T>
		auto sync_mod(size_t offset, T&& data) const {}

	private:
		const WgpuContext* _ctx;
	};

	template<typename T, wgpu::BufferUsage usage = wgpu::BufferUsage::None>
	class LegacyBuffer : public wgpu::Buffer {
	public:
		LegacyBuffer(wgpu::Device device, const wgpu::BufferDescriptor& desc) :
			_device(std::move(device)) {
			static_cast<wgpu::Buffer&>(*this) = _device.CreateBuffer(&desc);
		}

		LegacyBuffer(const wgpu::Device& device, size_t size) :
			LegacyBuffer {
				device,
				wgpu::BufferDescriptor {
										.usage			  = usage,
										.size			  = sizeof(T) * size,
										.mappedAtCreation = false,
										}
		} {}

	private:
		struct [[nodiscard]] WriteBufferSender {
			LegacyBuffer&	   buffer;
			std::span<const T> data;

			using sender_concept		= stdexec::sender_t;
			using completion_signatures = stdexec::completion_signatures<
				stdexec::set_value_t(const LegacyBuffer&),	//
				stdexec::set_error_t(std::exception_ptr)	//
				>;

			template<stdexec::receiver R>
			struct Opstate {
				R				   r;
				LegacyBuffer&	   buffer;
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
			const LegacyBuffer& buffer;
			std::span<const T>	data;

			using sender_concept		= stdexec::sender_t;
			using completion_signatures = stdexec::completion_signatures<
				stdexec::set_value_t(const LegacyBuffer&),	//
				stdexec::set_error_t(std::exception_ptr)	//
				>;

			template<stdexec::receiver R>
			struct Opstate {
				R					r;
				const LegacyBuffer& buffer;
				std::span<const T>	data;

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
			const LegacyBuffer<DstT, dst_usage>& dst, size_t src_offset = 0, size_t dst_offset = 0,
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
	using LegacyVertexBuffer =
		LegacyBuffer<T, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst>;

	template<typename T, wgpu::BufferUsage usage>
	class ArrayBuffer : public wgpu::Buffer {
	public:
		ArrayBuffer(const WgpuContext& ctx, std::span<const T> data) {
			const wgpu::BufferDescriptor buffer_desc = {
				.usage			  = usage,
				.size			  = data.size() * sizeof(T),
				.mappedAtCreation = false,
			};
			static_cast<wgpu::Buffer&>(*this) = ctx.device.CreateBuffer(&buffer_desc);

			ctx.queue.WriteBuffer(*this, 0, data.data(), data.size() * sizeof(T));
		}

		ArrayBuffer(const WgpuContext& ctx, size_t size) {
			const wgpu::BufferDescriptor buffer_desc = {
				.usage			  = usage,
				.size			  = size,
				.mappedAtCreation = false,
			};
			static_cast<wgpu::Buffer&>(*this) = ctx.device.CreateBuffer(&buffer_desc);
		}

		ArrayBuffer(std::span<const T> data) : ArrayBuffer(WgpuContext::global(), data) {}

		ArrayBuffer(size_t size) : ArrayBuffer(WgpuContext::global(), size) {}

	public:
		auto write(const WgpuContext& ctx, size_t offset, std::span<const T> data) {
			if (this->GetSize() < data.size() * sizeof(T))
				*this = ArrayBuffer { ctx, data.size() * sizeof(T) };
			ctx.queue.WriteBuffer(*this, offset, data.data(), data.size() * sizeof(T));
		}

		auto write(const WgpuContext& ctx, std::span<const T> data) { return write(ctx, 0, data); }

		auto write(size_t offset, std::span<const T> data) {
			WgpuContext::global()
				.queue.WriteBuffer(*this, offset, data.data(), data.size() * sizeof(T));
		}

		auto write(std::span<const T> data) {
			WgpuContext::global().queue.WriteBuffer(*this, 0, data.data(), data.size() * sizeof(T));
		}
	};

	template<typename T, wgpu::BufferUsage usage = wgpu::BufferUsage::None>
	using ArrayVertexBuffer =
		ArrayBuffer<T, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst | usage>;
	using ArrayIndexBuffer =
		ArrayBuffer<uint32_t, wgpu::BufferUsage::Index | wgpu::BufferUsage::CopyDst>;

	template<typename T>
	class StaticVertexBuffer : public wgpu::Buffer {
	public:
		inline static constexpr auto usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;

	public:
		StaticVertexBuffer(
			const WgpuContext& ctx, std::span<const T> data, std::string_view label = {}
		) {
			const wgpu::BufferDescriptor buffer_desc = {
				.label			  = label,
				.usage			  = usage,
				.size			  = sizeof(T) * data.size(),
				.mappedAtCreation = false,
			};
			static_cast<wgpu::Buffer&>(*this) = ctx.device.CreateBuffer(&buffer_desc);

			// MapAsync(
			// 	wgpu::MapMode::Write,
			// 	0,
			// 	sizeof(T) * data.size(),
			// 	wgpu::CallbackMode::AllowProcessEvents,
			// 	[&](wgpu::MapAsyncStatus status, wgpu::StringView msg) {
			// 		if (status != wgpu::MapAsyncStatus::Success) {
			// 			spdlog::error("wgpu failed to creaet buffer: {}", msg.data);
			// 			return;
			// 		}
			// 		std::memcpy(GetMappedRange(), data.data(), data.size() * sizeof(T));
			// 		Unmap();
			// 	}
			// );
			ctx.queue.WriteBuffer(*this, 0, data.data(), sizeof(T) * data.size());
		}

		StaticVertexBuffer(std::span<const T> data, std::string_view label = {}) :
			StaticVertexBuffer(WgpuContext::global(), data, label) {}

	public:
		[[nodiscard]] auto get() const -> const wgpu::Buffer& { return *this; }
	};

	template<typename T>
	inline static constexpr auto create_static_vertex_buffer(std::span<T> data)
		-> StaticVertexBuffer<T> {
		using namespace stdexec;
		return						   //
			read_env(get_scheduler) |  //
			then([data](auto&& sched) -> StaticVertexBuffer<T> { return { sched, data }; });
	}

	template<typename T>
	class ReflectedUniformBuffer {};

	template<ReflMapped T>
	class ReflectedUniformBuffer<T> : public wgpu::Buffer, public ReflectedParameter<T> {
	public:
		template<typename... Args>
		ReflectedUniformBuffer(
			const WgpuContext& ctx, const ReflectedParameter<T>& refl, Args&&... args
		) :
			ReflectedParameter<T>(refl) {
			const wgpu::BufferDescriptor buffer_desc = {
				.usage			  = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
				.size			  = refl.size,
				.mappedAtCreation = false,
			};
			static_cast<wgpu::Buffer&>(*this) = ctx.device.CreateBuffer(&buffer_desc);
		}

		template<typename... Args>
		ReflectedUniformBuffer(const ReflectedParameter<T>& refl, Args&&... args) :
			ReflectedUniformBuffer(WgpuContext::global(), refl, std::forward<Args>(args)...) {}

	public:
		template<typename Data>
		auto write(const WgpuContext& ctx, const Field<Data>& field, const Data& data) {
			ctx.queue.WriteBuffer(*this, field.offset, &data, field.size);
		}

		template<typename Data>
		auto write(const Field<Data>& field, const Data& data) {
			return write(WgpuContext::global(), field, data);
		}
	};

	// template<ParamMapped T>
	// class ReflectedUniformBuffer<T> : public wgpu::Buffer {
	// public:
	// 	ReflectedUniformBuffer(const WgpuContext& ctx, const T& refl) {
	// 		const wgpu::BufferDescriptor buffer_desc = {
	// 			.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
	// 			// .size			  = refl.,
	// 			.mappedAtCreation = false,
	// 		};
	// 		static_cast<wgpu::Buffer&>(*this) = ctx.device.CreateBuffer(&buffer_desc);
	// 	}

	// private:
	// };

	template<ReflMapped T>
	inline auto uniform_buffer_bindgroup(size_t binding, const ReflectedUniformBuffer<T>& buffer)
		-> wgpu::BindGroupEntry {
		return {
			.binding = binding,
			.buffer	 = buffer,
			.offset	 = buffer.offset,
			.size	 = buffer.size,
		};
	}

	template<ReflMapped T>
	inline auto uniform_buffer_bindgroup(const ReflectedUniformBuffer<T>& buffer)
		-> wgpu::BindGroupEntry {
		return {
			.binding = 0,
			.buffer	 = buffer,
			.offset	 = buffer.offset,
			.size	 = buffer.size,
		};
	}

	class DynamicBuffer : public wgpu::Buffer {
	public:
		struct Spec {};

	public:
		DynamicBuffer(const WgpuContext& ctx, const Spec& spec) {
			const wgpu::BufferDescriptor desc {

			};
		}
	};

	class DynamicBufferPool {
	public:
	private:
		std::vector<DynamicBuffer> _buffers;
	};

	template<typename T, wgpu::BufferUsage usage>
	inline auto array_buffer(const WgpuContext& ctx, std::span<const T> data) -> wgpu::Buffer {
		const wgpu::BufferDescriptor desc {
			.usage			  = usage,
			.size			  = sizeof(T) * data.size(),
			.mappedAtCreation = false,
		};
		wgpu::Buffer buffer = ctx.device.CreateBuffer(&desc);

		ctx.queue.WriteBuffer(buffer, 0, data.data(), sizeof(T) * data.size());

		return buffer;
	}

	template<typename T, wgpu::BufferUsage usage>
	inline auto array_buffer(std::span<const T> data) -> wgpu::Buffer {
		return array_buffer(WgpuContext::global(), data);
	}

	template<typename VerticeT>
	inline auto array_vertex_buffer(const WgpuContext& ctx, std::span<const VerticeT> data)
		-> wgpu::Buffer {
		return array_buffer<VerticeT, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst>(
			ctx,
			data
		);
	}

	template<typename VerticeT>
	inline auto array_vertex_buffer(std::span<const VerticeT> data) -> wgpu::Buffer {
		return array_vertex_buffer<VerticeT>(WgpuContext::global(), data);
	}

	template<typename T>
	inline auto array_index_buffer(const WgpuContext& ctx, std::span<const T> data)
		-> wgpu::Buffer {
		return array_buffer<T, wgpu::BufferUsage::Index | wgpu::BufferUsage::CopyDst>(ctx, data);
	}

	template<typename T>
	inline auto array_index_buffer(std::span<const T> data) -> wgpu::Buffer {
		return array_index_buffer<T>(WgpuContext::global(), data);
	}
}  // namespace dvdbchar::Render