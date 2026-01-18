package("spout")
    add_urls("https://github.com/leadedge/Spout2/releases/download/2.007.017/Spout-SDK-binaries_2-007-017_1.zip")
    add_versions("2.007.017", "695f20e3505fa0da51b2eb959af359f5d9e2c914bb9676e9118d19f6a5424bf4")

    on_install("windows", function(package)
        os.cd("Libs_2-007-017")
        os.cp("include", package:installdir())
        local runtime = package:config("runtimes")
        if runtime == "MD" or runtime == "MDd" then 
            os.cp(path.join("MD", "bin"), package:installdir())
            os.cp(path.join("MD", "lib"), package:installdir())
        elseif runtime == "MT" or runtime == "MTd" then 
            os.cp(path.join("MT", "bin"), package:installdir())
            os.cp(path.join("MT", "lib"), package:installdir())
        end
        package:add("incluedirs", "include")
        package:add("linkdirs", "bin")
        package:add("linkdirs", "lib")
    end)
