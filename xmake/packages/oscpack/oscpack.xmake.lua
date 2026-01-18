target("oscpack")
    set_kind("static")
    add_files("osc/**.cpp")
    add_headerfiles("osc/**.h")
    add_includedirs(".")

    add_defines("OSC_HOST_LITTLE_ENDIAN")
