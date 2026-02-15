#pragma once

#include "dvdbchar/Render/Texture.hpp"
#include "dvdbchar/Model.hpp"
#include "dvdbchar/Render/Pass/BasePass.hpp"
#include "dvdbchar/Render/ShaderReflection.hpp"
#include "dvdbchar/Render/Window.hpp"
#include "dvdbchar/Render/Camera.hpp"
#include "dvdbchar/Render/Pipeline.hpp"
#include "dvdbchar/Render/Buffer.hpp"
#include "dvdbchar/Render/Buffer.hpp"
#include "dvdbchar/Render/Mesh.hpp"

#include <webgpu/webgpu_cpp.h>

namespace dvdbchar {
	namespace details::vtubing_app {
		using namespace Render;

		class VtubingApp final {
		public:
			struct Spec {
				Window::Spec window;
				Model&&		 model;
			};

		public:
			// clang-format off
		VtubingApp(const Spec& spec) :
			_window(spec.window), _model(std::move(spec.model)),
            _ppl_base {{
                .shader     = *read_text_from("shaders/Pipeline.wgsl"),
		        .reflection = *read_text_from("shaders/Uniform.layout.json"),
                .format     = _window.format(),
            }},
            _global_ub(get_mapping<GlobalRefl>("global", "shaders/Uniform.refl.json")),
            _global_bg {{
                .layout  = parsed::bindgroup_layout_from_path("global", "shaders/Uniform.layout.json"),
                .entries = std::array { 
                    uniform_buffer_bindgroup(_global_ub),
                }, 
            }},
            _camera_ub(get_mapping<CameraRefl>("camera", "shaders/Uniform.refl.json")),
            _camera_bg {{
                .layout	 = parsed::bindgroup_layout_from_path("camera", "shaders/Uniform.layout.json"),
                .entries = std::array {
                    uniform_buffer_bindgroup(_camera_ub),
                }
            }}
        {
            _cam.aspect = _window.aspect();
            _camera_ub.write(_camera_ub.view_matrix, _cam.view_matrix());
	        _camera_ub.write(_camera_ub.projection_matrix, _cam.projection_matrix());
        }

			// clang-format on

		public:
			void launch() {
				auto bind = _window.bind(
					FpsCameraController { _cam },
					Window::EscExiter { _window },
					CameraAspectAdaptor { _cam },
					[&](Window::on_mouse_moved_t, auto&&...) {
						std::unique_lock lock { _mtx_context };
						_camera_ub.write(_camera_ub.view_matrix, _cam.view_matrix());
						_camera_ub.write(_camera_ub.projection_matrix, _cam.projection_matrix());
					}
				);

				std::jthread render_job { [&]() { render(); } };
				while (!glfwWindowShouldClose(_window.window())) glfwPollEvents();
			}

			void render() {
				auto& context = WgpuContext::global();

				auto  vb	  = ArrayVertexBuffer<Vertice> {
					  std::array {
								Vertice { .pos = { .5, .5, 0. } },
								Vertice { .pos = { .5, -.5, .0 } },
								Vertice { .pos = { -.5, -.5, .0 } },
								Vertice { .pos = { -.5, .5, .0 } },
								}
				};
				auto ib = ArrayIndexBuffer {
					std::array { 0u, 1u, 2u, 0u, 2u, 3u }
				};
				auto mesh = MeshPrimitive {
					.buf_vertex		 = vb,
					.buf_index		 = ib,
					.buf_index_count = 6,
				};

				auto							   test_mesh = _model._test_prmitive();

				auto							   tex_depth = depth_texture(_window.get_size());

				std::vector<Render::MeshPrimitive> gpu_primitives;
				for (const auto& mesh : _model.asset().meshes)
					for (const auto& prim : mesh.primitives)
						gpu_primitives.push_back(_model.primitive(prim));

				while (!glfwWindowShouldClose(_window.window())) {
					wgpu::SurfaceTexture tex;
					_window.surface().GetCurrentTexture(&tex);

					std::unique_lock lock { _mtx_context };

					//
					auto cmd = context.device.CreateCommandEncoder();
					auto pass =
						Pass::BasePass {
							.tex_target = { tex.texture },
							.tex_depth	= { tex_depth },
						}
							.start(cmd);
					for (const auto& prim : gpu_primitives) {
						pass.execute(
							prim,
							_ppl_base,
							{
								_global_bg,
								_camera_bg,
								prim.bg_pbr,
							}
						);
					}

					auto cbf = pass.end();
					context.queue.Submit(1, &cbf);

					_window.surface().Present();
					context.instance.ProcessEvents();
				}
			}

		private:
			mutable std::mutex _mtx_context;
			Window			   _window;
			// clang-format off
			Camera			   _cam = {
				.position  = { 0., 0.,  1. },
				.direction = { 0., 0., -2. },
			};
			// clang-format on
			Pipeline _ppl_base;

			//
			ReflectedUniformBuffer<GlobalRefl> _global_ub;
			Bindgroup						   _global_bg;
			ReflectedUniformBuffer<CameraRefl> _camera_ub;
			Bindgroup						   _camera_bg;

			//
			Model _model;
		};
	}  // namespace details::vtubing_app

	using details::vtubing_app::VtubingApp;
}  // namespace dvdbchar