project(binbloom)

add_executable(binbloom binbloom.c)

target_compile_options(binbloom PUBLIC -Wall -Wextra -O3)

install(TARGETS binbloom DESTINATION "bin")
install(DIRECTORY IDA_script DESTINATION "share/binbloom")

#"make package" will trigger this
SET(CPACK_GENERATOR "DEB")
SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "@PapaZours")
INCLUDE(CPack)
