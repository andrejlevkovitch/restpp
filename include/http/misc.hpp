// misc.hpp

#pragma once

#include <string_view>

#define OUTPUT


namespace misc {
template <typename other_string_view_type>
std::string_view string_view_cast(other_string_view_type str_view) noexcept {
  return std::string_view{str_view.data(), str_view.size()};
}
} // namespace misc
