set_arch("x64")
add_rules("mode.debug", "mode.release")
add_rules("plugin.vsxmake.autoupdate")

package("soloud")
    add_deps("cmake", "libsdl")
    set_sourcedir(path.join(os.scriptdir(), "external/soloud/contrib"))
    on_install(function (package)
        local configs = {}
        table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:debug() and "Debug" or "Release"))
        import("package.tools.cmake").install(package, configs, {packagedeps = {"libsdl"}})
    end)
    add_includedirs("include", "include/soloud")
package_end()

includes("engine")
includes("ffxi")

