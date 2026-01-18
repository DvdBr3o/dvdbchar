package("stdexec")
    set_kind("library", {headeronly = true})
    set_homepage("https://github.com/NVIDIA/stdexec")
    set_description("`std::execution`, the proposed C++ framework for asynchronous and parallel programming. ")
    set_license("Apache-2.0")

    add_urls("https://github.com/NVIDIA/stdexec.git")

    add_versions("2024.03.08", "b3ba13a7b8c206371207196e08844fb7bc745438")
    add_versions("2024.09.20", "519f931ef9d44ca6cc8b2c8a3b3cb760a3523d4f")
    add_versions("2025.07.12", "2674bebb8389ce2faaadbea9c1d230b98ee56770")
    add_versions("2025.07.22", "0d0a15c669cbd2571248536a7e70524d8b7071aa")
    add_versions("2026.01.04", "250f35737790392d666fd157e0af9be16d0c789f")
    
    add_configs("asio", {description = "Enable asioexec", default = false, type = "boolean"})
    add_configs("asio_implementation", {
        description = "Choose asio implementation of asioexec.", 
        default = "boost", 
        values = {
            "boost",
            "standalone",
        },
    })

    add_deps("cmake")

    if on_check then
        on_check("windows", function (package)
            if package:version():ge("3.0.0") then
                import("core.base.semver")

                local vs_toolset = package:toolchain("msvc"):config("vs_toolset")
                assert(vs_toolset and semver.new(vs_toolset):minor() >= 30, "package(stdexec): need vs_toolset >= v143")
            end
        end)
    end

    on_install("windows", "linux", "macosx", "mingw", function (package)
        if package:has_tool("cxx", "cl") then
            package:add("cxflags", "/Zc:__cplusplus", "/Zc:preprocessor", { public = true })
        end

        local configs = {
            "-DSTDEXEC_BUILD_EXAMPLES=OFF", 
            "-DSTDEXEC_BUILD_TESTS=OFF", 
            "-DSTDEXEC_BUILD_DOCS=OFF", 
            "-DCMAKE_CXX_STANDARD=20",
        }
        table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:is_debug() and "Debug" or "Release"))
        table.insert(configs, "-DBUILD_SHARED_LIBS=" .. (package:config("shared") and "ON" or "OFF"))
        table.insert(configs, "-DSTDEXEC_ENABLE_ASIO=" .. (package:config("asio") and "ON" or "OFF"))
        if package:config("asio") then
            table.insert(configs, "-DSTDEXEC_ASIO_IMPLEMENTATION=" .. package:config("asio_implementation"))
        end
        import("package.tools.cmake").install(package, configs)

        os.cp(path.join("include", "asioexec"), package:installdir("include"))
        os.cp(path.join("include", "execpools"), package:installdir("include"))
    end)

    on_test(function (package)
      assert(package:has_cxxincludes("exec/static_thread_pool.hpp", {configs = {languages = "c++20"}}))
      assert(package:check_cxxsnippets("static_assert(__cplusplus >= 202002L);", {configs = {languages = "c++20"}}))
  end)
