package("oscpack")
    add_urls("https://github.com/MariadeAnton/oscpack.git")
    add_versions("1.1.0", "1cb90c372a182ac13b9a5f68b938c06ade851836")

    on_install(function(package)
        os.cp(path.join(os.scriptdir(), "oscpack.xmake.lua"), "xmake.lua")
        import("package.tools.xmake").install(package)

        os.cp("osc", path.join(package:installdir(), "include"))
        package:add("includedirs", "include")
    end)