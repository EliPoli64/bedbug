qt_add_executable(tst_smoke
    tst_smoke.cpp
)

target_link_libraries(tst_smoke PRIVATE
    Qt6::Test
    bb_core
)

add_test(NAME tst_smoke COMMAND tst_smoke)

qt_add_executable(tst_model_json
    tst_model_json.cpp
)

target_link_libraries(tst_model_json PRIVATE
    Qt6::Test
    bb_core
)

add_test(NAME tst_model_json COMMAND tst_model_json)