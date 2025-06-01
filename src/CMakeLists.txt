file(GLOB DISP_SOURCES 
    "disp/*.cpp"
    "common/*.cpp"
)
file(GLOB AP_SOURCES 
    "ap/*.cpp"
    "common/*.cpp"
)

# 添加可执行文件
add_executable(disp ${DISP_SOURCES})
add_executable(ap ${AP_SOURCES})

# 链接MySQL库和其他依赖
target_link_libraries(disp PRIVATE ${MYSQL_LIBRARIES} pthread curl crypto ssl)
target_link_libraries(ap PRIVATE ${MYSQL_LIBRARIES} pthread crypto ssl)

# 安装目标
install(TARGETS disp ap DESTINATION bin)