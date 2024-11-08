#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <string>
#include <memory>

namespace beast = boost::beast;         
namespace http = beast::http;           
namespace websocket = beast::websocket; 
namespace net = boost::asio;            
using tcp = boost::asio::ip::tcp;       

class WebSocketClient {
public:
    WebSocketClient(const std::string& url, const std::string& host, const std::string& port);
    void connect();
    void send(const std::string& message);
    void receive();
    void close();
    void run();
    void send_ping();

private:
    void on_send(boost::system::error_code ec, std::size_t bytes_transferred);
    void on_receive(boost::system::error_code ec, std::size_t bytes_transferred);
    void receive_ssl(boost::beast::flat_buffer& buffer);
    void receive_non_ssl(boost::beast::flat_buffer& buffer);
    std::string url_;
    std::string host_;
    std::string port_;
    std::string path_;
    net::io_context io_context_;
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
	tcp::resolver resolver_{io_context_};
    std::unique_ptr<boost::asio::ssl::context> ssl_context_;
    std::unique_ptr<boost::beast::websocket::stream<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>> ws_ssl_;
    std::unique_ptr<boost::beast::websocket::stream<boost::asio::ip::tcp::socket>> ws_;

    bool use_ssl_;
};

