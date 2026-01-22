add_requires("slang")

rule("slang")
    set_extensions(".slang")

    on_build_file(function (target, sourcefile, opt)
        import("lib.detect.find_program")
        import("utils.progress")

        local target_kind = target:extraconf("rules", "slang", "target_kind") or "wgsl"
        local outputdir = target:extraconf("rules", "slang", "outputdir") or path.directory(sourcefile)
        local targetfile = path.join(outputdir, path.basename(sourcefile) .. "." .. target_kind) 
        local reflfile = path.join(outputdir, path.basename(sourcefile) .. ".refl.json") 
        local slangc_bindir = path.join(target:pkg("slang"):installdir(), "bin")
    
        local slangc = assert(find_program("slangc", {paths = slangc_bindir, check = "-v"}), "slangc not found!")
        os.vrunv(slangc, {
            sourcefile,
            "-target", target_kind,
            "-o", targetfile,
            "-reflection-json", reflfile,
            "-I", ".",
        })
        
        progress.show(opt.progress, "${color.build.target}generating.$(mode) %s", path.filename(targetfile))
    end)