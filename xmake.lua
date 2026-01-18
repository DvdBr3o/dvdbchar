set_project("dvdbchar")
set_version("0.1.0")

add_rules("mode.debug", "mode.release")
set_runtimes("MD") 
if is_mode("debug") then
    set_runtimes("MDd")
end

includes("xmake")

add_requires("dawn", {configs = {shared = true}})
add_requires("slang")
add_requires("spdlog")
add_requires("spout", {configs = {shared = true}})
add_requires("glfw")
add_requires("asio")
add_requires("oscpack")
add_requires("fastgltf")
add_requires("glm")

add_requires("stdexec")

target("dvdbchar.slang")
    set_kind("static")
    add_packages("slang")
    add_rules("slang")
    add_files("src/**.slang")
    
target("dvdbchar")
    set_kind("binary")
    set_languages("cxx20")
    add_rules("utils.bin2c", {extensions = ".wgsl"})

    add_deps("dvdbchar.slang")
    add_packages("dawn")
    add_packages("slang")
    add_packages("spdlog")
    add_deps("glfw3dawn")
    add_packages("spout")
    add_packages("glfw")
    add_packages("asio")
    add_packages("oscpack")
    add_packages("fastgltf")
    add_packages("glm")
    add_packages("stdexec")

    add_headerfiles("src/**.hpp")
    add_files("src/**.cpp")
    add_files("src/slang/*.wgsl")
    add_includedirs("src")

    if is_plat("windows") then
        add_defines("NOMINMAX")
    end
    
    after_build(function(target)
        os.cp("public/**", target:targetdir())
    end)
