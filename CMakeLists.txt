cmake_minimum_required(VERSION 3.10)
project(ServerProject VERSION 1.0)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# 查找MySQL库
# 使用pkg-config查找MySQL
find_package(PkgConfig REQUIRED)
pkg_check_modules(MYSQL REQUIRED mysqlclient)

if(NOT MYSQL_FOUND)
    message(FATAL_ERROR "MySQL not found")
endif()

# 添加头文件目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${MYSQL_INCLUDE_DIRS}
)

# 添加子目录
add_subdirectory(src)

# 设置安装规则
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin/
        DESTINATION bin
        FILE_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
                         GROUP_EXECUTE GROUP_READ
                         WORLD_EXECUTE WORLD_READ)