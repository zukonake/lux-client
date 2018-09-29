local ffi = require("ffi")

local function load_header(path)
    local header = assert(io.open(path, "r"))
    ffi.cdef(header:read("*all"))
    header:close()
end

load_header("api/decl.h")

local lux_c = ffi.load("api/liblux-api.so")

lux = {}
lux.print = function(str)
    lux_c.print(tostring(str))
end

lux.mt = {}
lux.mt.__index = function(tab, k)
    if type(rawget(tab, k)) ~= "nil" then
        return rawget(tab, k)
    else
        return lux_c[k]
    end
end

function s(str)
    assert(type(str) == "string")
    lux.server_command(str)
end

setmetatable(lux, lux.mt)
