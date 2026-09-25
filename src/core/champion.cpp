#include "champion.h"

namespace sintence {
namespace {

    std::string CheckName(std::string name) {
      if (name.empty()) return "Unknown";

      return name;
    }

    std::string CheckRole(std::string role) {
      if (role.empty()) return "UNKNOWN";
        for (auto& index : role) {
          index = static_cast<char>(std::toupper(index));
        }
      return role;
    }
}  // namespace

Champion::Champion(std::string name, std::string role) : name_(CheckName(name)), role_(CheckRole(role)) {
}

const std::string& Champion::Name() const {
    return name_;
}

const std::string& Champion::Role() const {
    return role_;
}

std::string Champion::DisplayName() const {
    return name_ + " (" + role_ + ")";
}

bool Champion::HasRole(const std::string& role) const {
    if (role.empty()) return false;

    if (CheckRole(role) == role_) return true;

    return false;
}

}  // namespace sintence
