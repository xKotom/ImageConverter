cmake_minimum_required(VERSION 3.21)
project(UniversalImageConverter VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Test)

if(MSVC)
    add_compile_options(/utf-8 /W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

# --- Ядро: domain + application + infrastructure. Не зависит от Qt Widgets. ---
file(GLOB_RECURSE UIC_CORE_SOURCES CONFIGURE_DEPENDS
    src/domain/*.cpp src/application/*.cpp src/infrastructure/*.cpp)
file(GLOB_RECURSE UIC_CORE_HEADERS CONFIGURE_DEPENDS
    src/domain/*.h src/application/*.h src/infrastructure/*.h)
add_library(uic_core STATIC ${UIC_CORE_SOURCES} ${UIC_CORE_HEADERS})
target_include_directories(uic_core PUBLIC src)
target_link_libraries(uic_core PUBLIC Qt6::Core Qt6::Gui)

# --- Интерфейс ---
file(GLOB UIC_UI_SOURCES CONFIGURE_DEPENDS src/ui/*.cpp src/ui/*.h)
add_library(uic_ui STATIC ${UIC_UI_SOURCES})
target_link_libraries(uic_ui PUBLIC uic_core Qt6::Widgets)

# --- Приложение ---
add_executable(UniversalImageConverter WIN32 app/main.cpp)
target_link_libraries(UniversalImageConverter PRIVATE uic_ui)

# --- Тесты ---
enable_testing()
add_subdirectory(tests)
