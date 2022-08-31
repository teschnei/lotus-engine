add_requires("stb")

rule("glslang")
    set_extensions(".comp", ".vert", ".frag", ".rgen", ".rmiss", ".rchit", ".rahit", ".rint")
    on_build_file(function (target, sourcefile, opt)
        import("core.project.depend")
        import("utils.progress")

        os.mkdir(path.join(target:targetdir(), "shaders"))

        local targetfile = path.join(target:targetdir(), "shaders", path.basename(sourcefile) .. ".spv")

        -- TODO: scan for includes
        depend.on_changed(function ()
            os.vrunv('glslangvalidator', { "--target-env", "vulkan1.3", "-V", sourcefile, "-o", targetfile})
            progress.show(opt.progress, "${color.build.object}glslangvalidator %s", sourcefile)
        end, {files = sourcefile})
    end)

rule("texture")
    set_extensions(".png")
    on_build_file(function (target, sourcefile, opt)
        import("core.project.depend")
        import("utils.progress")

        local targetdir = path.join(target:targetdir(), "textures")
        os.mkdir(targetdir)

        depend.on_changed(function ()
            os.cp(sourcefile, targetdir, {rootdir="ffxi/textures"})
            progress.show(opt.progress, "${color.build.object}texture %s", sourcefile)
        end, {files = sourcefile})
    end)

target("ffxi")
    set_kind("binary")
    set_languages("cxxlatest")
    add_packages("stb")
    add_deps("lotus-engine")
    add_rules("glslang", "texture")
    add_files("**.cpp", "**.comp", "**.vert", "**.frag", "**.rgen", "**.rmiss", "**.rchit", "**.rahit", "**.rint")
    add_files("textures/**.png")
    add_headerfiles("**.h", {install=false})
    add_syslinks("Advapi32")
    add_includedirs(".")
