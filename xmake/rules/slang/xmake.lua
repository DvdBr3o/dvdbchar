add_requires("slang")

rule("slang")
    set_extensions(".slang")

    on_build_file(function (target, sourcefile, opt)
        import("lib.detect.find_program")
        import("utils.progress")
        import("slangfn")

        local target_kind = target:extraconf("rules", "slang", "target_kind") or "wgsl"
        local outputdir = target:extraconf("rules", "slang", "outputdir") or path.directory(sourcefile)
        local targetfile = path.join(outputdir, path.basename(sourcefile) .. "." .. target_kind) 
        local reflfile = path.join(outputdir, path.basename(sourcefile) .. ".refl.json") 
        local layoutfile = path.join(outputdir, path.basename(sourcefile) .. ".layout.json") 
        local slangc_bindir = path.join(target:pkg("slang"):installdir(), "bin")
    
        local slangc = assert(find_program("slangc", {paths = slangc_bindir, check = "-v"}), "slangc not found!")
        if target_kind ~= "none" then 
            os.vrunv(slangc, {
                sourcefile,
                "-target", target_kind,
                "-o", targetfile,
                "-reflection-json", reflfile,
                "-I", ".",
            })
        else
            try { function()
               os.vrunv(slangc, {
                    sourcefile,
                    "-target", "spirv",
                    -- "-o", targetfile,
                    "-reflection-json", reflfile,
                    "-I", ".",
                })
                
            end,
            catch { function(err) end }
            }
        end
        local layout = slangfn.parse_layout(reflfile)
        io.writefile(layoutfile, layout)
        
        progress.show(opt.progress, "generating.slang %s", path.filename(targetfile))
    end)