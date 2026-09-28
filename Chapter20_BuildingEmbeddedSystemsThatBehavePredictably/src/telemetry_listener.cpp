#include "Lab.h"
#include <csignal>
#include <deque>

namespace { volatile std::sig_atomic_t running = 1; void stop(int) { running = 0; } }
int main() {
    try {
        std::signal(SIGINT, stop); std::signal(SIGTERM, stop);
        const int fd = ch20::socketFor(ch20::telemetryPort);
        timeval timeout{0, 250000};
        ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
        const bool dashboard = ::isatty(STDOUT_FILENO);
        std::string latestStatus = "(request with 'status' in the command pane)";
        std::string latestParameters = "(request with 'parameters' or 'get NAME')";
        std::string latestTemperature = "(waiting for temperature telemetry)";
        std::deque<std::string> recent;
        const auto redraw = [&] {
            std::cout << "\033[H\033[2J"
                      << "Telemetry Listener (UDP " << ch20::telemetryPort << ")\n"
                      << "LATEST STATUS (stays visible):\n" << latestStatus << "\n\n"
                      << "LATEST PARAMETERS:\n" << latestParameters << "\n\n"
                      << "LATEST TEMPERATURE:\n" << latestTemperature << "\n\n"
                      << "RECENT TELEMETRY:\n";
            for (const auto& line : recent) std::cout << line << '\n';
            std::cout << std::flush;
        };
        if (dashboard) redraw();
        else std::cout << "Telemetry Listener on UDP " << ch20::telemetryPort << '\n';
        while (running) {
            char bytes[ch20::maxDatagram + 1]{};
            const auto count = ::recv(fd, bytes, sizeof bytes, 0);
            if (count <= 0) continue;
            ch15::ParsedSentence parsed{};
            const std::string_view sentence(bytes, static_cast<std::size_t>(count));
            if (count > static_cast<ssize_t>(ch20::maxDatagram) ||
                ch15::parseSentence(sentence, parsed) != ch15::ParseStatus::Ok) {
                if (dashboard) {
                    recent.push_back("Rejected invalid telemetry");
                    if (recent.size() > 8) recent.pop_front();
                    redraw();
                } else std::cout << "Rejected invalid telemetry\n";
                continue;
            }
            const std::string line(sentence.substr(0, sentence.find_first_of("\r\n")));
            if (dashboard) {
                if (parsed.identifier == "WXSTS") latestStatus = line;
                if (parsed.identifier == "WXPAR") latestParameters = line;
                if (parsed.identifier == "WXTMP") latestTemperature = line;
                recent.push_back(line);
                if (recent.size() > 8) recent.pop_front();
                redraw();
            } else std::cout << sentence << std::flush;
        }
        ::close(fd);
    } catch (const std::exception& ex) { std::cerr << ex.what() << '\n'; return 1; }
}
