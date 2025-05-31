# 添加Disp可执行文件
add_executable(disp
    disp/main.cpp
    disp/server.cpp
    disp/request_handler.cpp
    common/config.cpp
    common/logger_enhanced.cpp
    common/utils.cpp
)

# 添加Ap可执行文件
add_executable(ap
    ap/main.cpp
    ap/processor.cpp
    ap/db_manager.cpp
    common/config.cpp
    common/logger_enhanced.cpp
    common/utils.cpp
)

# 链接MySQL库和其他依赖
target_link_libraries(disp PRIVATE ${MYSQL_LIBRARIES} pthread curl crypto ssl)
target_link_libraries(ap PRIVATE ${MYSQL_LIBRARIES} pthread crypto ssl)

# 安装目标
install(TARGETS disp ap DESTINATION bin)