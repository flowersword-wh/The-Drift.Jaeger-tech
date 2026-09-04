import("core.base.option")

function main(target)
  local profile = is_mode("release") and "release" or "debug"
  local executable = path.join(
    os.projectdir(), "test", "runner", "target", profile, "runner.exe"
  )
  local runargs = {}
  for _, argument in ipairs(table.wrap(option.get("arguments") or {})) do
    if argument ~= "--" then
      table.insert(runargs, argument)
    end
  end

  os.execv(executable, runargs)
end
