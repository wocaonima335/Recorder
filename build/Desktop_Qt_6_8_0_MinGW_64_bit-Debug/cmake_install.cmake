# Install script for directory: E:/myProgram/Recorder/record

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/bandicam")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "F:/Qt/Tools/mingw1310_64/bin/objdump.exe")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("E:/myProgram/Recorder/record/build/Desktop_Qt_6_8_0_MinGW_64_bit-Debug/demuxer/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("E:/myProgram/Recorder/record/build/Desktop_Qt_6_8_0_MinGW_64_bit-Debug/qml/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("E:/myProgram/Recorder/record/build/Desktop_Qt_6_8_0_MinGW_64_bit-Debug/queue/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("E:/myProgram/Recorder/record/build/Desktop_Qt_6_8_0_MinGW_64_bit-Debug/decoder/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("E:/myProgram/Recorder/record/build/Desktop_Qt_6_8_0_MinGW_64_bit-Debug/resampler/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("E:/myProgram/Recorder/record/build/Desktop_Qt_6_8_0_MinGW_64_bit-Debug/filter/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("E:/myProgram/Recorder/record/build/Desktop_Qt_6_8_0_MinGW_64_bit-Debug/muxer/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("E:/myProgram/Recorder/record/build/Desktop_Qt_6_8_0_MinGW_64_bit-Debug/event/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("E:/myProgram/Recorder/record/build/Desktop_Qt_6_8_0_MinGW_64_bit-Debug/thread/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("E:/myProgram/Recorder/record/build/Desktop_Qt_6_8_0_MinGW_64_bit-Debug/recorder/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("E:/myProgram/Recorder/record/build/Desktop_Qt_6_8_0_MinGW_64_bit-Debug/encoder/cmake_install.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "E:/myProgram/Recorder/record/build/Desktop_Qt_6_8_0_MinGW_64_bit-Debug/appbandicam.exe")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/appbandicam.exe" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/appbandicam.exe")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "F:/Qt/Tools/mingw1310_64/bin/strip.exe" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/appbandicam.exe")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE FILE FILES
    "E:/myProgram/Recorder/record/3rdparty/ffmpeg-amf/bin/avcodec-62.dll"
    "E:/myProgram/Recorder/record/3rdparty/ffmpeg-amf/bin/avdevice-62.dll"
    "E:/myProgram/Recorder/record/3rdparty/ffmpeg-amf/bin/avfilter-11.dll"
    "E:/myProgram/Recorder/record/3rdparty/ffmpeg-amf/bin/avformat-62.dll"
    "E:/myProgram/Recorder/record/3rdparty/ffmpeg-amf/bin/avutil-60.dll"
    "E:/myProgram/Recorder/record/3rdparty/ffmpeg-amf/bin/swresample-6.dll"
    "E:/myProgram/Recorder/record/3rdparty/ffmpeg-amf/bin/swscale-9.dll"
    )
endif()

if(CMAKE_INSTALL_COMPONENT)
  set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "E:/myProgram/Recorder/record/build/Desktop_Qt_6_8_0_MinGW_64_bit-Debug/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
