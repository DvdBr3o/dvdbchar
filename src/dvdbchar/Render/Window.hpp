#pragma once

#include "dvdbchar/Render/Context.hpp"
#include "dvdbchar/Render/Primitives.hpp"
#include "dvdbchar/Utils.hpp"
#include "dvdbchar/Glfw.hpp"

#include <GLFW/glfw3.h>
#include <dawn/webgpu_cpp.h>
#include <webgpu/webgpu_glfw.h>

#include <string_view>
#include <utility>

namespace dvdbchar::Render {
	class Window {
	public:
		struct on_window_resize_t : TagGenTaggedTuple<on_window_resize_t> {
			template<typename Base, size_t I>
			inline static auto bind(GLFWwindow* window) {
				glfwSetWindowSizeCallback(window, [](GLFWwindow* window, int width, int height) {
					auto& self = *static_cast<Base*>(glfwGetWindowUserPointer(window));
					_execute_all(
						std::get<I>(self).get(),
						on_window_resize_t {},
						Size { width, height }
					);
				});
			}
		};

		inline static constexpr on_window_resize_t on_window_resize;

		struct on_key_t : TagGenTaggedTuple<on_key_t> {
			template<typename Base, size_t I>
			inline static auto bind(GLFWwindow* window) {
				glfwSetKeyCallback(
					window,
					[](GLFWwindow* window, int key, int scancode, int action, int mods) {
						auto& self = *static_cast<Base*>(glfwGetWindowUserPointer(window));
						_execute_all(
							std::get<I>(self).get(),
							on_key_t {},
							KeySignal { key, scancode, action, mods }
						);
					}
				);
			}
		};

		inline static constexpr on_key_t on_key;

		struct on_mouse_moved_t : TagGenTaggedTuple<on_mouse_moved_t> {
			template<typename Base, size_t I>
			inline static auto bind(GLFWwindow* window) {
				glfwSetCursorPosCallback(window, [](GLFWwindow* window, double x, double y) {
					auto& self = *static_cast<Base*>(glfwGetWindowUserPointer(window));
					_execute_all(
						std::get<I>(self).get(),
						on_mouse_moved_t {},
						Pos { static_cast<int>(x), static_cast<int>(y) }
					);
				});
			}
		};

		inline static constexpr on_mouse_moved_t on_mouse_moved;

		struct on_mouse_pressed_t : TagGenTaggedTuple<on_mouse_pressed_t> {};

		inline static constexpr on_mouse_pressed_t on_mouse_pressed;

		struct on_mouse_released_t : TagGenTaggedTuple<on_mouse_released_t> {};

		inline static constexpr on_mouse_released_t on_mouse_released;

	private:
		template<typename... Args, typename... Ts>
		inline static constexpr auto _execute_all(std::tuple<Ts...>& tuple, Args&&... args) {
			std::apply(
				[&](auto&&... ts) { (std::invoke(ts, std::forward<Args>(args)...), ...); },
				tuple
			);
		}

		template<typename... Ts>
		struct BindedWindowProxy {};

		template<Like<TaggedTuple>... Ttps>
		struct BindedWindowProxy<Ttps...> : public std::tuple<Ttps...> {
			BindedWindowProxy(GLFWwindow* window, Ttps&&... btps) :
				std::tuple<Ttps...>(std::forward<Ttps>(btps)...) {
				bind(window);
			}

			auto bind(GLFWwindow* window) {
				glfwSetWindowUserPointer(window, this);
				[&]<size_t... Is>(std::index_sequence<Is...>) {
					(Ttps::tag_type::template bind<BindedWindowProxy<Ttps...>, Is>(window), ...);
				}(std::make_index_sequence<sizeof...(Ttps)>());
			}
		};

	public:
		struct Spec {
			int				 width;
			int				 height;
			std::string_view title;
		};

	public:
		Window(const WgpuContext& ctx, const Spec& spec = {}) {
			GlfwManager::init();

			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

			_window =
				glfwCreateWindow(spec.width, spec.height, spec.title.data(), nullptr, nullptr);
			if (!_window)
				panic("[GLFW]: failed to create window!");

			_surface = wgpu::glfw::CreateSurfaceForWindow(ctx.instance, _window);

			wgpu::SurfaceCapabilities capabilities;
			_surface.GetCapabilities(ctx.adapter, &capabilities);
			_format = capabilities.formats[0];

			//
			const wgpu::SurfaceConfiguration surface_conf = {
				.device		 = ctx.device,
				.format		 = _format,
				.width		 = static_cast<uint32_t>(spec.width),
				.height		 = static_cast<uint32_t>(spec.height),
				.presentMode = capabilities.presentModes[0],
			};
			_surface.Configure(&surface_conf);
		}

		Window(const Spec& spec = {}) : Window(WgpuContext::global(), spec) {}

		~Window() { glfwDestroyWindow(_window); }

	public:
		[[nodiscard]] auto window() const -> GLFWwindow* { return _window; }

		[[nodiscard]] auto should_close() const -> bool { return glfwWindowShouldClose(_window); }

		[[nodiscard]] auto surface() const -> const wgpu::Surface& { return _surface; }

		[[nodiscard]] auto format() const -> wgpu::TextureFormat { return _format; }

		[[nodiscard]] auto get_size() const -> Size {
			Size size {};
			glfwGetWindowSize(_window, &size.width, &size.height);
			return size;
		}

		[[nodiscard]] auto aspect() const -> float {
			const auto [width, height] = get_size();
			return static_cast<float>(width) / static_cast<float>(height);
		}

		template<Like<TaggedTuple>... Tps>
		auto bind(Tps&&... tps) -> BindedWindowProxy<std::remove_cvref_t<Tps>...> {
			return { _window, std::forward<Tps>(tps)... };
		}

	private:
		GLFWwindow*			_window;
		wgpu::Surface		_surface;
		wgpu::TextureFormat _format;
	};
}  // namespace dvdbchar::Render