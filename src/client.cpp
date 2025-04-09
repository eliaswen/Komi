#include <iostream>
#include <string>
#include <cpprest/http_client.h>
#include <cpprest/uri.h>
#include <cpprest/json.h>
#include <thread>
#include <chrono>
#include <map>

using namespace web;
using namespace web::http;
using namespace web::http::client;

std::wstring w_server_ip = L"http://localhost:8000/"; // Default server IP, don't forget trailing slash
int keep_alive_interval = 30; // Default keep-alive interval in seconds

// Convert to UTF-8 string for logging
std::string s_server_ip = utility::conversions::to_utf8string(w_server_ip);

// Function to parse command-line arguments into a map
std::map<std::string, std::string> parse_arguments(int argc, char* argv[]) {
    std::map<std::string, std::string> args;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto delimiter_pos = arg.find('=');
        if (delimiter_pos != std::string::npos) {
            std::string key = arg.substr(0, delimiter_pos);
            std::string value = arg.substr(delimiter_pos + 1);
            args[key] = value;
        }
    }
    return args;
}

// Handles keep-alive with the server
void keep_alive(const std::wstring& id , int keep_alive) {
    try {
        uri endpoint(w_server_ip + L"keep-alive/" + id);
        http_client client(endpoint);

        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(keep_alive));
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

int main(int argc, char* argv[]) {
    try {
        // Parse command-line arguments
        auto args = parse_arguments(argc, argv);

        // Update server IP if provided
        if (args.find("--server-ip") != args.end()) { // Fixed key lookup
            w_server_ip = utility::conversions::to_string_t(args["--server-ip"]); // Fixed key usage
            s_server_ip = args["--server-ip"]; // Fixed key usage
        }

        // Update keep alive interval if provided
        if (args.find("--keep-alive") != args.end()) {
            keep_alive_interval = std::stoi(args["--keep-alive"]);
            if (keep_alive_interval <= 0) {
                throw std::invalid_argument("Keep-alive interval must be greater than 0.");
            }
        }

        std::cout << "Using server IP: " << s_server_ip << std::endl;

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
        std::thread(keep_alive, client_id, keep_alive_interval).detach();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    // Keep the main loop running
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
