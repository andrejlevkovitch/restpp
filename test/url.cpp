// url.cpp

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

//

#include "http/url.hpp"

using namespace http::literals;


TEST_CASE("url", "[url]") {
  SECTION("getting path") {
    CHECK(http::url::get_path("") == "/"_path);
    CHECK(http::url::get_path("/") == "/"_path);

    CHECK(http::url::get_path("/some/path") == "/some/path"_path);
    CHECK(http::url::get_path("/some/path?with=args") == "/some/path"_path);
  }

  SECTION("getting args") {
    CHECK(http::url::get_query("invalid query") == "");
    CHECK(http::url::get_query("/only/path") == "");

    CHECK(http::url::get_query("?arg=arg") == "arg=arg");
    CHECK(http::url::get_query("/some/path?arg=arg") == "arg=arg");
  }

  SECTION("parsing args") {
    CHECK("arg=arg"_query.size() == 1);
    CHECK("arg=arg&arg2=arg2"_query.size() == 2);

    // dublicate key
    CHECK("arg=arg&arg=arg2"_query.size() == 2);

    http::url::args for_check;
    for_check.emplace("arg", "val");
    for_check.emplace("arg", "val2");
    for_check.emplace("other", "some_value");
    CHECK("arg=val&arg=val2&other=some_value"_query == for_check);
  }
}


TEST_CASE("path", "[path]") {
  SECTION("empty path") {
    http::url::path empty_path{""};
    http::url::path root_path{"/"};

    CHECK(empty_path.empty());
    CHECK(root_path.empty());
    CHECK(root_path == empty_path);

    CHECK(std::string{empty_path} == "/");
    CHECK(std::string{root_path} == "/");

    CHECK_FALSE(std::string{empty_path} == "");
    CHECK_FALSE(std::string{root_path} == "");
  }

  SECTION("wrong path") {
    CHECK_NOTHROW("/some/path"_path);
    CHECK_THROWS_AS("invalid/path"_path, std::invalid_argument);
  }

  SECTION("path size") {
    CHECK(""_path.size() == 0);
    CHECK("/"_path.size() == 0);
    CHECK("/root"_path.size() == 1);
    CHECK("/root/more"_path.size() == 2);
    CHECK("/root/more/"_path.size() == 2);
  }

  SECTION("slash dublicates") {
    CHECK(std::string{"/root//after"_path} == "/root//after");

    // ignore last slash
    CHECK(std::string{"/root/"_path} == "/root");
    CHECK(std::string{"///"_path} == "//");

    CHECK_FALSE(std::string{"/root"_path} == "/root/");
    CHECK_FALSE(std::string{"///"} == "/");
  }

  SECTION("path concatenation") {
    CHECK((""_path / ""_path) == "/"_path);
    CHECK(("/"_path / ""_path) == "/"_path);

    CHECK(("/root"_path / ""_path) == "/root"_path);
    CHECK(("/root"_path / "/more"_path) == "/root/more"_path);

    CHECK(("/very/very"_path / "/long/path"_path) ==
          "/very/very/long/path"_path);
  }

  SECTION("base path") {
    CHECK("/"_path.is_base_of("/"_path));
    CHECK("/"_path.is_base_of("/any/path"_path));

    CHECK("/some/path"_path.is_base_of("/some/path"_path));
    CHECK("/some/path"_path.is_base_of("/some/path/"_path));
    CHECK("/some/path"_path.is_base_of("/some/path/more"_path));
  }
}


TEST_CASE("path signature", "path_signature") {
  SECTION("checking signature with path") {
    CHECK("/"_path == "/"_psign);
    CHECK("/path"_path == "/path"_psign);
    CHECK_FALSE("/path"_path == "/other"_psign);

    CHECK("/root"_path == "/<val>"_psign);
    CHECK("/path"_path == "/<val>"_psign);
    CHECK_FALSE("/path/more"_path == "/<val>"_psign);
  }

  SECTION("gettig path args") {
    http::url::path::signature::args args;

    CHECK("/root/<arg>"_psign.match("/root/val"_path, args));
    CHECK(args.size() == 1);
    CHECK(args["arg"] == "val");

    args.clear();
    CHECK("/root/<arg>/<arg2>/something/<arg3>"_psign.match(
        "/root/val/val2/something/val3"_path,
        args));
    CHECK(args.size() == 3);
    CHECK(args["arg"] == "val");
    CHECK(args["arg2"] == "val2");
    CHECK(args["arg3"] == "val3");
  }

  SECTION("path args must be merged") {
    http::url::path::signature::args args;
    "/root/<arg>"_psign.match("/root/val"_path, args);
    "/root/<arg2>"_psign.match("/root/val2"_path, args);

    CHECK(args.size() == 2);
    CHECK(args["arg"] == "val");
    CHECK(args["arg2"] == "val2");
  }
}
