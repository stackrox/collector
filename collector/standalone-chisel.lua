-- Chisel description
description = "only display events relevant to security modeling and detection"
short_description = "security relevant"
category = "misc"

-- Chisel argument list
args = {}

-- Event parsing callback
function on_event()
    return true
end

function on_init()
    filter = "not container.id = 'host'\n"
    chisel.set_filter(filter)
    return true
end
