package("dawn")
    set_homepage("https://github.com/google/dawn")
    set_description("Dawn is an open-source implementation of WebGPU")

    if is_plat("windows") then
        if is_mode("release") then
            add_urls("https://github.com/eliemichel/dawn-prebuilt/releases/download/chromium%2F7187/Dawn-7187-windows-x64-Release.zip", {verify = false})
            add_versions("1.0.7187", "3311a28f5e77d705bbd95114595f70e787bebdb756ea234211d98f48bb4ae396")
        elseif is_mode("debug") then
            add_urls("https://github.com/eliemichel/dawn-prebuilt/releases/download/chromium%2F7187/Dawn-7187-windows-x64-Debug.zip", {verify = false})
            add_versions("1.0.7187", "b5929834628b8d7b41367616fab54b93b41a9caf8b2376ef35d824bc51e8be97")
        end
    elseif is_plat("linux") then
        if is_mode("release") then
            add_urls("https://github.com/eliemichel/dawn-prebuilt/releases/download/chromium%2F7187/Dawn-7187-linux-x64-Release.zip")
            add_versions("1.0.7187", "720aa6f5553a265e2103ac9cc1fd0a7188c63af6b6bb81bebc433d62447b75bf")
        elseif is_mode("debug") then
            add_urls("https://github.com/eliemichel/dawn-prebuilt/releases/download/chromium%2F7187/Dawn-7187-linux-x64-Debug.zip")
            add_versions("1.0.7187", "6eb2f55b817f46067d1af47d7c0eacc1ff863b653ef550e59e386cd162f0c3a0")
        end
    elseif is_plat("wasm") then
        add_urls("https://github.com/eliemichel/dawn-prebuilt/releases/download/v20250903.023548-eliemichel.dawn-prebuilt.main/emdawnwebgpu_pkg-v20250903.023548-eliemichel.dawn-prebuilt.main.zip")
        add_versions("20250903.023548", "bb6b9674e97d8d184fef4c0457624633745a2c573f7149836e3bbbaed8040f61")
    end

    on_install("windows", "linux", function (package)
        os.cp("bin", package:installdir())
        os.cp("include", package:installdir())
        os.cp("lib", package:installdir())

        package:add("bindirs", "bin")
        package:add("linkdirs", "bin")
        package:add("includedirs", "include")
        package:add("linkdirs", "lib")
    end)

    on_install("wasm", function (package)
        -- os.cd("emdawnwebgpu_pkg")
        os.cp(path.join(os.scriptdir(), "emdawn.xmake.lua"), "xmake.lua")

        import("package.tools.xmake").install(package)

        os.cp("webgpu", package:installdir())
        os.cp("webgpu_cpp", package:installdir())
        package:add("includedirs", path.join("webgpu", "include"))
        package:add("includedirs", path.join("webgpu_cpp", "include"))
    end)
