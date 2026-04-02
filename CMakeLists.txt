cmake_minimum_required(VERSION 3.10)
project(GateServer VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)

# 可执行程序，每个只有自己的源文件
add_executable(syncclient ${PROJECT_SOURCE_DIR}/src/syncclient.cpp)
add_executable(syncserver ${PROJECT_SOURCE_DIR}/src/syncserver.cpp)
add_executable(Asyncserver ${PROJECT_SOURCE_DIR}/src/AsyncServer.cpp)

# 公共头文件搜索路径
set(COMMON_INCLUDES
  ${PROJECT_SOURCE_DIR}/thirdparty/boost/boost
  ${PROJECT_SOURCE_DIR}/thirdparty/boost
  ${PROJECT_SOURCE_DIR}/inc
  # ${PROJECT_SOURCE_DIR}/thirdparty/protobuf/inc
  # ${PROJECT_SOURCE_DIR}/thirdparty/protobuf/inc/google/protobuf
)

# 公共库目录
set(COMMON_LINK_DIRS
  ${PROJECT_SOURCE_DIR}/thirdparty/boost/stage/lib
  ${PROJECT_SOURCE_DIR}/thirdparty/jsoncpp/lib
  ${PROJECT_SOURCE_DIR}/thirdparty/protobuf/bin
)

# 公共库
set(COMMON_LIBS
  boost_filesystem-mgw14-mt-x64-1_84
  boost_system-mgw14-mt-x64-1_84

  ws2_32
  mswsock
  bcrypt
)

# 配置依赖
set(TARGETS syncclient syncserver Asyncserver)
foreach(t ${TARGETS})
  target_include_directories(${t} PRIVATE ${COMMON_INCLUDES})
  target_link_directories(${t} PRIVATE ${COMMON_LINK_DIRS})
  target_link_libraries(${t} ${COMMON_LIBS})
endforeach()
