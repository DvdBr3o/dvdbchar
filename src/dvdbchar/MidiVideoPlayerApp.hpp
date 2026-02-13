#pragma once

#include "dvdbchar/Render/Window.hpp"
#include "opencv2/core/types.hpp"

#include <rtmidi/RtMidi.h>
#include <opencv2/opencv.hpp>

namespace dvdbchar {
	namespace details::midi_video_player_app {
		using namespace Render;

		class MidiIn : public RtMidiIn {
		public:
			using RtMidiIn::RtMidiIn;
			using Callback = std::function<void(double, std::vector<unsigned char>*)>;

		public:
			void setCallback(Callback&& f) {
				_callback = std::move(f);
				this->RtMidiIn::setCallback(
					[](double timeStamp, std::vector<unsigned char>* message, void* userData) {
						std::invoke(*static_cast<Callback*>(userData), timeStamp, message);
					},
					&_callback
				);
			}

		private:
			Callback _callback;
		};

		class MidiVideoPlayerApp {
		public:
			struct Spec {
				std::filesystem::path	   video_path;
				std::optional<std::string> title;
			};

		public:
			MidiVideoPlayerApp(const Spec& spec) :
				_video_path(spec.video_path), _title(spec.title) {
				_in.openPort(0);
				_in.ignoreTypes(false, false, false);
				_in.setCallback([&](double time_stamp, std::vector<unsigned char>* message) {
					if (message->empty()) {
						spdlog::warn("empty midi message!");
						return;
					}

					if (message->at(3) != 0x06)
						return;

					const auto status = message->at(4);
					switch (status) {
						case 0x02:	// Start
							spdlog::info("Start!");
							_is_playing	  = !_is_playing;
							_should_reset = false;
							break;
						case 0x01:	// Restart
							spdlog::info("Stop!");
							_is_playing	  = false;
							_should_reset = true;
							break;
						default: spdlog::info("Unexpected midi note: {:X}", status); break;
					}
				});
			}

		public:
			void launch() {
				cv::VideoCapture cap(_video_path.string());
				if (!cap.isOpened()) {
					panic(std::format("failed to open video at `{}`", _video_path.string()));
					return;
				}

				const double fps	= cap.get(cv::CAP_PROP_FPS);
				const int	 width	= static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
				const int	 height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));

				// const auto time_per_frame = static_cast<int>(1000.0 / fps);
				const auto time_per_frame = 1000.0 / fps;
				spdlog::info("fps = {}", fps);
				spdlog::info("time per frame = {}", time_per_frame);

				const auto title = _title.value_or(_video_path.string());

				cv::namedWindow(title, cv::WINDOW_NORMAL);
				cv::resizeWindow(title, width / 2, height / 2);

				cv::Mat frame;

				// 3. 渲染主循环
				while (true) {
					auto start = std::chrono::steady_clock::now();

					if (cv::getWindowProperty(title, cv::WND_PROP_VISIBLE) < 1)
						break;

					if (_should_reset) {
						cap.set(cv::CAP_PROP_POS_FRAMES, 0);
						_should_reset = false;
					}

					if (_is_playing) {
						if (!cap.read(frame))
							cap.set(cv::CAP_PROP_POS_FRAMES, 0);
					}

					if (!frame.empty())
						cv::imshow(title, frame);

					// const auto end = std::chrono::steady_clock::now();
					// const auto elapsed =
					// 	std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

					// const auto target_delay = time_per_frame - static_cast<double>(elapsed);
					// spdlog::info("target_delay = {}", target_delay);
					// int actual_delay = std::max(1, static_cast<int>(target_delay));

					while (std::chrono::duration_cast<std::chrono::milliseconds>(
							   std::chrono::steady_clock::now() - start
						   )
							   .count()
						   < time_per_frame)
						if (cv::waitKey(1) == 27)
							break;

					// if (cv::waitKey(actual_delay) == 27)
					// 	break;
				}
			}

		private:
			MidiIn					   _in;
			std::filesystem::path	   _video_path;
			std::atomic<bool>		   _is_playing	 = false;
			std::atomic<bool>		   _should_reset = false;
			std::optional<std::string> _title;
		};
	}  // namespace details::midi_video_player_app

	using details::midi_video_player_app::MidiVideoPlayerApp;
}  // namespace dvdbchar