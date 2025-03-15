#include "doctest.h"

#include "pkg/git.h"

using pkg::url_to_protocol;

TEST_CASE("to ssh") {
  CHECK(url_to_protocol("https://github.com/motis-project/pkg",
                        pkg::protocol::kSsh) ==
        "ssh://git@github.com/motis-project/pkg");

  CHECK(url_to_protocol("ssh://git@github.com/motis-project/pkg",
                        pkg::protocol::kSsh) ==
        "ssh://git@github.com/motis-project/pkg");

  CHECK(url_to_protocol("git://github.com/motis-project/pkg",
                        pkg::protocol::kSsh) ==
        "ssh://git@github.com/motis-project/pkg");

  CHECK(url_to_protocol("git@github.com:motis-project/pkg",
                        pkg::protocol::kSsh) ==
        "ssh://git@github.com/motis-project/pkg");
}

TEST_CASE("to https") {
  CHECK(url_to_protocol("https://github.com/motis-project/pkg",
                        pkg::protocol::kHttps) ==
        "https://github.com/motis-project/pkg");

  CHECK(url_to_protocol("ssh://git@github.com/motis-project/pkg",
                        pkg::protocol::kHttps) ==
        "https://github.com/motis-project/pkg");

  CHECK(url_to_protocol("git://github.com/motis-project/pkg",
                        pkg::protocol::kHttps) ==
        "https://github.com/motis-project/pkg");

  CHECK(url_to_protocol("git@github.com:motis-project/pkg",
                        pkg::protocol::kHttps) ==
        "https://github.com/motis-project/pkg");
}
