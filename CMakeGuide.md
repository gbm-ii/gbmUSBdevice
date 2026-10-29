
# Using gbmUSBdevice with CMake-based projects

(initial version)

Add these to CMakeLists.txt, adjusting the values in set() commands.
COMPONENTDIR is the folder containing folders STM32_Inc and gbmUSBdevice

```
set(MCUFAMILY STM32C0)
set(COMPONENTDIR $ENV{USERPROFILE}/<put the path here>)

add_subdirectory(${COMPONENTDIR}/STM32_Inc STM32_Inc)
add_subdirectory(${COMPONENTDIR}/gbmUSBdevice/USBdev gbmusbdevice)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${COMPONENTDIR}/gbmUSBdevice/Example/Inc/${MCUFAMILY}
)
```
