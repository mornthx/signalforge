add_library(signalforge_buffer STATIC
    signal_buffer.cpp
    signal_buffer.hpp
    signal_buffer_registry.cpp
    signal_buffer_registry.hpp
)
target_include_directories(signalforge_buffer PUBLIC ${CMAKE_SOURCE_DIR}/src)
# Note: signalforge_buffer references decoder_interface.hpp which is
# entirely header-only (pure-virtual + struct layouts). Linking
# signalforge_decoder is therefore not needed; consumers that want
# decoder symbols link signalforge_decoder directly. Dropping it here
# also breaks the would-be cycle introduced by S11.5 — signalforge_decoder
# now PRIVATE-links signalforge_buffer for the decoder-side buffer cache.
target_link_libraries(signalforge_buffer
    PUBLIC
        Qt6::Core
    PRIVATE
        signalforge_observability
)
