-- stress_echo.lua
--
local wrk = _G.wrk

local C_TEST_VERB = "GET" -- passible values: GET, POST

local endpoints = {"/alpha", "/bravo", "/charlie", "/delta", "/echo",
                   "/foxtrot", "/golf", "/india", "/juliet", "/lima"}

local counter = 0
function _G.setup(thread)
  local endpoint = endpoints[(counter % #endpoints) + 1]

  if C_TEST_VERB == "GET" then
    thread:set("verb", "GET")
    thread:set("endpoint", wrk.path .. endpoint)
    thread:set("req_body", nil)
    thread:set("res_body", endpoint)
  elseif C_TEST_VERB == "POST" then
    thread:set("verb", "POST");
    thread:set("endpoint", wrk.path .. "/data")
    thread:set("req_body", endpoint)
    thread:set("res_body", endpoint)
  else
    error("unsupported verb: " .. C_TEST_VERB)
  end

  counter = counter + 1
end

function _G.request()
  local verb = wrk.thread:get("verb")
  local endpoint = wrk.thread:get("endpoint")
  local body = wrk.thread:get("req_body")
  return wrk.format(verb, endpoint, nil, body)
end

function _G.response(status, _, body)
  local check = wrk.thread:get("res_body")
  if status ~= 200 or body ~= check then
    print(string.format("invalid response: %d %s", status, body))
  end
end
