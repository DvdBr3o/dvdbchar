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
			template<typename Base>
			inline static auto bind(GLFWwindow* window) {
				glfwSetWindowSizeCallback(window, [](GLFWwindow* window, int width, int height) {
					auto& self = *static_cast<Base*>(glfwGetWindowUserPointer(window));
					_auto_execute_all(self, on_window_resize_t {}, Size { width, height });
				});
			}
		};

		inline static constexpr on_window_resize_t on_window_resize;

		struct on_key_t : TagGenTaggedTuple<on_key_t> {
			template<typename Base>
			inline static auto bind(GLFWwindow* window) {
				glfwSetKeyCallback(
					window,
					[](GLFWwindow* window, int key, int scancode, int action, int mods) {
						auto& self = *static_cast<Base*>(glfwGetWindowUserPointer(window));
						_auto_execute_all(
							self,
							on_key_t {},
							KeySignal { key, scancode, action, mods }
						);
					}
				);
			}
		};

		inline static constexpr on_key_t on_key;

		struct on_mouse_moved_t : TagGenTaggedTuple<on_mouse_moved_t> {
			template<typename Base>
			inline static auto bind(GLFWwindow* window) {
				glfwSetCursorPosCallback(window, [](GLFWwindow* window, double x, double y) {
					auto& self = *static_cast<Base*>(glfwGetWindowUserPointer(window));
					_auto_execute_all(
						self,
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

		template<typename... Args, typename... Ts>
		inline static constexpr auto _auto_execute_all(std::tuple<Ts...>& tuple, Args&&... args) {
			std::apply(
				[&](auto&&... ts) {
					(
						[&]() {
							if constexpr (requires {
											  std::invoke(ts, std::forward<Args>(args)...);
										  })
								std::invoke(ts, std::forward<Args>(args)...);
						}(),
						...
					);
				},
				tuple
			);
		}

		template<typename... Ts>
		struct AutoBindedWindowProxy : public std::tuple<Ts...> {
			AutoBindedWindowProxy(GLFWwindow* window, Ts&&... btps) :
				std::tuple<Ts...>(std::forward<Ts>(btps)...) {
				bind(window);
			}

			auto bind(GLFWwindow* window) {
				glfwSetWindowUserPointer(window, this);
				_bind_tag<Window::on_key_t>(window);
				_bind_tag<Window::on_mouse_moved_t>(window);
				_bind_tag<Window::on_window_resize_t>(window);
			}

		private:
			template<typename TagT>
			inline static constexpr auto _bind_tag(GLFWwindow* window) {
				TagT::template bind<AutoBindedWindowProxy<Ts...>>(window);
			}
		};

	public:
		struct EscExiter {
			const Window& window;

			//
			auto operator()(Window::on_key_t, const KeySignal& key) const {
				if (key.key == GLFW_KEY_ESCAPE && key.action == GLFW_RELEASE)
					glfwSetWindowShouldClose(window.window(), GLFW_TRUE);
			}
		};

		struct Spec {
			int				 width	= 800;
			int				 height = 600;
			std::string_view title;
			bool			 transparent = false;
		};

	public:
		Window(const WgpuContext& ctx, const Spec& spec) {
			GlfwManager::init();

			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
			glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, spec.transparent);

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

		Window(const Spec& spec) : Window(WgpuContext::global(), spec) {}

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

		template<typename... BindableTs>
		[[nodiscard]] auto bind(BindableTs&&... binadable) const
			-> AutoBindedWindowProxy<std::remove_cvref_t<BindableTs>...> {
			return { _window, std::forward<BindableTs>(binadable)... };
		}

	private:
		GLFWwindow*			_window;
		wgpu::Surface		_surface;
		wgpu::TextureFormat _format;
	};
}  // namespace dvdbchar::Render