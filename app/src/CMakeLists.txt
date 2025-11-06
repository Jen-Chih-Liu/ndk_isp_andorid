# 1. CMake 最低版本要求
cmake_minimum_required(VERSION 3.18)

# 2. 專案名稱
project(ndk_simple_project)

# 3. ★ 關鍵修正：先「定義」您的可執行檔 ★
#    (CMake 現在知道 "nu_usb_isp_android" 是什麼了)
add_executable(nu_usb_isp_android nu_usb_isp_android.c)

# 4. 找到 libusb.h 標頭檔
#    (現在我們可以安全地「設定」它)
target_include_directories(nu_usb_isp_android PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 5. 加入預先編譯好的 libusb 1.0.so 函式庫
add_library(libusb_shared SHARED IMPORTED)

# 6. 告訴 CMake .so 檔案的新路徑
set_target_properties(libusb_shared PROPERTIES
    IMPORTED_LOCATION 
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/${ANDROID_ABI}/libusb1.0.so
)

# 7. 連結函式庫
#    (現在我們也可以安全地「設定」它)
target_link_libraries(nu_usb_isp_android
    libusb_shared
    log
)