// url.cpp

#include "restpp/http/url.hpp"
#include <regex>
#include <sstream>

#define MATCH_ALL ".*"


const std::regex split_path_reg{"/"};
const std::regex split_query_reg{R"(&)"};

const std::regex path_from_url{R"(\/[^?]*)"};
const std::regex query_from_url{R"(\?(.+))"};

const std::regex smpl_value_reg{R"(^\w*$)"};
const std::regex spec_value_reg{R"(^\<[^\>]+\>$)"};


namespace restpp::http {
using str_pair = std::pair<std::string, std::string>;
using path_token_matcher =
    std::function<bool(std::string_view, std::optional<str_pair> &)>;
using matcher_list = std::list<path_token_matcher>;


url::path::path(std::string_view val)
    : path_{} {
  if (val.empty() || val == "/") {
    return;
  }

  if (val.front() != '/') {
    throw std::invalid_argument{"invalid path: path must starts with /"};
  }


  for (auto iter =
           std::cregex_token_iterator{val.begin() + 1, // skip first shash
                                      val.end(),
                                      split_path_reg,
                                      -1};
       iter != std::cregex_token_iterator{};
       ++iter) {
    std::string token = iter->str();

    path_.emplace_back(std::move(token));
  }
}


bool url::path::empty() const noexcept {
  return path_.empty();
}

size_t url::path::size() const noexcept {
  return path_.size();
}

bool url::path::is_base_of(const path &rhs) const {
  if (this->path_.size() > rhs.path_.size()) {
    return false;
  }

  return std::equal(path_.begin(), path_.end(), rhs.path_.begin());
}

bool url::path::operator==(const path &rhs) const {
  return path_ == rhs.path_;
}

bool url::path::operator!=(const path &rhs) const {
  return path_ != rhs.path_;
}

url::path url::path::operator/(std::string_view rhs) const {
  return this->operator/(path{rhs});
}

url::path url::path::operator/(const path &rhs) const noexcept {
  if (rhs.empty()) {
    return *this;
  } else if (this->empty()) {
    return rhs;
  }

  path retval = *this;
  std::copy(rhs.path_.begin(),
            rhs.path_.end(),
            std::back_inserter(retval.path_));

  return retval;
}

url::path &url::path::operator/=(std::string_view rhs) {
  return this->operator/=(path{rhs});
}

url::path &url::path::operator/=(const path &rhs) noexcept {
  std::copy(rhs.path_.begin(),
            rhs.path_.end(),
            std::back_inserter(this->path_));
  return *this;
}

url::path::operator std::string() const noexcept {
  if (path_.empty()) {
    return "/";
  }

  std::stringstream ss;
  ss << "/";
  std::copy(path_.begin(),
            std::prev(path_.end()),
            std::ostream_iterator<std::string>{ss, "/"});
  ss << path_.back();
  return ss.str();
}

url::path::const_iterator url::path::begin() const noexcept {
  return path_.begin();
}

url::path::const_iterator url::path::end() const noexcept {
  return path_.end();
}


std::string_view url::get_path(std::string_view target) noexcept {
  std::cmatch match;
  if (std::regex_search(target.begin(), target.end(), match, path_from_url)) {
    return std::string_view(match[0].first,
                            std::distance(match[0].first, match[0].second));
  }

  return {};
}

std::string_view url::get_query(std::string_view target) noexcept {
  std::cmatch match;
  if (std::regex_search(target.begin(), target.end(), match, query_from_url) &&
      match.size() >= 2) {
    return std::string_view(match[1].first,
                            std::distance(match[1].first, match[1].second));
  }

  return {};
}

std::pair<std::string_view, std::string_view>
url::split(std::string_view url) noexcept {
  return {url::get_path(url), url::get_query(url)};
}


url::query::args url::query::split(std::string_view query) noexcept {
  std::list<std::string> tokens;
  std::copy(std::cregex_token_iterator{query.begin(),
                                       query.end(),
                                       split_query_reg,
                                       -1},
            std::cregex_token_iterator{},
            std::back_inserter(tokens));

  args retval;
  for (const std::string &token : tokens) {
    if (auto found = std::find(token.begin(), token.end(), '=');
        found != token.end()) {
      retval.emplace(std::string{token.begin(), found},
                     std::string{std::next(found), token.end()});
    } else { // token contains only key
      retval.emplace(token, "");
    }
  }

  return retval;
}


class url::path_signature::path_signature_impl {
public:
  virtual ~path_signature_impl() = default;

  path_signature_impl(std::string_view signature) {
    url::path path{signature};

    for (const auto &path_token : path) {
      path_token_matcher matcher;
      if (std::regex_match(path_token, spec_value_reg)) {
        // at first split the special value on key and reg
        std::string key;
        std::regex  reg;

        if (auto found = std::find(path_token.begin(), path_token.end(), '=');
            found != path_token.end()) {
          key = std::string{std::next(path_token.begin()), found};
          reg = std::string{found + 1, std::prev(path_token.end())};
        } else {
          key = path_token.substr(1, path_token.size() - 2);
          reg = MATCH_ALL;
        }


        matcher = [key, reg](std::string_view         token,
                             std::optional<str_pair> &spec) {
          std::cmatch match;
          bool ok = std::regex_match(token.begin(), token.end(), match, reg);
          if (ok) {
            std::string val;
            if (match.size() > 1) { // return first capture
              val = std::string{match[1].first, match[1].second};
            } else { // return all match
              val = token;
            }

            spec.emplace(key, std::move(val));
          }
          return ok;
        };
      } else if (std::regex_match(path_token, smpl_value_reg)) { // full match
        matcher = [path_token](std::string_view                          input,
                               [[maybe_unused]] std::optional<str_pair> &spec) {
          return input == path_token;
        };
      } else { // so, this token is wrong, because it don't match special and
               // simple values
        std::invalid_argument{"invalid path token: " + path_token};
      }

      matchers_.emplace_back(matcher);
    }
  }

  virtual bool match(const url::path &     path,
                     path_signature::args &path_args) const noexcept {
    if (path.size() != matchers_.size()) {
      return false;
    }


    path_signature::args retval;
    auto                 match_iter = matchers_.begin();
    auto                 path_iter  = path.begin();
    for (; path_iter != path.end() && match_iter != matchers_.end();
         ++match_iter, ++path_iter) {
      std::string_view token = *path_iter;

      std::optional<str_pair>   spair;
      const path_token_matcher &matcher = *match_iter;
      if (matcher(token, spair) == false) {
        return false;
      }

      if (spair.has_value()) {
        retval.emplace(std::move(spair.value()));
      }
    }

    path_args.merge(retval);

    return true;
  }

private:
  matcher_list matchers_;
};

class any_matcher : public url::path::signature::path_signature_impl {
public:
  any_matcher()
      : url::path::signature::path_signature_impl{""} {
  }

  bool match([[maybe_unused]] const url::path &           path,
             [[maybe_unused]] url::path::signature::args &path_args) const
      noexcept override {
    return true;
  }
};


url::path_signature::path_signature(std::string_view signature)
    : impl_{new path_signature_impl{signature}} {
}

url::path_signature::~path_signature() {
  delete impl_;
  DEAD_BEAF(impl_);
}

url::path_signature::path_signature(const path_signature &rhs)
    : impl_{new path_signature_impl{""}} {
  *impl_ = *rhs.impl_;
}

url::path_signature::path_signature(path_signature &&rhs)
    : impl_{rhs.impl_} {
  rhs.impl_ = nullptr;
}

url::path_signature &url::path_signature::operator=(const path_signature &rhs) {
  *impl_ = *rhs.impl_;
  return *this;
}

url::path_signature &url::path_signature::operator=(path_signature &&rhs) {
  delete impl_;
  impl_     = rhs.impl_;
  rhs.impl_ = nullptr;
  return *this;
}

url::path_signature url::path_signature::any() {
  path_signature retval{"/"};
  delete retval.impl_;
  retval.impl_ = new any_matcher{};
  return retval;
}

bool url::path_signature::match(const url::path &     path,
                                path_signature::args &path_args) const
    noexcept {
  return impl_->match(path, path_args);
}

bool url::path_signature::operator==(const url::path &path) const noexcept {
  path_signature::args path_args;
  return impl_->match(path, path_args);
}

bool operator==(const url::path &path, const url::path::signature &signature) {
  return signature == path;
}

bool url::path_signature::operator!=(const url::path &path) const noexcept {
  path_signature::args path_args;
  return impl_->match(path, path_args) == false;
}

bool operator!=(const url::path &path, const url::path::signature &signature) {
  return signature != path;
}
} // namespace restpp::http
