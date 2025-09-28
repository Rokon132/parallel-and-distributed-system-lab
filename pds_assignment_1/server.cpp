
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <csignal>
#include <fstream>
#include <iostream>
#include <mutex>
#include <numeric>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

struct Joke { std::string setup, punch; };

// ----------------------- globals & helpers -----------------------
static std::atomic<bool> g_running{true};
static std::atomic<int>  g_active{0};          // clients currently being served
static std::atomic<int>  g_served_sessions{0}; // total finished sessions
static int  g_expected_sessions = -1;          // optional: auto-exit after N sessions
static int  g_idle_exit_ms = -1;               // optional: auto-exit when idle this long
static std::mutex g_logmx;

static void sigint_handler(int){ g_running = false; }

static std::string trim(const std::string& s){
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// send an entire string
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

// receive one line (up to '\n'), returns nullopt on close/error
static std::optional<std::string> recv_line(int fd){
    std::string buf; buf.reserve(128);
    char c;
    while (true){
        ssize_t n = ::recv(fd, &c, 1, 0);
        if (n <= 0) return std::nullopt;
        if (c == '\n') break;
        buf.push_back(c);
        if (buf.size() > 4096) return std::nullopt; // guard
    }
    return buf;
}

// Normalize: trim, lowercase, collapse spaces, curly apostrophe → ASCII '
static std::string normalize(const std::string& raw){
    std::string s = trim(raw);

    // map U+2018/U+2019 to ASCII '
    std::string tmp; tmp.reserve(s.size());
    for (size_t i=0; i<s.size(); ++i){
        unsigned char a = s[i];
        if (i+2 < s.size() && a==0xE2 && (unsigned char)s[i+1]==0x80 &&
           ((unsigned char)s[i+2]==0x98 || (unsigned char)s[i+2]==0x99)) {
            tmp.push_back('\''); i+=2; continue;
        }
        tmp.push_back(s[i]);
    }

    // collapse spaces
    std::string collapsed; collapsed.reserve(tmp.size());
    bool prev_space=false;
    for(char c: tmp){
        if (std::isspace((unsigned char)c)){
            if (!prev_space) collapsed.push_back(' ');
            prev_space = true;
        } else { collapsed.push_back(c); prev_space = false; }
    }

    std::transform(collapsed.begin(), collapsed.end(), collapsed.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return trim(collapsed);
}

static bool is_whos_there(const std::string& in){
    std::string s = normalize(in);
    return (s == "who's there?" || s == "whos there?");
}
static bool is_setup_who(const std::string& in, const std::string& setup){
    std::string s = normalize(in);
    std::string need = normalize(setup) + " who?";
    return s == need;
}
static bool is_yes(const std::string& in){
    std::string s = normalize(in);
    return (s == "y" || s == "yes");
}

// ----------------------- jokes I/O -----------------------
static std::vector<Joke> load_jokes(const std::string& path){
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Failed to open jokes file: " + path);

    std::vector<Joke> jokes;
    std::string line;
    while (std::getline(f, line)){
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        auto p = line.find('|');
        if (p == std::string::npos) continue;   // enforce Setup|Punchline
        Joke j{ trim(line.substr(0, p)), trim(line.substr(p+1)) };
        if (!j.setup.empty() && !j.punch.empty())
            jokes.push_back(std::move(j));
    }
    if (jokes.size() < 15)
        throw std::runtime_error("Need at least 15 jokes in jokes.txt (have " + std::to_string(jokes.size()) + ")");
    return jokes;
}

// ----------------------- client worker -----------------------
enum class State { WAIT_WHO, WAIT_WHO_SETUP, WAIT_CONTINUE };

static void client_worker(int cfd, sockaddr_in cli, const std::vector<Joke>& jokes){
    g_active++;
    {
        std::lock_guard<std::mutex> lk(g_logmx);
        std::cout << "[+] Client " << inet_ntoa(cli.sin_addr) << ":" << ntohs(cli.sin_port)
                  << " connected. (active=" << g_active.load() << ")\n";
    }

    // Per-client random order; no repetition within session.
    std::vector<int> order(jokes.size());
    std::iota(order.begin(), order.end(), 0);
    std::mt19937 rng(std::random_device{}());
    std::shuffle(order.begin(), order.end(), rng);

    size_t idx = 0;
    State st = State::WAIT_WHO;

    auto restart_joke = [&](const std::string& correction){
        send_all(cfd, "Server: " + correction + " Let’s try again.\n");
        send_all(cfd, "Server: Knock knock!\n");
        st = State::WAIT_WHO;
    };

    // Start first prompt
    if (!send_all(cfd, "Server: Knock knock!\n")) goto done;

    while (g_running && idx < order.size()){
        const Joke& J = jokes[ order[idx] ];

        auto line = recv_line(cfd);
        if (!line) break;             // client closed
        std::string in = trim(*line);
        if (in.empty()) continue;     // ignore empty lines

        if (st == State::WAIT_WHO){
            if (!is_whos_there(in)){
                restart_joke("You are supposed to say, \"Who’s there?\"");
                continue;
            }
            if (!send_all(cfd, "Server: " + J.setup + ".\n")) break;
            st = State::WAIT_WHO_SETUP;
            continue;
        }

        if (st == State::WAIT_WHO_SETUP){
            if (!is_setup_who(in, J.setup)){
                std::ostringstream ss;
                ss << "You are supposed to say, \"" << J.setup << " who?\"";
                restart_joke(ss.str());
                continue;
            }
            if (!send_all(cfd, "Server: " + J.punch + "\n")) break;
            if (!send_all(cfd, "Server: Would you like to listen to another? (Y/N)\n")) break;
            st = State::WAIT_CONTINUE;
            continue;
        }

        if (st == State::WAIT_CONTINUE){
            if (is_yes(in)){
                idx++;
                if (idx >= order.size()) break;      // out of jokes for this client
                if (!send_all(cfd, "Server: Knock knock!\n")) break;
                st = State::WAIT_WHO;
            } else {
                // user chose not to continue
                goto done;
            }
            continue;
        }
    }

    if (idx >= order.size()){
        // client has heard all jokes this session
        send_all(cfd, "Server: I have no more jokes to tell.\n");
    }

done:
    ::close(cfd);
    g_active--;
    g_served_sessions++;
    {
        std::lock_guard<std::mutex> lk(g_logmx);
        std::cout << "[-] Client finished. active=" << g_active.load()
                  << "  totalServed=" << g_served_sessions.load() << "\n";
    }
}

// ----------------------- main (accept loop) -----------------------
int main(int argc, char** argv){
    if (argc < 3){
        std::cerr << "Usage: " << argv[0]
                  << " <bind_ip> <port> [--jokes jokes.txt] [--expected N] [--idle-exit-ms MS]\n";
        return 1;
    }
    std::string bind_ip = argv[1];
    int port = std::stoi(argv[2]);
    std::string jokes_path = "jokes.txt";

    for (int i=3; i<argc; ++i){
        std::string a = argv[i];
        if (a == "--jokes" && i+1 < argc)         jokes_path = argv[++i];
        else if (a == "--expected" && i+1 < argc) g_expected_sessions = std::stoi(argv[++i]);
        else if (a == "--idle-exit-ms" && i+1<argc) g_idle_exit_ms = std::stoi(argv[++i]);
    }

    auto jokes = load_jokes(jokes_path);

    std::signal(SIGINT, sigint_handler);

    int srv = ::socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0){ perror("socket"); return 1; }
    int opt=1; setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(port);
    if (inet_pton(AF_INET, bind_ip.c_str(), &addr.sin_addr) <= 0){
        perror("inet_pton"); return 1;
    }
    if (bind(srv, (sockaddr*)&addr, sizeof(addr)) < 0){ perror("bind"); return 1; }
    if (listen(srv, 64) < 0){ perror("listen"); return 1; }

    {
        std::lock_guard<std::mutex> lk(g_logmx);
        std::cout << "[*] Server listening on " << bind_ip << ":" << port
                  << "  (jokes=" << jokes.size() << ")\n";
        if (g_expected_sessions >= 0)
            std::cout << "[*] Will exit after serving " << g_expected_sessions << " client(s).\n";
        if (g_idle_exit_ms > 0)
            std::cout << "[*] Will exit when idle (no clients) for " << g_idle_exit_ms << " ms.\n";
        std::cout << "[*] Press Ctrl+C to stop.\n";
    }

    std::vector<std::thread> workers;
    int idle_ms = 0;

    while (g_running){
        // Exit conditions for demos (both optional)
        if (g_expected_sessions >= 0 && g_served_sessions.load() >= g_expected_sessions) break;
        if (g_idle_exit_ms > 0 && g_active.load() == 0){
            if (idle_ms >= g_idle_exit_ms) break;
        } else {
            idle_ms = 0;
        }

        fd_set rfds; FD_ZERO(&rfds); FD_SET(srv, &rfds);
        timeval tv{0, 200000}; // 200 ms tick
        int rv = select(srv+1, &rfds, nullptr, nullptr, &tv);
        if (rv > 0){
            sockaddr_in cli{}; socklen_t cl = sizeof(cli);
            int cfd = accept(srv, (sockaddr*)&cli, &cl);
            if (cfd >= 0){
                workers.emplace_back(client_worker, cfd, cli, std::cref(jokes));
            } else if (errno != EINTR){
                perror("accept");
            }
        } else {
            // timeout tick for idle counter
            if (g_idle_exit_ms > 0 && g_active.load() == 0) idle_ms += 200;
        }
    }

    for (auto& t : workers) if (t.joinable()) t.join();
    ::close(srv);
    std::cout << "[*] Server terminated.\n";
    return 0;
}
