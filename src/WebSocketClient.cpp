#include "WebSocketClient.hpp"
#include <boost/beast/websocket.hpp>
#include <boost/beast/core.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <string>


WebSocketClient::WebSocketClient(const std::string& url, const std::string& host, const std::string& port)
    : url_(url), host_(host), port_(port),
      strand_(io_context_.get_executor()),
      resolver_(io_context_) {
    if (port_ == "443") {
        ssl_context_ = std::make_unique<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);
        ssl_context_->set_verify_mode(boost::asio::ssl::verify_none);
        ssl_context_->set_verify_callback(boost::asio::ssl::rfc2818_verification(host));
		ssl_context_->set_default_verify_paths();
        ws_ssl_ = std::make_unique<boost::beast::websocket::stream<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>>(io_context_, *ssl_context_);
        ws_ = nullptr;
    } else {
        ws_ = std::make_unique<boost::beast::websocket::stream<boost::asio::ip::tcp::socket>>(io_context_);
        ws_ssl_ = nullptr;
    }
}

void WebSocketClient::connect() {
    try {
        std::cout << "Host: " << host_ << ", Port: " << port_ << std::endl;
        std::string path = host_.find_first_of("/") != std::string::npos ? host_.substr(host_.find_first_of("/")) : "/";
        std::cout << "Path: " << path << std::endl;
        auto const results = resolver_.resolve(host_, port_);
        
        if (ws_ssl_) {
            boost::asio::connect(ws_ssl_->next_layer().next_layer(), results.begin(), results.end());
            ws_ssl_->next_layer().handshake(boost::asio::ssl::stream_base::client);
            ws_ssl_->set_option(websocket::stream_base::decorator(
                [](websocket::request_type& req)
                {
                    req.set(http::field::user_agent,
                        std::string(BOOST_BEAST_VERSION_STRING) +
                            " websocket-client-coro");
                    req.set(http::field::connection, "upgrade");
                }));
            ws_ssl_->handshake(host_, path);
        } else {
            boost::asio::connect(ws_->next_layer(), results.begin(), results.end());
            ws_->set_option(websocket::stream_base::decorator(
                [](websocket::request_type& req)
                {
                    req.set(http::field::user_agent,
                        std::string(BOOST_BEAST_VERSION_STRING) +
                            " websocket-client-coro");
                }));
            ws_->handshake(host_, path);
        }
        std::cout << "Connected to " << url_ << std::endl;
    } catch (const boost::system::system_error& e) {
        std::cerr << "Connection error: " << e.what() << std::endl;
        std::cerr << "Error code: " << e.code() << std::endl;
        if (e.code() == boost::asio::error::host_not_found) {
            std::cerr << "Host not found." << std::endl;
        } else if (e.code() == boost::asio::ssl::error::stream_truncated) {
            std::cerr << "SSL stream truncated." << std::endl;
        }else {
            std::cerr << "Other error: " << e.code().message() << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Connection error: " << e.what() << std::endl;
    }
}

void WebSocketClient::send(const std::string& message) {
    try {
        if (ws_ssl_) {
            boost::asio::async_write(ws_ssl_->next_layer().next_layer(), boost::asio::buffer(message),
                boost::asio::bind_executor(strand_, std::bind(&WebSocketClient::on_send, this, std::placeholders::_1, std::placeholders::_2)));
        } else {
            boost::asio::async_write(ws_->next_layer(), boost::asio::buffer(message),
                boost::asio::bind_executor(strand_, std::bind(&WebSocketClient::on_send, this, std::placeholders::_1, std::placeholders::_2)));
        }
    } catch (const std::exception& e) {
        std::cerr << "Error sending message: " << e.what() << std::endl;
    }
}

void WebSocketClient::on_send(boost::system::error_code ec, std::size_t bytes_transferred) {
    if (ec) {
        std::cerr << "Error sending message: " << ec.message() << std::endl;
    } else {
        std::cout << "Sent message: " << bytes_transferred << " bytes" << std::endl;
    }
}

void WebSocketClient::receive() {
    boost::beast::flat_buffer buffer;
    if (ws_ssl_ != nullptr)
        receive_ssl(buffer);
    else if (ws_ != nullptr)
        receive_non_ssl(buffer);
    else
        std::cerr << "No WebSocket connection available!" << std::endl;
}

void WebSocketClient::receive_ssl(boost::beast::flat_buffer& buffer) {
    boost::asio::async_read(
        ws_ssl_->next_layer(),
        buffer,
        [this, &buffer](boost::system::error_code ec, std::size_t bytes_transferred) {
            if (ec) {
                std::cerr << "Error during receive (SSL): " << ec.message() << std::endl;
                return;
            }
            std::cout << "Received message (SSL): " << boost::beast::buffers_to_string(buffer.data()) << std::endl;
            buffer.consume(bytes_transferred);

            receive();
        });
}

void WebSocketClient::receive_non_ssl(boost::beast::flat_buffer& buffer) {
    boost::asio::async_read(
    ws_->next_layer(),
        buffer,
        [this, &buffer](boost::system::error_code ec, std::size_t bytes_transferred) {
            if (ec) {
                std::cerr << "Error during receive (Non-SSL): " << ec.message() << std::endl;
                return;
            }
            std::cout << "Received message (Non-SSL): " << boost::beast::buffers_to_string(buffer.data()) << std::endl;

            buffer.consume(bytes_transferred);

            receive();
        });
}


void WebSocketClient::on_receive(boost::system::error_code ec, std::size_t bytes_transferred) {
    if (ec) {
        std::cerr << "Error receiving message: " << ec.message() << std::endl;
    } else {
        std::cout << "Received message: " << bytes_transferred << " bytes" << std::endl;
    }
}

void WebSocketClient::close() {
    try {
        ws_->close(boost::beast::websocket::close_code::normal);
        std::cout << "Connection closed." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error closing connection: " << e.what() << std::endl;
    }
}

void WebSocketClient::run() {
    try {
        std::cout << "Running io_context..." << std::endl;
        io_context_.run();
    } catch (const std::exception& e) {
        std::cerr << "Error running io_context: " << e.what() << std::endl;
    }
}

void WebSocketClient::send_ping() {
    if (ws_ssl_) {
        std::cout << "Sending ping (SSL)" << std::endl;
        ws_ssl_->ping(boost::beast::websocket::ping_data{});
    } else if (ws_) {
        std::cout << "Sending ping (Non-SSL)" << std::endl;
        ws_->ping(boost::beast::websocket::ping_data{});
    }
}
