#pragma once

#include "Context.hpp"
#include "dvdbchar/Render/Context.hpp"
#include "dvdbchar/Utils.hpp"
#include "dvdbchar/Glfw.hpp"

#include <GLFW/glfw3.h>
#include <dawn/webgpu_cpp.h>
#include <webgpu/webgpu_glfw.h>

#include <string_view>

namespace dvdbchar::Render {
	class Window {
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

			// glfwSetWindowUserPointer(_window, this);

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

	private:
		GLFWwindow*			_window;
		wgpu::Surface		_surface;
		wgpu::TextureFormat _format;
	};
}  // namespace dvdbchar::Render