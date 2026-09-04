set_project("The-Drift.Jaeger-tech")
set_version("0.0.1")
-- 设置最小版本为：3.1.1，低于此版本的xmake编译此工程将会提示版本错误信息
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

local function cargo_build_runner(path_api, os_api)
  local manifest = path_api.join(os_api.projectdir(), "test", "runner", "Cargo.toml")
  local args = {"build", "--manifest-path", manifest}

  if is_mode("release") then
    table.insert(args, "--release")
  end

  os_api.vrunv("cargo", args)
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

target("test_runner")
  set_kind("phony")
  add_deps("server", "client")

  on_build(function(target)
      cargo_build_runner(path, os)
  end)

  on_run("lua/test_runner_run")

  after_clean(function(target)
      local project_dir = os.projectdir()
      os.rm(path.join(project_dir, "logs"))
      os.rm(path.join(project_dir, "test", "sandbox"))
  end)
