install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/com.obsproject.v4l2sink.conf
    DESTINATION "${CMAKE_INSTALL_PREFIX}/share/dbus-1/system.d"
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/com.obsproject.v4l2sink.service
    DESTINATION "${CMAKE_INSTALL_PREFIX}/share/dbus-1/system-services"
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/com.obsproject.v4l2sink.policy
    DESTINATION "${CMAKE_INSTALL_PREFIX}/share/polkit-1/actions"
)
