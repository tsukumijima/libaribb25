# common
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "ARIB STD-B1 / ARIB STD-B25 library"
    CACHE STRING "Package description for the package metadata"
)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

# deb
SET(CPACK_PACKAGE_CONTACT "tsukumijima@users.noreply.github.com")
SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "tsukumi")
set(CPACK_DEB_COMPONENT_INSTALL YES)

# deb パッケージのファイル名を Debian の標準に合わせる
# ref: https://stackoverflow.com/a/59202791/17124142
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

# CPack は最後にインポートする
include(CPack)
