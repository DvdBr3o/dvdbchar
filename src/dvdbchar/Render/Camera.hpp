#pragma once

#include "dvdbchar/Render/Primitives.hpp"
#include "dvdbchar/Render/Window.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <GLFW/glfw3.h>

namespace dvdbchar::Render {
	struct Camera {
		glm::vec3					 position;
		glm::vec3					 direction;
		float						 fov = 90.f;
		float						 aspect;

		inline static constexpr auto up = glm::vec3(0., 1., 0.);

		//
		[[nodiscard]] auto view_matrix() const -> glm::mat4 {
			return glm::lookAt(	 //
				position,
				position + direction,
				up
			);
		}

		[[nodiscard]] auto view_matrix(const glm::vec3& up) const -> glm::mat4 {
			return glm::lookAt(	 //
				position,
				position + direction,
				up
			);
		}

		[[nodiscard]] auto projection_matrix() const -> glm::mat4 {
			return glm::perspective(  //
				fov,
				aspect,
				.001f,
				100.f
			);
		}

		template<typename... Args>
		inline static auto from(const Window& window, Args&&... args) -> Camera {
			Camera camera = { std::forward<Args>(args)... };
			camera.aspect = window.aspect();
			return camera;
		}
	};

	struct FpsCameraController {
		Camera& cam;
		bool	_first_mouse = true;
		Pos		_mouse;

		//
		auto operator()(const Window::on_key_t, const KeySignal& key) {
			constexpr auto velocity = .1f;

			switch (key.action) {
				case GLFW_REPEAT:
				case GLFW_PRESS:
					switch (key.key) {
						case GLFW_KEY_W:
							cam.position +=
								glm::vec3(cam.direction.x, 0.f, cam.direction.z) * velocity;
							break;
						case GLFW_KEY_S:
							cam.position -=
								glm::vec3(cam.direction.x, 0.f, cam.direction.z) * velocity;
							break;
						case GLFW_KEY_A:
							cam.position -= glm::cross(
												glm::vec3(cam.direction.x, 0.f, cam.direction.z),
												Camera::up
											)
										  * velocity;
							break;
						case GLFW_KEY_D:
							cam.position += glm::cross(
												glm::vec3(cam.direction.x, 0.f, cam.direction.z),
												Camera::up
											)
										  * velocity;
							break;
						case GLFW_KEY_SPACE: cam.position += Camera::up * velocity; break;
						case GLFW_KEY_LEFT_SHIFT: cam.position -= Camera::up * velocity; break;
						default: break;
					}
			}
		}

		auto operator()(Window::on_mouse_moved_t, const Pos& pos) {
			constexpr auto sensivity = .1f;

			if (_first_mouse) {
				_mouse		 = pos;
				_first_mouse = false;
			}
			const auto dx		   = pos.x - _mouse.x;
			const auto dy		   = pos.y - _mouse.y;
			_mouse				   = pos;

			constexpr auto world   = Camera::up;
			const auto	   local   = glm::normalize(glm::cross(cam.direction, world));

			const auto	   q_yaw   = glm::angleAxis(glm::radians<float>(-dx * sensivity), world);
			const auto	   q_pitch = glm::angleAxis(
				std::clamp(glm::radians<float>(-dy * sensivity), -89.99f, 89.99f),
				local
			);
			const auto q  = q_yaw * q_pitch;

			cam.direction = glm::normalize(q * cam.direction);
		}
	};

	struct CameraAspectAdaptor {
		Camera& cam;

		auto	operator()(Window::on_window_resize_t, const Size& size) {
			   cam.aspect = size.aspect();
		}
	};
}  // namespace dvdbchar::Render
