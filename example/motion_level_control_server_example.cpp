#include "motion_level_control.h"
#include "bpx_sdk_version.h"
#include "example_options.h"
#include "example_feedback.h"
#include <cmath>
#include <ctime>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
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

constexpr uint16_t kDefaultListenPort = 50051;

struct ServerOptions {
    bpx_sdk::example::Options sdk;
    std::string listen_ip = "127.0.0.1";
    uint16_t listen_port = kDefaultListenPort;
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

void printVersion() {
    std::cout
        << BPX_SDK_PROJECT_NAME
        << " version=" << BPX_SDK_PROJECT_VERSION
        << std::endl;
}

void printRobotVersion(const bpx_sdk::RequestRobotState& robot_state) {
    uint16_t robot_major = 0;
    uint16_t robot_minor = 0;
    uint16_t robot_patch = 0;
    uint32_t robot_commit = 0;
    uint32_t robot_build = 0;
    uint32_t robot_build_time = 0;
    if (robot_state.getRobotVersion(&robot_major, &robot_minor, &robot_patch,
                                    &robot_commit, &robot_build,
                                    &robot_build_time)) {
        std::cout << "robot version (queried on connect): "
                  << robot_major << "." << robot_minor << "." << robot_patch
                  << " commit=0x" << std::hex << robot_commit
                  << " build=" << std::dec << robot_build
                  << "T" << std::setw(6) << std::setfill('0') << robot_build_time
                  << std::setfill(' ') << std::endl;
    } else {
        std::cout << "robot version: unknown (not supported by robot)" << std::endl;
    }
}

void printUsage(const char* program) {
    std::cout
        << "Usage: " << program << " [options]\n"
        << "\n"
        << "SDK options:\n"
        << "  --robot-ip IP              Robot IP address (default: " << bpx_sdk::DEFAULT_SERVER_IP << ")\n"
        << "  --state-port PORT          Local UDP port for robot state (default: "
        << bpx_sdk::DEFAULT_CLIENT_ROBOT_STATE_UDP_PORT << ")\n"
        << "  --tcp-local-port PORT      Local TCP port, 0 for automatic selection (default: 0)\n"
        << "  --state-rate HZ            Requested robot state upload rate (default: 100)\n"
        << "\n"
        << "Server options:\n"
        << "  --listen-ip IP             Client listen IP (default: 127.0.0.1)\n"
        << "  --listen-port PORT         Client listen port (default: " << kDefaultListenPort << ")\n"
        << "  -h, --help                 Show this help\n"
        << "\n"
        << "Commands: zero, stand, sit, charging-sit, damping, stop, status,\n"
        << "          velocity X Y YAW, velocity-control on|off,\n"
        << "          gait walk|running|bipedal|inv_bipedal|pronk|pace|bound|left_flip|right_flip,\n"
        << "          jump up|front|back|left|right, quit\n";
}

bool parseOptions(int argc, char** argv, ServerOptions* options) {
    if (!options) {
        return false;
    }

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            options->sdk.help_requested = true;
            return true;
        }
        if (arg == "--robot-ip") {
            if (!bpx_sdk::example::requireValue(argc, argv, i)) return false;
            options->sdk.robot_ip = argv[++i];
            continue;
        }
        if (arg == "--state-port" || arg == "--robot-state-port") {
            if (!bpx_sdk::example::requireValue(argc, argv, i) ||
                !bpx_sdk::example::parseUint16(argv[++i], &options->sdk.robot_state_port)) {
                std::cerr << arg << " expects a port in range 0..65535" << std::endl;
                return false;
            }
            continue;
        }
        if (arg == "--tcp-local-port") {
            if (!bpx_sdk::example::requireValue(argc, argv, i) ||
                !bpx_sdk::example::parseUint16(argv[++i], &options->sdk.tcp_local_port)) {
                std::cerr << arg << " expects a port in range 0..65535" << std::endl;
                return false;
            }
            continue;
        }
        if (arg == "--state-rate") {
            if (!bpx_sdk::example::requireValue(argc, argv, i) ||
                !bpx_sdk::example::parseUint16(argv[++i], &options->sdk.state_rate_hz) ||
                options->sdk.state_rate_hz == 0) {
                std::cerr << arg << " expects a positive rate in range 1..65535" << std::endl;
                return false;
            }
            continue;
        }
        if (arg == "--listen-ip") {
            if (!bpx_sdk::example::requireValue(argc, argv, i)) return false;
            options->listen_ip = argv[++i];
            continue;
        }
        if (arg == "--listen-port") {
            if (!bpx_sdk::example::requireValue(argc, argv, i) ||
                !bpx_sdk::example::parseUint16(argv[++i], &options->listen_port) ||
                options->listen_port == 0) {
                std::cerr << arg << " expects a positive port in range 1..65535" << std::endl;
                return false;
            }
            continue;
        }

        std::cerr << "Unknown option: " << arg << std::endl;
        printUsage(argv[0]);
        return false;
    }

    return true;
}

std::string trim(const std::string& text) {
    auto begin = std::find_if_not(text.begin(), text.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    auto end = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();
    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}

std::vector<std::string> splitWords(const std::string& line) {
    std::istringstream input(line);
    std::vector<std::string> words;
    std::string word;
    while (input >> word) {
        words.push_back(word);
    }
    return words;
}

bool parseFloat(const std::string& text, float* value) {
    if (!value) {
        return false;
    }
    char* end = nullptr;
    const float parsed = std::strtof(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0' || !std::isfinite(parsed)) {
        return false;
    }
    *value = parsed;
    return true;
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

SocketHandle createListenSocket(const std::string& listen_ip, uint16_t listen_port) {
    SocketHandle server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == kInvalidSocket) {
        return kInvalidSocket;
    }

    int reuse = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(listen_port);
    if (inet_pton(AF_INET, listen_ip.c_str(), &address.sin_addr) != 1) {
        std::cerr << "invalid listen ip: " << listen_ip << std::endl;
        closeSocket(server);
        return kInvalidSocket;
    }

    if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        closeSocket(server);
        return kInvalidSocket;
    }
    if (listen(server, 4) != 0) {
        closeSocket(server);
        return kInvalidSocket;
    }
    return server;
}

const char* motionStateName(uint8_t state) {
    switch (static_cast<bpx_sdk::MotionState>(state)) {
        case bpx_sdk::MotionState::LyingDown:
            return "LyingDown";
        case bpx_sdk::MotionState::StandingUp:
            return "StandingUp";
        case bpx_sdk::MotionState::Passive:
            return "Passive";
        case bpx_sdk::MotionState::SitDown:
            return "SitDown";
        case bpx_sdk::MotionState::ChargingSitDown:
            return "ChargingSitDown";
        case bpx_sdk::MotionState::Motion:
            return "Motion";
    }
    return "Unknown";
}

const char* motionGaitName(uint8_t gait) {
    switch (static_cast<bpx_sdk::MotionGait>(gait)) {
        case bpx_sdk::MotionGait::Jump:
            return "Jump";
        case bpx_sdk::MotionGait::Walk:
            return "Walk";
        case bpx_sdk::MotionGait::Bipedal:
            return "Bipedal";
        case bpx_sdk::MotionGait::Flip:
            return "Flip";
        case bpx_sdk::MotionGait::WalkPhase:
            return "WalkPhase";
        case bpx_sdk::MotionGait::PoseTracking:
            return "PoseTracking";
        case bpx_sdk::MotionGait::Running:
            return "Running";
    }
    return "Unknown";
}

std::string statusLineUnlocked(bpx_sdk::MotionLevelControl& motion) {
    const auto sn = motion.getRobotSerialNumberValue();
    const auto model = motion.getRobotModelValue();
    const char* model_name = "<unavailable>";
    if (model) {
        switch (*model) {
            case bpx_sdk::RobotModel::BPX: model_name = "BPX"; break;
            case bpx_sdk::RobotModel::BPXPro: model_name = "BPX-Pro"; break;
            case bpx_sdk::RobotModel::BPW: model_name = "BPW"; break;
            default: model_name = "Unknown"; break;
        }
    }
    std::ostringstream output;
    output << "OK connected=" << motion.isConnected()
           << " SN=" << (sn ? *sn : "<unavailable>") << " model=" << model_name
           << " control_mode=" << bpx_sdk::example::formatControlMode(motion);
    uint8_t state = 0, gait = 0;
    output << " motion_state=";
    if (motion.getCurrentMotionState(&state)) output << unsigned(state) << "(" << motionStateName(state) << ")";
    else output << "<unavailable>";
    output << " last_motion_state=";
    if (motion.getLastMotionState(&state)) output << unsigned(state) << "(" << motionStateName(state) << ")";
    else output << "<unavailable>";
    output << " gait=";
    if (motion.getCurrentGait(&gait)) output << unsigned(gait) << "(" << motionGaitName(gait) << ")";
    else output << "<unavailable>";
    float velocity[3]{};
    output << " max_vel=";
    if (motion.getMaxVelocity(velocity)) output << "(" << velocity[0] << "," << velocity[1] << "," << velocity[2] << ")";
    else output << "<unavailable>";
    output << " " << bpx_sdk::example::formatPowerState(motion);
    return output.str();
}

void printTelemetryLoop(bpx_sdk::MotionLevelControl& motion,
                        std::mutex& sdk_mutex,
                        std::atomic<bool>& running) {
    while (running.load()) {
        {
            std::lock_guard<std::mutex> lock(sdk_mutex);
            const auto now = std::chrono::system_clock::now();
            const auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
            const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now - seconds).count();
            const auto time = std::chrono::system_clock::to_time_t(seconds);
            std::tm local{};
#ifdef _WIN32
            localtime_s(&local, &time);
#else
            localtime_r(&time, &local);
#endif
            std::ostringstream output;
            output << "robot state: " << std::put_time(&local, "%Y-%m-%d %H:%M:%S")
                   << "." << std::setfill('0') << std::setw(3) << millis
                   << " " << statusLineUnlocked(motion).substr(3);
            std::cout << output.str() << std::endl;
        }
        for (int i = 0; i < 10 && running.load(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool executeCommand(bpx_sdk::MotionLevelControl& motion,
                    const std::string& raw_line,
                    std::mutex& sdk_mutex,
                    std::string* response) {
    const std::string line = trim(raw_line);
    const std::vector<std::string> words = splitWords(line);
    if (!response) {
        return false;
    }
    if (words.empty()) {
        *response = "ERR empty command";
        return false;
    }

    const std::string& command = words[0];
    if (command == "help") {
        *response = "OK commands: zero, stand, sit, charging-sit, damping, stop, status, velocity X Y YAW, velocity-control on|off, gait walk|running|bipedal|inv_bipedal|pronk|pace|bound|left_flip|right_flip, jump up|front|back|left|right, help, quit|shutdown";
        return false;
    }

    std::lock_guard<std::mutex> lock(sdk_mutex);
    if (command == "zero") {
        motion.setZeroPositionsFlag();
        *response = "OK zero-position flag sent";
        return false;
    }
    if (command == "stand") {
        *response = motion.setStandUp() ? "OK stand command sent" : "ERR stand command failed";
        return false;
    }
    if (command == "sit") {
        *response = motion.setSitDown() ? "OK sit command sent" : "ERR sit command failed";
        return false;
    }
    if (command == "charging-sit") {
        *response = motion.setChargingSitDown()
            ? "OK charging sit-down command sent" : "ERR charging sit-down command failed";
        return false;
    }
    if (command == "damping") {
        *response = motion.setDamping() ? "OK damping command sent" : "ERR damping command failed";
        return false;
    }
    if (command == "stop") {
        const bool ok = motion.setVelocity(0.0f, 0.0f, 0.0f);
        motion.setVelocityControlFlag(false);
        *response = ok ? "OK velocity stopped" : "ERR stop command failed";
        return false;
    }
    if (command == "status") {
        *response = statusLineUnlocked(motion);
        return false;
    }
    if (command == "velocity") {
        if (words.size() != 4) {
            *response = "ERR usage: velocity X Y YAW";
            return false;
        }
        float x = 0.0f;
        float y = 0.0f;
        float yaw = 0.0f;
        if (!parseFloat(words[1], &x) || !parseFloat(words[2], &y) || !parseFloat(words[3], &yaw)) {
            *response = "ERR velocity values must be numbers";
            return false;
        }
        motion.setVelocityControlFlag(true);
        *response = motion.setVelocity(x, y, yaw) ? "OK velocity command sent" : "ERR velocity command failed";
        return false;
    }
    if (command == "velocity-control") {
        if (words.size() != 2 || (words[1] != "on" && words[1] != "off")) {
            *response = "ERR usage: velocity-control on|off";
            return false;
        }
        motion.setVelocityControlFlag(words[1] == "on");
        *response = "OK velocity control " + words[1];
        return false;
    }
    if (command == "jump") {
        if (words.size() != 2) {
            *response = "ERR usage: jump up|front|back|left|right";
            return false;
        }
        if (words[1] == "up") motion.setUpJump();
        else if (words[1] == "front") motion.setFrontJump();
        else if (words[1] == "back") motion.setBackJump();
        else if (words[1] == "left") motion.setLeftJump();
        else if (words[1] == "right") motion.setRightJump();
        else {
            *response = "ERR usage: jump up|front|back|left|right";
            return false;
        }
        *response = "OK jump command sent";
        return false;
    }
    if (command == "gait") {
        if (words.size() != 2) {
            *response = "ERR usage: gait NAME";
            return false;
        }
        const std::string& gait = words[1];
        if (gait == "walk") motion.setWalk();
        else if (gait == "running") motion.setRunning();
        else if (gait == "bipedal") motion.setBipedal();
        else if (gait == "inv_bipedal") motion.setInvBipedal();
        else if (gait == "pronk") motion.setPronk();
        else if (gait == "pace") motion.setPace();
        else if (gait == "bound") motion.setBound();
        else if (gait == "left_flip") motion.setLeftFlip();
        else if (gait == "right_flip") motion.setRightFlip();
        else {
            *response = "ERR unknown gait";
            return false;
        }
        *response = "OK gait command sent";
        return false;
    }
    if (command == "quit" || command == "shutdown") {
        *response = "OK server shutting down";
        return true;
    }

    *response = "ERR unknown command";
    return false;
}

bool serveClient(SocketHandle client, bpx_sdk::MotionLevelControl& motion, std::mutex& sdk_mutex) {
    if (!sendAll(client, "OK motion-level server ready\n")) return false;
    std::string line;
    while (readLine(client, &line)) {
        std::string response;
        const bool should_stop = executeCommand(motion, line, sdk_mutex, &response);
        const bool sent = sendAll(client, response + "\n");
        if (should_stop) {
            return true;
        }
        if (!sent) return false;
    }
    return false;
}

}  // namespace

int main(int argc, char** argv) {
    ServerOptions options;
    if (!parseOptions(argc, argv, &options)) {
        return 1;
    }
    if (options.sdk.help_requested) {
        return 0;
    }

    printVersion();

    bpx_sdk::MotionLevelControl motion;
    motion.setRobotIp(options.sdk.robot_ip.c_str());
    motion.setRobotStateUploadPort(options.sdk.robot_state_port);
    motion.setTcpLocalPort(options.sdk.tcp_local_port);
    motion.setRobotStateUploadRate(options.sdk.state_rate_hz);
    motion.setMotionCommandRate(50);

    std::cout << "robot_ip=" << options.sdk.robot_ip
              << " state_port=" << options.sdk.robot_state_port
              << " tcp_local_port=" << options.sdk.tcp_local_port
              << " state_rate=" << options.sdk.state_rate_hz
              << std::endl;

    if (!motion.connect()) {
        std::cerr << "failed to connect motion level control" << std::endl;
        return 1;
    }
    printRobotVersion(motion);
    bpx_sdk::example::printIdentityAvailability(motion);
    std::cout << "motion level control started" << std::endl;

    if (!initializeSockets()) {
        std::cerr << "failed to initialize sockets" << std::endl;
        motion.disconnect();
        return 1;
    }

    SocketHandle server = createListenSocket(options.listen_ip, options.listen_port);
    if (server == kInvalidSocket) {
        std::cerr << "failed to listen on " << options.listen_ip << ":" << options.listen_port << std::endl;
        cleanupSockets();
        motion.disconnect();
        return 1;
    }

    std::cout << "waiting for clients on " << options.listen_ip << ":" << options.listen_port << std::endl;
    std::mutex sdk_mutex;
    std::atomic<bool> telemetry_running{true};
    std::thread telemetry_thread(printTelemetryLoop,
                                 std::ref(motion),
                                 std::ref(sdk_mutex),
                                 std::ref(telemetry_running));

    bool should_stop = false;
    while (!should_stop) {
        sockaddr_in peer{};
#ifdef _WIN32
        int peer_len = sizeof(peer);
#else
        socklen_t peer_len = sizeof(peer);
#endif
        SocketHandle client = accept(server, reinterpret_cast<sockaddr*>(&peer), &peer_len);
        if (client == kInvalidSocket) {
            std::cerr << "accept failed" << std::endl;
            break;
        }
        std::cout << "client connected" << std::endl;
        should_stop = serveClient(client, motion, sdk_mutex);
        closeSocket(client);
        std::cout << "client disconnected" << std::endl;
    }

    telemetry_running.store(false);
    telemetry_thread.join();
    closeSocket(server);
    cleanupSockets();
    {
        std::lock_guard<std::mutex> lock(sdk_mutex);
        motion.disconnect();
    }
    return should_stop ? 0 : 1;
}
