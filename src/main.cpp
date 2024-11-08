#include "WebSocketClient.hpp"
#include "CLI11.hpp"
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char** argv) {
    CLI::App app{"WebSocket Client"};

    std::string url, host, port;
    app.add_option("-u,--url", url, "WebSocket server URL")->required();

    CLI11_PARSE(app, argc, argv);
    if (url.find("wss://") == 0) {
        std::cout << "SSL is used" << std::endl;
        host = url.substr(6);
        host = host.substr(0, host.find("/"));
        port = "443";
    } else if (url.find("ws://") == 0) {
        std::cout << "SSL is not used" << std::endl;
        host = url.substr(5);
        host = host.substr(0, host.find("/"));
        port = "80";
    }

    WebSocketClient client(url, host, port);
    try {
        client.connect();
        client.send_ping();
		std::thread io_thread([&]() { 
			try {
				client.run();
			} catch (const std::exception& e) {
				std::cerr << "Error during io_context run: " << e.what() << std::endl;
			}
		});
        std::string message;
        while (std::getline(std::cin, message)) {
            std::cout << "Sending message: " << message << std::endl;
            client.send(message);
            client.receive();
        }

        client.close();
        io_thread.join();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}

