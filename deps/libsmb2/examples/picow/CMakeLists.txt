cmake_minimum_required(VERSION 3.24)

# Some user configuration needed below

# Get the Pico SDK from https://github.com/raspberrypi/pico-sdk.git tested on TAG 1.4.0
# Get FreeRTOS from https://github.com/FreeRTOS/FreeRTOS-Kernel.git tested on TAG V10.5.0
# Replace the /path/to's below with appropriate paths
# Something like /home/me/dve/github/pico_sdk, but wherever you put the pico-sdk or freertos-kernel
set(PICO_SDK_PATH /path/to/pick-sdk) 
set(FREERTOS_KERNEL_PATH /path/to/freertos-kernel)
set(LIBSMB2_PATH ${CMAKE_CURRENT_LIST_DIR}/../..)

# Replace with the name of your WiFi (SSID) and the password to join that WiFi
set(WIFI_SSID ssid_name)
set(WIFI_PASSWORD wifi_name)

# Put in the URL of the server/share you want to see the directory of
set(SMB2_URL smb://myserver.lan/myshare)

# No other configuration needed, below this
# Name the project and prepare it to be a pico_w project, using FreeRTOS and lwip
set(PROJECT_NAME libsmb2-picow-ls)
set(PICO_BOARD pico_w)
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)
project(${PROJECT_NAME} C CXX ASM)
pico_sdk_init()

# Add a couple of definitions that all projects/libraries can/should use
add_definitions(-DPICO_PLATFORM=${PICO_PLATFORM})
add_definitions(-DHAVE_CONFIG_H)


# Build libsmb2 as a library
add_subdirectory(${LIBSMB2_PATH} libsmb2)

# The example
set(PROJECT_NAME smb2-ls-sync)
project(${PROJECT_NAME} C CXX ASM)

# Application, including in the FreeRTOS-Kernel
add_executable(${PROJECT_NAME} main.cpp)

# Build the app
include_directories(
        ${CMAKE_CURRENT_LIST_DIR}
        ${FREERTOS_KERNEL_PATH}/include
        ${LIBSMB2_PATH}/include
        ${LIBSMB2_PATH}/include/smb2
        ${LIBSMB2_PATH}/include/picow
)

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)
pico_enable_stdio_semihosting(${PROJECT_NAME} ENABLED)

target_compile_definitions(${PROJECT_NAME} PRIVATE
        WIFI_SSID=\"${WIFI_SSID}\"
        WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
        SMB2_URL=\"${SMB2_URL}\"
        NO_SYS=0            # don't want NO_SYS (generally this would be in your lwipopts.h)
)

target_link_libraries(${PROJECT_NAME}
        pico_cyw43_arch_lwip_sys_freertos
        FreeRTOS-Kernel
        pico_stdlib
        libsmb2
        FreeRTOS-Kernel-Heap4
)

pico_add_extra_outputs(${PROJECT_NAME})
