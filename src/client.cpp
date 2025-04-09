#include <iostream>
#include <string>
#include <cpprest/http_client.h>
#include <cpprest/uri.h>
#include <cpprest/json.h>
#include <thread>
#include <chrono>

using namespace web;
using namespace web::http;
using namespace web::http::client;

std::wstring w_server_ip = L"http://localhost:8000/"; // Default server IP, don't forget trailing slash

// Convert to UTF-8 string for logging
std::string s_server_ip = utility::conversions::to_utf8string(w_server_ip);

// Handles keep-alive with the server
void keep_alive(const std::wstring& id) {
    try {
        uri endpoint(w_server_ip + L"keep-alive/" + id);
        http_client client(endpoint);

        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            client.request(methods::GET).then([](http_response response) {
                if (response.status_code() == status_codes::OK) {
                    std::cout << "Keep-alive successful.\n";
                } else {
                    std::cerr << "Keep-alive failed with status code: " << response.status_code() << "\n";
                }
            }).wait();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in keep-alive: " << e.what() << std::endl;
    }
}

int main() {
    try {
        // Connect to /get-id to retrieve a unique ID
        uri endpoint(w_server_ip + L"get-id");
        http_client client(endpoint);
        
        std::cout << "Connecting to server at: " << s_server_ip << std::endl;
        std::wstring client_id;
        client.request(methods::GET).then([&client_id](http_response response) {
            if (response.status_code() == status_codes::OK) {
                return response.extract_string();
            } else {
                throw std::runtime_error("Failed to get ID with status code: " + std::to_string(response.status_code()));
            }
        }).then([&client_id](const utility::string_t& body) {
            client_id = utility::conversions::to_string_t(body);
            std::cout << "Server assigned ID: " << utility::conversions::to_utf8string(client_id) << std::endl;
        }).wait();

        // Start the keep-alive thread
        std::thread(keep_alive, client_id).detach();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    // Keep the main loop running
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}