cmake_minimum_required(VERSION 3.28)

project(orderflow LANGUAGES C CXX)

set(USE_TLS ON CACHE BOOL "" FORCE) # Binance will reject any connection that isn't wss (encrypted)
set(USE_MBEDTLS ON CACHE BOOL "" FORCE) 
set(USE_ZLIB OFF CACHE BOOL "" FORCE)

# Help CMake find the MbedTLS
list(APPEND CMAKE_PREFIX_PATH "C:/msys64/ucrt64")

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

set(JSON_BuildTests OFF CACHE INTERNAL "")

add_subdirectory(thirdparty/nlohmann_json)
add_subdirectory(thirdparty/ixwebsocket)
add_subdirectory(thirdparty/glfw)	
add_subdirectory(thirdparty/imgui-docking)	
add_subdirectory(thirdparty/implot)

file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_executable("${CMAKE_PROJECT_NAME}")
set_property(TARGET "${CMAKE_PROJECT_NAME}" PROPERTY CXX_STANDARD 17)
target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC GLFW_INCLUDE_NONE=1) 
target_sources("${CMAKE_PROJECT_NAME}" PRIVATE ${MY_SOURCES} )

if(MSVC) # If using the VS compiler...
	target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC _CRT_SECURE_NO_WARNINGS)

	#remove console
	#set_target_properties("${CMAKE_PROJECT_NAME}" PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
	
	set_property(TARGET "${CMAKE_PROJECT_NAME}" PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDebug<$<CONFIG:Debug>:Debug>")
	set_property(TARGET "${CMAKE_PROJECT_NAME}" PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")
endif()

#force remove unicode
#if (WIN32)
	#target_compile_options("${CMAKE_PROJECT_NAME}" PRIVATE /UUNICODE /U_UNICODE)
#endif()

target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC RESOURCES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/resources/")

target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/")

target_link_libraries("${CMAKE_PROJECT_NAME}" PRIVATE 
	nlohmann_json::nlohmann_json
    ixwebsocket
    glfw
	imgui
    implot
)

# Mandatory Windows system libraries for networking
if(WIN32)
    target_link_libraries("${CMAKE_PROJECT_NAME}" PRIVATE ws2_32 crypt32 mswsock)
    # standard Windows libraries required for any app that wants to talk to the internet and check SSL certificates
endif()

# C:\msys64\usr\bin\bash.exe -lc "pacman -S --noconfirm mingw-w64-ucrt-x86_64-openssl"
# C:\msys64\usr\bin\bash.exe -lc "pacman -S --noconfirm mingw-w64-ucrt-x86_64-mbedtls"