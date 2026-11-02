#pragma once
#include <string>
namespace lms::security {
std::string hashPassword(const std::string& password);
bool verifyPassword(const std::string& password, const std::string& encoded);
std::string readSecret(const std::string& prompt);
}
