add_subdirectory(lib/xrp_sim)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/xrp.dts.o
  COMMAND dtc -O dtb
          ${CMAKE_CURRENT_SOURCE_DIR}/xrp.dts
          -o ${CMAKE_CURRENT_BINARY_DIR}/xrp.dtb
  COMMAND ${CMAKE_C_COMPILER} ${CMAKE_CURRENT_SOURCE_DIR}/xrp.dtb.s -c -o
                              ${CMAKE_CURRENT_BINARY_DIR}/xrp.dts.o
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/xrp.dtb.s
          ${CMAKE_CURRENT_SOURCE_DIR}/xrp.dts
  )

add_custom_target(xrp_dts ALL DEPENDS
                  ${CMAKE_CURRENT_BINARY_DIR}/xrp.dts.o)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/xrp.dts.o
        DESTINATION ${CMAKE_INSTALL_PREFIX}/host/lib)
