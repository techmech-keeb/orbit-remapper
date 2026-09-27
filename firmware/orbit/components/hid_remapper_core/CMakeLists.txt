# The upstream HID Remapper core, built unmodified from the subtree in
# firmware/hid-remapper (decision I1). The file list is the one upstream's
# Bluetooth build uses (firmware-bluetooth/CMakeLists.txt at 51ab8b3).
set(core ${CMAKE_CURRENT_LIST_DIR}/../../../hid-remapper/firmware/src)

idf_component_register(SRCS "${core}/config.cc"
                            "${core}/crc.cc"
                            "${core}/descriptor_parser.cc"
                            "${core}/globals.cc"
                            "${core}/interval_override.cc"
                            "${core}/our_descriptor.cc"
                            "${core}/quirks.cc"
                            "${core}/remapper.cc"
                            "${core}/ps_auth.cc"
                       INCLUDE_DIRS "${core}")

# Same value as upstream's Bluetooth build.
target_compile_definitions(${COMPONENT_LIB} PUBLIC PERSISTED_CONFIG_SIZE=2048
                                                  ORBIT_UPSTREAM_COMMIT="51ab8b3")

# ESP-IDF's warning set is stricter than upstream's builds. Relax it for the
# unmodified upstream sources only (no source changes needed).
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-narrowing -Wno-missing-field-initializers)

# The core calls back into the platform layer (platform.h, remapper.h) that
# lives in main, so let the linker revisit this library.
set_property(TARGET ${COMPONENT_LIB} APPEND PROPERTY LINK_INTERFACE_MULTIPLICITY 3)
