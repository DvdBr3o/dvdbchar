if is_plat("windows", "macos", "linux") then

add_requires("glfw")
add_requires("dawn")

target("glfw3dawn")
    set_languages("cxx20")
    set_kind("static")
    add_packages("glfw", {public = true})
    add_packages("dawn", {public = true})
    add_files("src/utils.cpp")
    add_headerfiles("src/webgpu/webgpu_glfw.h", {public = true})
    add_includedirs("src", {public = true})

    if is_plat("wasm") then
        add_files("src/utils_emscripten.cpp")
    end
end