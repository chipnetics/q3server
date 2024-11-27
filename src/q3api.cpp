#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <sstream>
#include <thread>
#include <mutex>
#include "json.hpp"
#include "httplib.h"

using json = nlohmann::json;

#define SERVER_IP "192.241.238.177"
#define SERVER_PORT 27950
#define BUFFER_SIZE 1024
#define UDP_TIMEOUT_SECS 1
#define MAX_THREADS 10
#define LISTEN_PORT 80

// Function to query a server and return JSON string
std::string query_server(const std::string &ip, int port)
{
    int sockfd;
    sockaddr_in server_addr{};
    char buffer[1024];

    //std::cout << "Querying server " << ip << ":" << port << std::endl;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        //perror("Socket creation failed");
        return "";
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0)
    {
        //perror("Invalid server address");
        close(sockfd);
        return "";
    }

    std::string request = "\xFF\xFF\xFF\xFFgetstatus\n";
    if (sendto(sockfd, request.c_str(), request.size(), 0, (const sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        //perror("Send failed");
        close(sockfd);
        return "";
    }

    // Set a timeout for the recvfrom function
    struct timeval tv;
    tv.tv_sec = UDP_TIMEOUT_SECS; // Timeout in seconds
    tv.tv_usec = 0;               // Additional timeout in microseconds

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv)) < 0)
    {
        //perror("Error setting socket options");
        close(sockfd);
        return "";
    }

    socklen_t server_addr_len = sizeof(server_addr);
    int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (sockaddr *)&server_addr, &server_addr_len);
    if (n < 0)
    {
        if (errno == EWOULDBLOCK || errno == EAGAIN)
        {
            //std::cerr << "Receive timed out" << std::endl;
        }
        else
        {
            //perror("Receive failed");
        }
        close(sockfd);
        return "";
    }

    buffer[n] = '\0';
    std::string data(buffer);

    std::vector<std::string> lines;
    std::string delimiter = "\n";
    size_t pos = 0;
    while ((pos = data.find(delimiter)) != std::string::npos)
    {
        lines.push_back(data.substr(0, pos));
        data.erase(0, pos + delimiter.length());
    }

    std::map<std::string, std::string> return_data;
    if (lines.size() > 1)
    {
        std::vector<std::string> configuration;
        std::string config_line = lines[1];
        delimiter = "\\";
        while ((pos = config_line.find(delimiter)) != std::string::npos)
        {
            configuration.push_back(config_line.substr(0, pos));
            config_line.erase(0, pos + delimiter.length());
        }

        for (size_t i = 1; i < configuration.size() - 1; i += 2)
        {
            return_data[configuration[i]] = configuration[i + 1];
        }
    }

    return_data[".0_server_ip"] = ip;
    return_data[".0_server_port"] = std::to_string(port);

    if (lines.size() > 2)
    {
        return_data[".0_player_count"] = std::to_string(lines.size() - 2);
    }

    json j = return_data;

    std::string return_data_str = j.dump();
    // std::cout << "Server data: " << return_data_str << std::endl;

    close(sockfd);
    return return_data_str;
}

// Worker function to process a list of server addresses
std::vector<std::string> worker(const std::vector<std::pair<std::string, int>> &addresses)
{
    std::vector<std::string> combined_json;

    for (const auto &server : addresses)
    {
        std::string ip = server.first;
        int port = server.second;
        std::string json_result = query_server(ip, port);
        if (!json_result.empty())
        {
            combined_json.push_back(json_result);
        }
    }

    return combined_json;
}

std::vector<std::pair<std::string, int>> get_ip_port(const std::string &packet)
{
    std::string p = packet.substr(sizeof("\xFF\xFF\xFF\xFFgetserversResponse") - 1);
    std::vector<std::pair<std::string, int>> addr;

    while (p.size() >= 7)
    {
        int ip_byte_0 = static_cast<unsigned char>(p[1]);
        int ip_byte_1 = static_cast<unsigned char>(p[2]);
        int ip_byte_2 = static_cast<unsigned char>(p[3]);
        int ip_byte_3 = static_cast<unsigned char>(p[4]);
        int port_byte_0 = static_cast<unsigned char>(p[5]);
        int port_byte_1 = static_cast<unsigned char>(p[6]);
        int port = (port_byte_0 << 8) | port_byte_1;

        std::ostringstream ip;
        ip << ip_byte_0 << "." << ip_byte_1 << "." << ip_byte_2 << "." << ip_byte_3;

        addr.emplace_back(ip.str(), port);
        p = p.substr(7);
    }

    return addr;
}

std::vector<std::string> getUdpServers()
{
    int sockfd;
    sockaddr_in server_addr{};
    char buffer[BUFFER_SIZE];

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("Socket creation failed");
        return {};
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0)
    {
        perror("Invalid server address");
        close(sockfd);
        return {};
    }

    // Send a message to the server
    std::string message = "\xFF\xFF\xFF\xFFgetservers 68 empty";
    if (sendto(sockfd, message.c_str(), message.size(), 0, (const sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Send failed");
        close(sockfd);
        return {};
    }

    socklen_t server_addr_len = sizeof(server_addr);
    int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (sockaddr *)&server_addr, &server_addr_len);
    if (n < 0)
    {
        perror("Receive failed");
        close(sockfd);
        return {};
    }

    buffer[n] = '\0';
    std::vector<std::pair<std::string, int>> addresses = get_ip_port(buffer);
    std::cout << "Number of addresses: " << addresses.size() << std::endl;

    // Partition the addresses into MAX_THREADS
    size_t num_threads = MAX_THREADS;
    size_t partition_size = addresses.size() / num_threads;
    std::vector<std::thread> threads;
    std::vector<std::string> combined_results;
    std::mutex results_mutex;

    for (size_t i = 0; i < num_threads; ++i)
    {
        size_t start_index = i * partition_size;
        size_t end_index = (i == num_threads - 1) ? addresses.size() : start_index + partition_size;

        std::vector<std::pair<std::string, int>> partition(addresses.begin() + start_index, addresses.begin() + end_index);

        threads.emplace_back([&combined_results, &results_mutex, partition]()
                             {
            std::vector<std::string> serverinfo = worker(partition);
            std::lock_guard<std::mutex> lock(results_mutex);
            combined_results.insert(combined_results.end(), serverinfo.begin(), serverinfo.end()); });
    }

    for (auto &thread : threads)
    {
        thread.join();
    }

    close(sockfd);
    return combined_results;
}

int main()
{
    httplib::Server server;

    server.Post("/getServers", [](const httplib::Request& req, httplib::Response& res) {
        std::string requestBody = req.body;
        std::cout << "Received POST data: " << requestBody << std::endl;

        std::vector<std::string> serverData = getUdpServers();
        json responseJson = serverData; // Convert the vector to JSON
        std::cout << "Sending JSON response." << std::endl;

        res.set_content(responseJson.dump(), "application/json"); 
        res.set_header("Access-Control-Allow-Origin", "*");
    });

    std::cout << "Server is running on http://localhost:" << LISTEN_PORT << std::endl;
    server.listen("0.0.0.0", LISTEN_PORT);

    
    return 0;
}
