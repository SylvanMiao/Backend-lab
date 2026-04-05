cmake_minimum_required(VERSION 3.10)
project(GateServer VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 第三方目录位于上一级 Backend-lab
get_filename_component(BACKEND_LAB_DIR "${PROJECT_SOURCE_DIR}/.." ABSOLUTE)
set(THIRDPARTY_DIR "${BACKEND_LAB_DIR}/thirdparty")

# Server 程序源文件（含 main）
set(SERVER_SOURCES
  ${PROJECT_SOURCE_DIR}/src/Server.cpp
  ${PROJECT_SOURCE_DIR}/src/Session.cpp
  ${PROJECT_SOURCE_DIR}/src/MsgNode.cpp
  ${PROJECT_SOURCE_DIR}/src/logicsystem.cpp
)

# 可执行程序
add_executable(Asyncserver ${SERVER_SOURCES})
add_executable(JsonClient ${PROJECT_SOURCE_DIR}/src/JsonClient.cpp)

# 公共头文件搜索路径
set(COMMON_INCLUDES
  ${THIRDPARTY_DIR}/boost/boost
  ${THIRDPARTY_DIR}/boost
  ${THIRDPARTY_DIR}/jsoncpp/include
  ${PROJECT_SOURCE_DIR}/inc
  # ${THIRDPARTY_DIR}/protobuf/inc
  # ${THIRDPARTY_DIR}/protobuf/inc/google/protobuf
)

# 公共库目录
set(COMMON_LINK_DIRS
  ${THIRDPARTY_DIR}/boost/stage/lib
  ${THIRDPARTY_DIR}/jsoncpp/lib
  ${THIRDPARTY_DIR}/protobuf/bin
)

# 公共库
set(COMMON_LIBS
  boost_filesystem-mgw14-mt-x64-1_84
  boost_system-mgw14-mt-x64-1_84
  jsoncpp_static

  ws2_32
  mswsock
  bcrypt
)

# 配置依赖
set(TARGETS Asyncserver JsonClient)
foreach(t ${TARGETS})
  target_include_directories(${t} PRIVATE ${COMMON_INCLUDES})
  target_link_directories(${t} PRIVATE ${COMMON_LINK_DIRS})
  target_link_libraries(${t} PRIVATE ${COMMON_LIBS})
endforeach()
