// url.hpp

#pragma once

#include <list>
#include <map>
#include <ostream>
#include <restpp/misc.hpp>
#include <string>


namespace restpp::http {
class url {
public:
  class path;
  class query;

  class args : public std::multimap<std::string, std::string> {
  public:
    args::mapped_type &operator[](const args::key_type &key) {
      if (auto found = this->lower_bound(key); found != this->end()) {
        return found->second;
      }

      const auto &out = this->emplace(key, args::mapped_type{});
      return out->second;
    }

    const args::mapped_type &at(const args::key_type &key) const {
      if (auto found = this->lower_bound(key); found != this->end()) {
        return found->second;
      }

      throw std::out_of_range{"args::at"};
    }

    args::mapped_type &at(const args::key_type &key) {
      if (auto found = this->lower_bound(key); found != this->end()) {
        return found->second;
      }

      throw std::out_of_range{"args::at"};
    }
  };

private:
  class path_signature {
  public:
    class path_signature_impl;
    using args = url::args;

    explicit path_signature(std::string_view signature);
    ~path_signature();

    path_signature(const path_signature &rhs);
    path_signature(path_signature &&rhs);
    path_signature &operator=(const path_signature &rhs);
    path_signature &operator=(path_signature &&rhs);

    /**\brief check that path matches the signature
     * \note that new arguments will be added to existed in args
     */
    bool match(const url::path &path, OUTPUT args &args) const noexcept;
    bool operator==(const url::path &path) const noexcept;

    /**\return path_signature that match any path
     */
    static path_signature any();


  private:
    path_signature_impl *impl_;
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


  static std::string_view get_path(std::string_view url) noexcept;
  static std::string_view get_query(std::string_view url) noexcept;

  /**\return path and query from the url
   */
  static std::pair<std::string_view, std::string_view>
  split(std::string_view url) noexcept;


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
} // namespace restpp::http


namespace std {
inline ostream &operator<<(ostream &out, const restpp::http::url::path &path) {
  out << std::string{path};
  return out;
}
} // namespace std
