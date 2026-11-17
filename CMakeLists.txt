cmake_minimum_required(VERSION 3.21)
project(UniversalImageConverter VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Test)

# Кодеки этапа 5: libheif (HEIC/AVIF), exiv2 (метаданные), libwebp/libwebpmux (анимированный WEBP),
# libtiff (многостраничный TIFF), giflib (GIF).
find_package(PkgConfig REQUIRED)
pkg_check_modules(UIC_DEPS REQUIRED IMPORTED_TARGET libheif exiv2 libwebp libwebpmux libtiff-4)
find_library(UIC_GIF_LIB NAMES gif giflib REQUIRED)
find_path(UIC_GIF_INCLUDE gif_lib.h REQUIRED)

# Запись HEIC требует HEVC-кодировщика x265 (лицензия GPL) — по умолчанию выключена.
option(UIC_ENABLE_HEIC_ENCODE "Разрешить запись HEIC (нужен кодировщик HEVC, лицензия GPL)" OFF)

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
target_include_directories(uic_core PRIVATE ${UIC_GIF_INCLUDE})
target_link_libraries(uic_core PUBLIC Qt6::Core Qt6::Gui PkgConfig::UIC_DEPS ${UIC_GIF_LIB})
if(UIC_ENABLE_HEIC_ENCODE)
    target_compile_definitions(uic_core PRIVATE UIC_ENABLE_HEIC_ENCODE)
endif()

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
