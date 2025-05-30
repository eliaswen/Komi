# Initial setup
cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Komi)

# Find Client Packages
find_package(cpprestsdk CONFIG REQUIRED)

# Find Server Packages
find_package(boost_asio REQUIRED CONFIG)
find_package(boost_beast REQUIRED CONFIG)
find_package(re2 CONFIG REQUIRED)

# Add executables
add_executable(KomiClient client.cpp)
add_executable(KomiServer server.cpp)

# Link Client Libraries
target_link_libraries(KomiClient PRIVATE cpprestsdk::cpprest cpprestsdk::cpprestsdk_zlib_internal cpprestsdk::cpprestsdk_brotli_internal)

# Link Server Libraries
target_link_libraries(KomiServer PRIVATE Boost::asio)
target_link_libraries(KomiServer PRIVATE Boost::beast)
target_link_libraries(KomiServer PRIVATE re2::re2)