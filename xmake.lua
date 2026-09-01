set_project("The-Drift.Jaeger-tech")
set_version("0.0.1")
-- 设置最小版本为：2.1.0，低于此版本的xmake编译此工程将会提示版本错误信息
set_xmakever("3.1.1")
add_rules("mode.debug", "mode.release")

-- std::string_view requires C++17.  /utf-8 also makes MSVC parse the
-- Chinese comments and other UTF-8 source text without using code page 936.
set_languages("c++17")
add_cxflags("/utf-8")

add_includedirs("include")
 
local function copy_exe(target, subdir, path_api, os_api)
  local deploy_dir = path_api.join(os_api.projectdir(), "test", subdir)

  os_api.mkdir(deploy_dir)
  os_api.cp(target:targetfile(), deploy_dir)
end

target("server")
  set_kind("binary")
  add_files("server.cpp", "fileoverview.cpp")

  after_build(function(target)
      copy_exe(target, "server_test", path, os)
  end)

target("client")
  set_kind("binary")
  add_files("client.cpp")

  after_build(function(target)
      copy_exe(target, "client_test", path, os)
  end)

target("fileoverview")
  set_kind("binary")
  add_files("fileoverview_main.cpp", "fileoverview.cpp")
