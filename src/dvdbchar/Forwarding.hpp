#pragma once

#include "dvdbchar/Glfw.hpp"

#ifdef _WIN32
#	include <sdkddkver.h>
#	define WIN32_LEAN_AND_MEAN	 // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#	include <windows.h>
#	include <SpoutGL/Spout.h>
#	include <SpoutDX/SpoutDX.h>
#	include <GL/GL.h>
#	pragma comment(lib, "opengl32.lib")
#endif
#include <spdlog/spdlog.h>

#include <stdexcept>

namespace dvdbchar {
#ifdef _WIN32
	class SpoutForwarder {
	public:
		struct ImageView {
			const unsigned char* data;
			int					 width;
			int					 heigh;
		};

	public:
		SpoutForwarder(std::string_view name = "dvdbchar") {
			GlfwManager::init();

			glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
			glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);

			_window =
				glfwCreateWindow(800, 600, std::format("{}_window", name).data(), nullptr, nullptr);
			if (!_window)
				throw std::runtime_error {
					std::format("Failed to create window for spout forwarder `{}`.", name)
				};
			glfwMakeContextCurrent(_window);

			if (!_sender.CreateOpenGL())
				throw std::runtime_error {
					std::format("Failed to init spout opengl context for forwarder `{}`.", name)
				};
			_sender.SetSenderName(name.data());
			spdlog::info("Created forwarder `{}`!", name);
		}

	public:
		void send(ImageView img) { _sender.SendImage(img.data, img.width, img.heigh); }

	private:
		GLFWwindow* _window;
		Spout		_sender;
	};
#endif
}  // namespace dvdbchar
