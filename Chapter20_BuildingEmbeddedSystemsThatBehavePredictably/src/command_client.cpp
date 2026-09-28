#include "Lab.h"
#include <poll.h>
#include <sstream>
#include <vector>

namespace {
void help() {
    std::cout << "Commands: status | parameters | get NAME | set NAME VALUE | save | help | quit\n"
                 "Names: ReportingInterval (ms), TemperatureLimit (C), StaleTimeout (ms)\n";
}
bool execute(int fd, const std::vector<std::string>& words, std::uint32_t& nextId) {
    if (words.empty()) return true;
    if (words[0] == "quit" || words[0] == "exit") return false;
    if (words[0] == "help") { help(); return true; }
    std::string operation;
    if (words[0] == "status" && words.size() == 1) operation = "STATUS";
    else if (words[0] == "parameters" && words.size() == 1) operation = "PARAMETERS";
    else if (words[0] == "get" && words.size() == 2) operation = "GET," + words[1];
    else if (words[0] == "set" && words.size() == 3) operation = "SET," + words[1] + "," + words[2];
    else if (words[0] == "save" && words.size() == 1) operation = "SAVE";
    else { help(); return true; }
    for (const auto& word : words)
        if (word.find_first_of(",*$\r\n") != std::string::npos) {
            std::cout << "Invalid command argument\n"; return true;
        }
    const std::string id = std::to_string(++nextId);
    if (!ch20::send(fd, ch20::commandPort, "WXCMD," + id + "," + operation)) {
        std::cout << "Failed to send command\n"; return true;
    }
    const auto deadline = ch20::Clock::now() + std::chrono::milliseconds(1000);
    while (ch20::Clock::now() < deadline) {
        pollfd input{fd, POLLIN, 0};
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - ch20::Clock::now()).count();
        if (::poll(&input, 1, static_cast<int>(std::max<std::int64_t>(1, remaining))) <= 0) break;
        if (!(input.revents & POLLIN)) continue;
        ch15::ParsedSentence response{};
        sockaddr_in sender{};
        if (ch20::receive(fd, response, &sender) && sender.sin_port == htons(ch20::commandPort) &&
            response.identifier == "WXACK" && response.fieldCount == 3 && response.fields[0] == id) {
            std::cout << response.fields[1] << ": " << response.fields[2] << '\n';
            if (operation == "STATUS" || operation == "PARAMETERS" || operation.rfind("GET,", 0) == 0)
                std::cout << "See the telemetry listener pane for the response.\n";
            return true;
        }
    }
    std::cout << "No ACK received; the command's outcome is unknown.\n";
    return true;
}
} // namespace

int main(int argc, char** argv) {
    try {
        const int fd = ch20::socketFor();
        std::uint32_t nextId{};
        if (argc > 1) {
            std::vector<std::string> words(argv + 1, argv + argc);
            execute(fd, words, nextId);
        } else {
            help();
            for (std::string line; std::cout << "weather> " && std::getline(std::cin, line); ) {
                std::istringstream input(line);
                std::vector<std::string> words;
                for (std::string word; input >> word; ) words.push_back(word);
                if (!execute(fd, words, nextId)) break;
            }
        }
        ::close(fd);
    } catch (const std::exception& error) {
        std::cerr << "command client: " << error.what() << '\n'; return 1;
    }
}
