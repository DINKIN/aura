#!/usr/bin/lua
package.cpath=package.cpath..";./lib?.so"
package.path=package.path..";./lua/?.lua"

aura = require("aura");
aura.slog_init(nil, 88);

node = aura.open("dummy", 0x1d50, 0x6032, "www.ncrmnt.org");

function cb(arg) 
   print("whoohoo");
end

node.ping = function(arg)
   print("ping",arg);
end

evloop = aura.eventloop(node)

while true do
   evloop:handle_events(8500);
end

aura.close(node);

