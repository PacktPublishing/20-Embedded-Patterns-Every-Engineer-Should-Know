#pragma once
#include <random>
#include <string>

namespace nmea_sim
{
std::string make_sentence(const std::string& body);
std::string corrupt_bad_checksum(std::string sentence);
std::string corrupt_truncate(std::string sentence, std::mt19937& rng);
}
