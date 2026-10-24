set(TARGET_NAME "main")

add_library(${TARGET_NAME} STATIC 
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

include_directories(${MRE_SDK}/include/service ${CMAKE_SOURCE_DIR}/include)
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${TARGET_NAME} mreapi)

target_compile_definitions(${TARGET_NAME}
    PRIVATE
    __MRE_SDK__=1
)
