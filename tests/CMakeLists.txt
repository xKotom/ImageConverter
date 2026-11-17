function(uic_add_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE uic_core Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

uic_add_test(test_unit unit/test_unit.cpp)
uic_add_test(test_integration integration/test_integration.cpp)
uic_add_test(test_stage5 integration/test_stage5.cpp)
