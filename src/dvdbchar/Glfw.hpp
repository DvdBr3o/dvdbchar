#pragma once

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#include <stdexcept>

namespace dvdbchar {
	class GlfwManager {
	public:
		~GlfwManager() { glfwTerminate(); }

		inline static auto& init() {
			static GlfwManager manager;
			return manager;
		}

	private:
		GlfwManager() {
			glfwSetErrorCallback([](int error_code, const char* description) {
				spdlog::error("[GLFW/{}]: {}", error_code, description);
			});

			if (!glfwInit()) {
				spdlog::critical("Failed to init glfw!");
				throw std::runtime_error { "Failed to init glfw!" };
			}
		}
	};
}  // namespace dvdbchar