#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <random>
#include <chrono>
#include <re2/re2.h>

namespace beast = boost::beast;         // From <boost/beast.hpp>
namespace http = beast::http;          // From <boost/beast/http.hpp>
namespace net = boost::asio;           // From <boost/asio.hpp>
using tcp = net::ip::tcp;              // From <boost/asio/ip/tcp.hpp>

std::unordered_map<std::string, std::pair<std::string, std::chrono::steady_clock::time_point>> clients;
std::mutex clients_mutex;

// Generates a random 8-character ID for clients
std::string generate_id() {
    static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);
    std::string id(8, '\0');
    for (auto& c : id) c = charset[dist(rng)];
    return id;
}

// Cleans up clients that have not been active for 120 seconds
void cleanup_clients() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(clients_mutex);
        for (auto it = clients.begin(); it != clients.end();) {
            if (std::chrono::duration_cast<std::chrono::seconds>(now - it->second.second).count() > 120) {
                it = clients.erase(it);
            } else {
                ++it;
            }
        }
    }
}

// Logs web requests to access.log
void log_request(const http::request<http::string_body>& req, const http::response<http::string_body>& res, const std::string& client_ip) {
    std::ofstream log_file("access.log", std::ios::app);
    if (log_file.is_open()) {
        auto t = std::time(nullptr);
        std::tm tm;
        localtime_s(&tm, &t);
        std::ostringstream time_stream;
        char time_buffer[20];
        std::strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", &tm);
        time_stream << time_buffer;
        log_file << client_ip << " - " << time_stream.str() << " - " 
             << req.method_string() << " " << req.target() << " " 
             << req.version() << " " << static_cast<int>(res.result_int()) 
             << "\n";
        log_file.close();
    } else {
        std::cerr << "Error: Unable to open access.log for writing.\n";
    }
}

// Handles requests
void handle_request(http::request<http::string_body> const& req, http::response<http::string_body>& res, tcp::socket& socket) {
    std::string client_ip = socket.remote_endpoint().address().to_string();
    log_request(req, res, client_ip); // Log the request
    // Initial ID generation
    if (req.target() == "/get-id") {
        std::string id;
        do {
            id = generate_id();
            std::cout << "Generated ID " << id << "for IP " << client_ip << "\n"; // Debugging output, can be removed later
        } while (clients.find(id) != clients.end());

        {
            // Lock the clients map to ensure thread safety
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients[id] = {client_ip, std::chrono::steady_clock::now()};
        }

        res.result(http::status::ok);
        res.set(http::field::content_type, "text/plain");
        res.body() = id;
    } else if (req.target().starts_with("/keep-alive")) {
        auto id = std::string(req.target().substr(12)); // Remove first 12 characters to extract ID from "keep-alive/<id>"
        std::cout << "Keep-alive request for ID: " << id << "from " << client_ip <<"\n"; // Could probably be removed or put in a verbose mode/log file
        std::lock_guard<std::mutex> lock(clients_mutex);
        // Respond with Available when ID is found, otherwise 401
        if (clients.find(id) != clients.end()) {
            clients[id].second = std::chrono::steady_clock::now();
            res.result(http::status::ok);
            res.set(http::field::content_type, "text/plain");
            res.body() = "OK";
            std::cout << "Keep-alive from" << id << "\n";
        } else {
            res.result(http::status::unauthorized);
            res.set(http::field::content_type, "text/plain");
            res.body() = "Unauthorized";
            std::cout << "Attempt at keep-alive for ID: " << id << "from " << client_ip << " failed" << "\n";
        }
    // Basic /health endpoint, not used atp
    } else if (req.target() == "/health") {
        res.result(http::status::ok);
        res.set(http::field::content_type, "text/plain");
        res.body() = "OK";
    // General 404 for any other requests
    } else {
        res.result(http::status::not_found);
        res.set(http::field::content_type, "text/plain");
        res.body() = "404";
    }
    res.prepare_payload();
}

void session(tcp::socket socket) {
    try {
        beast::flat_buffer buffer;
        http::request<http::string_body> req;
        http::response<http::string_body> res; // Declare and initialize res
        http::read(socket, buffer, req);

        handle_request(req, res, socket);

        http::write(socket, res);
    } catch (std::exception const& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
}

// CLI interface
void cli_interface() {
    while (true) {
        std::string command;
        std::getline(std::cin, command);

        // clients command

        if (re2::RE2::FullMatch(command, "clients(?:\\s\\S*)?")) { // Match clients command using re2
            std::string action;
            if (re2::RE2::FullMatch(command, "clients\\s([a-zA-Z]*)", &action)) { // Match 'clients <action>' command using re2
                std::lock_guard<std::mutex> lock(clients_mutex); // Lock the clients map
                if (action == "list") {
                    std::cout << "Active clients:\n";
                    for (const auto& client : clients) {
                        std::cout << "ID: " << client.first << ", IP: " << client.second.first << "\n"; }
                } else {
                    std::cout << "Unknown action: " << action << "\nType 'help clients' for usage\n"; // Handle unknown action
                }
            // Match errors
            } else if (re2::RE2::FullMatch(command, "clients")) {
                std::cout << "Missing action\nType 'help clients' for usage\n"; // Handle missing action
            } else {
                std::cout << "Unknown error, please report the issue. Type 'help issue' for information.\nType 'help clients' for usage\n"; // Handle unknown error in command parsing
            }

        // client command

        } else if (re2::RE2::FullMatch(command, "client(?:\\s\\S*)?")) { // Match client command using re2
            std::string id, action;
            if (re2::RE2::FullMatch(command, "client\\s([a-zA-Z0-9]{8})\\s([a-zA-Z]*)", &id, &action)) { // Match 'client <id> <action>' command using re2
                std::lock_guard<std::mutex> lock(clients_mutex); // Lock the clients map
                auto idPointer = clients.find(id);
                if (idPointer != clients.end()) { // If the ID exists, continue to parse command
                    if (action == "remove") { // Match 'client <id> remove' command using re2
                        clients.erase(idPointer); // Remove the client from the map
                        std::cout << "Client with ID " << id << " removed\n";
            // Handle issues IMPORTANT: For some reason, client <ID> <something not in matched> error handling is not working, TBF
                    } else {
                        std::cout << "Unknown action: " << action << "\nType 'help client' for usage\n"; // Handle unknown action
                    }
                } else {
                    std::cout << "Client with ID " << id << " not found\nType 'help client' for usage\n"; // Handle client not found
                }
            } else if (re2::RE2::FullMatch(command, "client\\s[a-zA-Z0-9]{8}")) {
                std::cout << "Missing action\nType 'help client' for usage\n"; // Handle missing action
            } else if (re2::RE2::FullMatch(command, "client")) {
                std::cout << "Missing arguments\nType 'help client' for usage\n"; // Handle missing ID and action
            } else if (re2::RE2::FullMatch(command, "client\\s(?:[a-zA-Z0-9]){1,7}")) {
                std::cout << "ID too short\nType 'help client' for usage\n"; // Handle missing ID and action
            } else if (re2::RE2::FullMatch(command, "client\\s(?:[a-zA-Z0-9]){9,}")) {
                std::cout << "ID too long\nType 'help client' for usage\n"; // Handle missing ID and action
            } else if (re2::RE2::FullMatch(command, "client\\s(?:[a-zA-Z0-9]){9,}")) {
                std::cout << "ID too long\nType 'help client' for usage\n"; // Handle missing ID and action
            } else {
                std::cout << "Unknown error, please report the issue. Type 'help issue' for information.\nType 'help client' for usage\n"; // Handle unknown error in command parsing
            }

            // help command

        } else if (re2::RE2::FullMatch(command, "help(?:\\s\\S*)?")) {
            std::cout << "Not implemented yet\n"; // Placeholder for help command
        }
    }
}


int main() {
    try {
        const int server_port = 8000;
        net::io_context ioc;

        tcp::acceptor acceptor(ioc, tcp::endpoint(tcp::v4(), server_port));
        std::cout << "Starting server on port " << server_port << "...\n";

        std::thread(cleanup_clients).detach();
        std::thread(cli_interface).detach();

        while (true) {
            tcp::socket socket(ioc);
            acceptor.accept(socket);
            std::thread(session, std::move(socket)).detach();
        }
    } catch (std::exception const& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}