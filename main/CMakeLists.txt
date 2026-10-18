set(TARGET_NAME "main")

set(SOURCES 
    main.c
    config.c
    error.c
    image.c
    refcnt.c
    scanner.c
    decoder.c
    symbol.c
    img_scanner.c
    decoder/qr_finder.c
    qrcode/qrdec.c
    qrcode/qrdectxt.c
    qrcode/rs.c
    qrcode/isaac.c
    qrcode/bch15_5.c
    qrcode/binarize.c
    qrcode/util.c
)

add_exec_vxp(${TARGET_NAME} 
    SRCS ${SOURCES}
    RESOURCES resources
    APP_NAME "${APP_NAME}"
    APPID "${APPID}"
    BACKGROUND ${BACKGROUND}
    API "${API}"
    RAM "${RAM}"
)

include_directories(${MRE_SDK}/include/service ${CMAKE_SOURCE_DIR}/include)
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${TARGET_NAME} mreapi)

target_compile_definitions(${TARGET_NAME}
    PRIVATE __MRE_SDK__)

#target_compile_options(${TARGET_NAME}
#    PRIVATE -fpermissive)
