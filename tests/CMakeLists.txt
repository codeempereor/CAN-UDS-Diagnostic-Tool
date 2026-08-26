# 单元测试
enable_testing()

# CAN基础测试
add_executable(test_can_filter test_can_filter.cpp)
target_link_libraries(test_can_filter PRIVATE core_protocol)
add_test(NAME test_can_filter COMMAND test_can_filter)

# ISO-TP测试
add_executable(test_isotp test_isotp.cpp)
target_link_libraries(test_isotp PRIVATE core_protocol)
add_test(NAME test_isotp COMMAND test_isotp)

# UDS测试
add_executable(test_uds test_uds.cpp)
target_link_libraries(test_uds PRIVATE core_protocol)
add_test(NAME test_uds COMMAND test_uds)

# DBC解析测试
add_executable(test_dbc test_dbc.cpp)
target_link_libraries(test_dbc PRIVATE core_protocol)
add_test(NAME test_dbc COMMAND test_dbc)
