cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(stackchan_phone)

# `idf.py flash` 를 막는다: 이 프로젝트의 파티션 표에서 IDF 가 고르는 "기본 앱 자리"는 factory = UIFlow2(스트림덱)의 자리다(partitions.csv).
# 거기에 전화기 앱을 쓰면 스트림덱이 지워진다 → 올리기는 tools/sc_flash.sh(앱만 ota_0 자리에)·첫 설치는 tools/sc_install.sh 로만 한다.
add_custom_target(sc_no_idf_flash
    COMMAND ${CMAKE_COMMAND} -E echo "stackchan_phone: 'idf.py flash' 는 막혀 있다 - 앱이 UIFlow2 자리(factory)에 쓰인다. tools/sc_flash.sh 또는 tools/sc_install.sh 를 쓸 것."
    COMMAND ${CMAKE_COMMAND} -E false
    VERBATIM)
foreach(t flash app-flash bootloader-flash partition-table-flash encrypted-flash encrypted-app-flash)
    if(TARGET ${t})
        add_dependencies(${t} sc_no_idf_flash)
    endif()
endforeach()
