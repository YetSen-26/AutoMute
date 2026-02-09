cmake_minimum_required(VERSION 3.22)
project(AutoMute LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Set wxWidgets root directory
set(wxWidgets_ROOT_DIR "C:/wxWidgets-3.3.1")
set(wxWidgets_LIB_DIR "${wxWidgets_ROOT_DIR}/lib/vc_x64_lib")  # �������ı���������
set(wxWidgets_CONFIGURATION mswu)  # Unicode Release�汾

find_package(wxWidgets COMPONENTS core base adv REQUIRED)
include(${wxWidgets_USE_FILE})

add_executable(AutoMute WIN32 App.cpp MainFrame.cpp AudioSessions.cpp AutoMute.rc)
target_compile_definitions(AutoMute PRIVATE -D_UNICODE -DUNICODE)
target_link_libraries(AutoMute ${wxWidgets_LIBRARIES} ole32 uuid winmm)
