# SPDX-License-Identifier: BSL-1.0

add_executable(edn-tests
  test_value.cpp
  test_parser.cpp
  test_emitter.cpp
)

target_link_libraries(edn-tests
  PRIVATE edn::edn
          Catch2::Catch2WithMain
          sanitizers::sanitizers
)

set_target_properties(edn-tests PROPERTIES
  CXX_STANDARD          20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS        OFF
)

include(CTest)
include(Catch)
catch_discover_tests(edn-tests)
