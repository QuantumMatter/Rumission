zephyr_library()
zephyr_library_sources_ifdef(CONFIG_UART_SOCKET uart_socket.c)