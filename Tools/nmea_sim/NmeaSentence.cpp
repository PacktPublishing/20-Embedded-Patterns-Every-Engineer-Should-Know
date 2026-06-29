#include "NmeaSentence.h"
#include <iomanip>
#include <sstream>

namespace nmea_sim
{
std::string make_sentence(const std::string& body)
{
    unsigned char checksum = 0;
    for (const unsigned char c : body)
    {
        checksum ^= c;
    }

    std::ostringstream os;
    os << '$' << body << '*'
       << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<int>(checksum)
       << "\r\n";
    return os.str();
}

std::string corrupt_bad_checksum(std::string sentence)
{
    const auto star = sentence.find('*');
    if (star != std::string::npos && star + 2 < sentence.size())
    {
        sentence[star + 1] = (sentence[star + 1] == '0') ? '1' : '0';
    }
    return sentence;
}

std::string corrupt_truncate(std::string sentence, std::mt19937& rng)
{
    if (sentence.size() <= 4) return sentence;
    std::uniform_int_distribution<std::size_t> dist(1, sentence.size() - 2);
    sentence.resize(dist(rng));
    sentence += "\r\n";
    return sentence;
}
}
