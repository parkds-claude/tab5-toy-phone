cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(tab5_phone)

# esp_hosted 생성자 자동초기화 무력화 패치 (스냅부스·시험 펌웨어와 같은 방식, 멱등). 이유는 tools/patch_esp_hosted.py 참조.
execute_process(COMMAND python3 ${CMAKE_CURRENT_LIST_DIR}/tools/patch_esp_hosted.py RESULT_VARIABLE _eh_patch_rc)
if(NOT _eh_patch_rc EQUAL 0)
  message(FATAL_ERROR "patch_esp_hosted.py failed (rc=${_eh_patch_rc})")
endif()
