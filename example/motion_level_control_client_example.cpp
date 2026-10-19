#include "example_options.h"

#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
#endif

namespace {

constexpr uint16_t kDefaultServerPort = 50051;

struct ClientOptions {
    std::string server_ip = "127.0.0.1";
    uint16_t server_port = kDefaultServerPort;
    bool help_requested = false;
    std::vector<std::string> command;
};

void closeSocket(SocketHandle socket) {
    if (socket == kInvalidSocket) {
        return;
    }
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

bool initializeSockets() {
#ifdef _WIN32
    WSADATA data{};
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
#else
    return true;
#endif
}

void cleanupSockets() {
#ifdef _WIN32
    WSACleanup();
#endif
}

void printUsage(const char* program) {
    std::cout
        << "Usage: " << program << " [options] [--] [command...]\n"
        << "\n"
        << "Options:\n"
        << "  --server-ip IP             Motion server IP (default: 127.0.0.1)\n"
        << "  --server-port PORT         Motion server port (default: " << kDefaultServerPort << ")\n"
        << "  -h, --help                 Show this help\n"
        << "\n"
        << "Commands:\n"
        << "  help                         List all commands and gait/jump choices\n"
        << "  zero                         Send the zero-position flag\n"
        << "  stand                        Stand up\n"
        << "  tumbler                      Request self-righting recovery while lying down\n"
        << "  sit                          Sit down\n"
        << "  charging-sit                 Request charging sit-down\n"
        << "  damping                      Enter damping mode\n"
        << "  stop                         Set velocity to zero and disable velocity control\n"
        << "  status                       Query connection, SN, model, control mode,\n"
        << "                               current/previous motion state, gait, max velocity,\n"
        << "                               battery, chargers, and hard estop\n"
        << "  velocity X Y YAW             Set forward/lateral velocity (m/s) and yaw rate\n"
        << "                               (rad/s), and enable velocity control\n"
        << "  velocity-control on|off      Enable or disable velocity control\n"
        << "  gait NAME                    Select a gait:\n"
        << "                               walk, running, bipedal, inv_bipedal, pronk,\n"
        << "                               pace, bound, left_flip, right_flip\n"
        << "  jump up|front|back|left|right Select a jump direction (BPX)\n"
        << "  quit, shutdown               Shut down the server\n"
        << "\n"
        << "Available actions depend on the robot model.\n"
        << "Without a command, enter interactive mode; type help to list commands.\n"
        << "Ctrl-D (Windows: Ctrl-Z then Enter) disconnects the client; quit stops the server.\n"
        << "\nExamples:\n"
        << "  " << program << " stand\n"
        << "  " << program << " charging-sit\n"
        << "  " << program << " velocity 0.2 0 0\n"
        << "  " << program << " gait walk\n"
        << "  " << program << " jump up\n"
        << "  " << program << " status\n";
}

bool parseOptions(int argc, char** argv, ClientOptions* options) {
    if (!options) {
        return false;
    }

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            options->help_requested = true;
            return true;
        }
        if (arg == "--server-ip") {
            if (!bpx_sdk::example::requireValue(argc, argv, i)) return false;
            options->server_ip = argv[++i];
            continue;
        }
        if (arg == "--server-port") {
            if (!bpx_sdk::example::requireValue(argc, argv, i) ||
                !bpx_sdk::example::parseUint16(argv[++i], &options->server_port) ||
                options->server_port == 0) {
                std::cerr << arg << " expects a positive port in range 1..65535" << std::endl;
                return false;
            }
            continue;
        }
        if (arg == "--") {
            for (++i; i < argc; ++i) {
                options->command.emplace_back(argv[i]);
            }
            break;
        }
        options->command.emplace_back(arg);
    }

    return true;
}

std::string joinCommand(const std::vector<std::string>& words) {
    std::ostringstream output;
    for (std::size_t i = 0; i < words.size(); ++i) {
        if (i != 0) {
            output << ' ';
        }
        output << words[i];
    }
    return output.str();
}

bool sendAll(SocketHandle socket, const std::string& text) {
    const char* data = text.data();
    std::size_t remaining = text.size();
    while (remaining > 0) {
#ifdef _WIN32
        const int sent = send(socket, data, static_cast<int>(remaining), 0);
#else
        #ifdef MSG_NOSIGNAL
        const ssize_t sent = send(socket, data, remaining, MSG_NOSIGNAL);
#else
        const ssize_t sent = send(socket, data, remaining, 0);
#endif
#endif
        if (sent <= 0) {
            return false;
        }
        data += sent;
        remaining -= static_cast<std::size_t>(sent);
    }
    return true;
}

bool readLine(SocketHandle socket, std::string* line) {
    if (!line) {
        return false;
    }
    line->clear();
    char ch = '\0';
    while (true) {
#ifdef _WIN32
        const int received = recv(socket, &ch, 1, 0);
#else
        const ssize_t received = recv(socket, &ch, 1, 0);
#endif
        if (received <= 0) {
            return !line->empty();
        }
        if (ch == '\n') {
            return true;
        }
        if (ch != '\r') {
            line->push_back(ch);
        }
    }
}

SocketHandle connectToServer(const std::string& server_ip, uint16_t server_port) {
    SocketHandle client = socket(AF_INET, SOCK_STREAM, 0);
    if (client == kInvalidSocket) {
        return kInvalidSocket;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip.c_str(), &address.sin_addr) != 1) {
        std::cerr << "invalid server ip: " << server_ip << std::endl;
        closeSocket(client);
        return kInvalidSocket;
    }

    if (connect(client, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        closeSocket(client);
        return kInvalidSocket;
    }
    return client;
}

bool sendCommand(SocketHandle socket, const std::string& command) {
    if (!sendAll(socket, command + "\n")) {
        std::cerr << "failed to send command" << std::endl;
        return false;
    }
    std::string response;
    if (!readLine(socket, &response)) {
        std::cerr << "server closed the connection" << std::endl;
        return false;
    }
    std::cout << response << std::endl;
    return response.rfind("OK", 0) == 0;
}

}  // namespace

int main(int argc, char** argv) {
    ClientOptions options;
    if (!parseOptions(argc, argv, &options)) {
        return 1;
    }
    if (options.help_requested) {
        return 0;
    }

    if (!initializeSockets()) {
        std::cerr << "failed to initialize sockets" << std::endl;
        return 1;
    }

    SocketHandle client = connectToServer(options.server_ip, options.server_port);
    if (client == kInvalidSocket) {
        std::cerr << "failed to connect to " << options.server_ip << ":" << options.server_port << std::endl;
        cleanupSockets();
        return 1;
    }

    std::string greeting;
    if (readLine(client, &greeting)) {
        std::cout << greeting << std::endl;
    }

    bool ok = true;
    if (!options.command.empty()) {
        ok = sendCommand(client, joinCommand(options.command));
    } else {
        std::cout << "Enter commands, help for command list, Ctrl-D to disconnect." << std::endl;
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) {
                continue;
            }
            if (!sendCommand(client, line)) {
                ok = false;
                break;
            }
            if (line == "quit" || line == "shutdown") {
                break;
            }
        }
    }

    closeSocket(client);
    cleanupSockets();
    return ok ? 0 : 1;
}
