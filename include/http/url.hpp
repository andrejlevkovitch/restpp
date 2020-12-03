// url.hpp

#pragma once

#include <functional>
#include <http/misc.hpp>
#include <list>
#include <map>
#include <ostream>
#include <string>


namespace http {
class url {
public:
  class path;
  class query;

  class args : public std::multimap<std::string, std::string> {
  public:
    const args::mapped_type &operator[](const args::key_type &key) {
      if (auto found = this->lower_bound(key); found != this->end()) {
        return found->second;
      }

      const auto &out = this->emplace(key, args::mapped_type{});
      return out->second;
    }
  };

private:
  class path_signature {
    using str_pair = std::pair<std::string, std::string>;
    using path_token_matcher =
        std::function<bool(std::string_view, std::optional<str_pair> &)>;
    using matcher_list = std::list<path_token_matcher>;

  public:
    using args = url::args;

    explicit path_signature(std::string_view signature);

    /**\brief check that path matches the signature
     * \note that new arguments will be added to existed in args
     */
    bool match(const url::path &path, OUTPUT args &args) const noexcept;
    bool operator==(const url::path &path) const noexcept;


  private:
    matcher_list matchers_;
  };

public:
  class path {
  public:
    using tokens         = std::list<std::string>;
    using const_iterator = tokens::const_iterator;
    using signature      = path_signature;

    path() = default;
    explicit path(std::string_view path);

    /**\return true if path is empty or '/'
     */
    bool empty() const noexcept;

    /**\return capacity of tokens in path
     */
    size_t size() const noexcept;

    bool is_base_of(const path &rhs) const;
    bool operator==(const path &rhs) const;

    path operator/(std::string_view rhs) const;
    path operator/(const path &rhs) const noexcept;

    path &operator/=(std::string_view rhs);
    path &operator/=(const path &rhs) noexcept;

    operator std::string() const noexcept;

    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;

    template <typename val, template <typename> typename container>
    static path join(const container<val> &path_tokens) {
      path retval;
      for (const auto &rhs : path_tokens) {
        retval /= rhs;
      }

      return retval;
    }

  private:
    tokens path_;
  };


  static path             get_path(std::string_view url) noexcept;
  static std::string_view get_query(std::string_view url) noexcept;


  class query {
  public:
    using args = url::args;
    static args split(std::string_view query) noexcept;
  };
};

bool operator==(const url::path &path, const url::path::signature &signature);

namespace literals {
inline url::path operator""_path(const char *str, size_t len) {
  return url::path{std::string_view{str, len}};
}

inline url::path::signature operator""_psign(const char *str, size_t len) {
  return url::path::signature(std::string_view{str, len});
}

inline url::query::args operator""_query(const char *str, size_t len) {
  return url::query::split(std::string_view{str, len});
}
} // namespace literals
} // namespace http


namespace std {
inline ostream &operator<<(ostream &out, const http::url::path &path) {
  out << std::string{path};
  return out;
}
} // namespace std
