cmake_minimum_required(VERSION 3.8)

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(CMAKE_C_COMPILER "/usr/bin/clang")
    set(CMAKE_CXX_COMPILER "/usr/bin/clang++")
    set(CMAKE_CXX_FLAGS "-fno-rtti -g -pipe -W -Wall -fPIC")
endif()

file(GLOB all_src
	./*.cpp
	./base/*.cpp
	./media/*.cpp
	./device/*.cpp
	./tools/*.cpp
	./render/*.cpp
	./render/win/*.cpp
    ./codec/*.cpp
    ./codec/encoder/*.cpp
    ./codec/nvcodec/NvEncoder/NvEncoder.cpp
    ./codec/nvcodec/NvEncoder/NvEncoderD3D11.cpp
    ./codec/qsvcodec/*.cpp
    ./codec/qsvcodec/src/*.cpp
)

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    file(GLOB exclude_src
        ./render/win/*.cpp
        ./codec/*.cpp
        ./codec/encoder/*.cpp
        ./codec/nvcodec/NvEncoder/NvEncoder.cpp
        ./codec/nvcodec/NvEncoder/NvEncoderD3D11.cpp
        ./codec/qsvcodec/*.cpp
        ./codec/qsvcodec/src/*.cpp
    )
    list(REMOVE_ITEM all_src ${exclude_src})
endif()

include_directories(
    ${KRTC_DIR}
    ${KRTC_THIRD_PARTY_DIR}/include
    ${WEBRTC_INCLUDE_DIR}
    ${WEBRTC_INCLUDE_DIR}/third_party/abseil-cpp
    ${WEBRTC_INCLUDE_DIR}/third_party/libyuv/include
    ${WEBRTC_INCLUDE_DIR}/third_party/libyuv/include/libyuv
    ${WEBRTC_INCLUDE_DIR}/third_party/jsoncpp/source/include
    ${KRTC_DIR}/xrtc
	${KRTC_DIR}/xrtc/codec
	${KRTC_DIR}/xrtc/codec/encoder
    ${KRTC_DIR}/xrtc/codec/nvcodec
	${KRTC_DIR}/xrtc/codec/qsvcodec
	${KRTC_DIR}/xrtc/codec/qsvcodec/include
)

link_directories(
    ${WEBRTC_LIB_DIR}
    ${KRTC_THIRD_PARTY_DIR}/lib
)

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
    add_definitions(-DKRTC_API_EXPORT
        -DWEBRTC_WIN
        -DCURL_STATICLIB
        -DNOMINMAX
        -D_CRT_SECURE_NO_WARNINGS
        -DUNICODE
        -D_UNICODE
        -DWIN32_LEAN_AND_MEAN
        -DUSE_DTLS=1
        -DOPENSSL_NO_ASM
    )
elseif (CMAKE_SYSTEM_NAME MATCHES "Linux")
    add_definitions(-DKRTC_API_EXPORT
        -DWEBRTC_POSIX
        -DWEBRTC_LINUX
        -DUSE_GLIB=1)
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
    add_library(xrtc SHARED ${all_src} "base/xrtc_json.cpp" "base/xrtc_json.h")
elseif (CMAKE_SYSTEM_NAME MATCHES "Linux")
    add_library(xrtc_static STATIC ${all_src})
    set_target_properties(xrtc_static PROPERTIES  OUTPUT_NAME "xrtc")
    set_target_properties(xrtc_static PROPERTIES CLEAN_DIRECT_OUTPUT 1)

    add_library(xrtc SHARED ${all_src})
    set_target_properties(xrtc PROPERTIES  OUTPUT_NAME "xrtc")
    set_target_properties(xrtc PROPERTIES CLEAN_DIRECT_OUTPUT 1)
    set_target_properties(xrtc PROPERTIES VERSION 1.0 SOVERSION 1)
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_link_libraries(xrtc
        winmm
	    ws2_32
	    strmiids
	    wldap32
	    crypt32
	    iphlpapi
	    msdmo
	    dmoguids
	    wmcodecdspuuid
	    secur32
	    d3d11
        dxva2
	    dxgi
       
        ${WEBRTC_LIB_DIR}/webrtc.lib
        ${KRTC_THIRD_PARTY_DIR}/lib/libcurl.lib
        ${KRTC_THIRD_PARTY_DIR}/lib/libssl.lib
        ${KRTC_THIRD_PARTY_DIR}/lib/libcrypto.lib
        ${KRTC_THIRD_PARTY_DIR}/lib/jsoncpp_static.lib
    )
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_link_libraries(xrtc
        -lcurl
        -lwebrtc
        -ljsoncpp
        -lpthread 
        -ldl
	)
endif()
