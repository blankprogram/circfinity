find_package(Catch2 CONFIG REQUIRED)

add_executable(test_compute
  test_compute.cpp
)

target_link_libraries(test_compute
  PRIVATE
    compute_lib
    Catch2::Catch2WithMain
)

add_test(
  NAME compute_tests
  COMMAND test_compute
)
