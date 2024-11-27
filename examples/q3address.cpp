#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <sstream>

#define SERVER_IP "192.241.238.177"
#define SERVER_PORT 27950
#define BUFFER_SIZE 1024

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

int main()
{
    int sockfd;
    sockaddr_in server_addr{};
    char buffer[BUFFER_SIZE];

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("Socket creation failed");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0)
    {
        perror("Invalid server address");
        close(sockfd);
        return 1;
    }

    // Send a message to the server
    std::string message = "\xFF\xFF\xFF\xFFgetservers 68 empty";
    if (sendto(sockfd, message.c_str(), message.size(), 0, (const sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Send failed");
        close(sockfd);
        return 1;
    }
    std::cout << "Message sent to server: \"" << message << "\"" << std::endl;

    socklen_t server_addr_len = sizeof(server_addr);
    int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (sockaddr *)&server_addr, &server_addr_len);
    if (n < 0)
    {
        perror("Receive failed");
    }
    else
    {
        buffer[n] = '\0';
        std::vector<std::pair<std::string, int>> addresses = get_ip_port(buffer);

        for (const auto &address : addresses)
        {
            std::cout << "IP: " << address.first << ", Port: " << address.second << std::endl;
        }
    }

    close(sockfd);
    return 0;
}
