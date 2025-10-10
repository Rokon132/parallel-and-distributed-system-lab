
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <optional>
#include <string>
// here is a simple TCP client implementation
static std::string trim(const std::string& s){
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

static std::optional<std::string> recv_line(int fd){
    std::string buf; buf.reserve(128);
    char c;
    while (true){
        ssize_t n = ::recv(fd, &c, 1, 0);
        if (n <= 0) return std::nullopt;
        if (c == '\n') break;
        buf.push_back(c);
        if (buf.size() > 4096) return std::nullopt;
    }
    return buf;
}

static bool send_all(int fd, const std::string& msg){
    const char* p = msg.c_str();
    size_t left = msg.size();
    while (left > 0){
        ssize_t n = ::send(fd, p, left, 0);
        if (n <= 0) return false;
        p += n; left -= n;
    }
    return true;
}

int main(int argc, char** argv){
    if (argc < 3){
        std::cerr << "Usage: " << argv[0] << " <server_ip> <port>\n";
        return 1;
    }
    std::string ip = argv[1];
    int port = std::stoi(argv[2]);

    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0){ perror("socket"); return 1; }

    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0){
        perror("inet_pton"); return 1;
    }
    if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0){
        perror("connect"); return 1;
    }

    std::cout << "[*] Connected to " << ip << ":" << port << "\n";

    while (true){
        auto line = recv_line(fd);
        if (!line) break;                 // server closed
        std::string msg = trim(*line);
        if (msg.empty()) continue;

        std::cout << msg << "\n";

        // server may send final message
        std::string low = msg;
        std::transform(low.begin(), low.end(), low.begin(), ::tolower);
        if (low.find("no more jokes") != std::string::npos) break;

        // prompt user and forward
        std::cout << "You: ";
        std::string in;
        if (!std::getline(std::cin, in)) break;
        if (!send_all(fd, in + "\n")) break;
    }

    ::close(fd);
    std::cout << "[*] Client terminated.\n";
    return 0;
}
