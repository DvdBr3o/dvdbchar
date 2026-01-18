#pragma once

#include <asio.hpp>
#include <spdlog/spdlog.h>
#include <osc/OscReceivedElements.h>

#include <stdexcept>

namespace dvdbchar {
	class OscServer {
	public:
		using Handler = std::function<void(const osc::ReceivedPacket&)>;

		struct Spec {
			asio::io_context& ctx;
			uint32_t		  port;
			Handler			  handle_osc = [](const osc::ReceivedPacket&) {};
		};

	public:
		OscServer(const Spec& spec) :
			_socket(spec.ctx, asio::ip::udp::endpoint(asio::ip::udp::v4(), spec.port)),
			_handler(spec.handle_osc) {
			_start_recv();
		}

	private:
		void _start_recv() {
			_socket.async_receive_from(
				asio::buffer(_buffer),
				_endpoint,
				[this](asio::error_code ec, std::size_t size) {
					if (!ec) {
						spdlog::info("received:\n{}", std::string_view(_buffer.data(), size));
						_handler(osc::ReceivedPacket(_buffer.data(), size));
						_start_recv();
					} else {
						spdlog::error("error: {}", ec.message());
						throw std::runtime_error { std::format("error: {}", ec.message()) };
					}
				}
			);
		}

	private:
		asio::ip::udp::socket	_socket;
		asio::ip::udp::endpoint _endpoint;
		std::array<char, 65536> _buffer;
		Handler					_handler;
	};

	class OscService {
	public:
		struct Spec {
			std::uint32_t	   port;
			OscServer::Handler handler;
		};

	public:
		OscService(const Spec& spec) : _port(spec.port), _handler(spec.handler) {}

	public:
		auto serve() {
			spdlog::info("start serving on port {}", _port);
			OscServer server({ _io, _port, _handler });
			_io.run();
		}

		auto stop() { _io.stop(); }

	private:
		asio::io_context   _io;
		uint32_t		   _port;
		OscServer::Handler _handler;
	};

}  // namespace dvdbchar
