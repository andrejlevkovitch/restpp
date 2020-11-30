-- stress_echo.lua
--
local wrk = _G.wrk

local endpoints = {"/alpha", "/bravo", "/charlie", "/delta", "/echo",
                   "/foxtrot", "/golf", "/india", "/juliet", "/lima"}

local counter = 0
function _G.setup(thread)
  local endpoint = endpoints[(counter % #endpoints) + 1]
  thread:set("endpoint", wrk.path .. endpoint)
  thread:set("body", endpoint)
  counter = counter + 1
end

function _G.request()
  local endpoint = wrk.thread:get("endpoint")
  return wrk.format("GET", endpoint)
end

function _G.response(status, _, body)
  local check = wrk.thread:get("body")
  if status ~= 200 or body ~= check then
    print(string.format("invalid response: %d %s", status, body))
  end
end
