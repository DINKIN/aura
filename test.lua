#!/usr/bin/lua
package.cpath=package.cpath..";./lib?.so"
package.path=package.path..";./lua/?.lua"

aura = require("aura");
aura.slog_init(nil, 0);


node = aura.open_node("simpleusb", "./simpleusbconfigs/pw-ctl.conf");
aura.wait_status(node, 1);

print("confg1")
node:bit_set(bit32.lshift(12,8) + 1,1);
print("confg2")
node:bit_set(bit32.lshift(13,8) + 1,1);
print("confg3")
node:bit_set(bit32.lshift(14,8) + 1,1);
print("start");
node:bit_set(bit32.lshift(12,8),1);
node:bit_set(bit32.lshift(13,8),1);
node:bit_set(bit32.lshift(14,8),1);

while true do 
   node:bit_set(bit32.lshift(12,8),0);
   os.execute("sleep 1");
   node:bit_set(bit32.lshift(13,8),0);
   os.execute("sleep 1");
   node:bit_set(bit32.lshift(14,8),0);
   os.execute("sleep 1");
   node:bit_set(bit32.lshift(12,8),1);
   os.execute("sleep 1");
   node:bit_set(bit32.lshift(13,8),1);
   os.execute("sleep 1");
   node:bit_set(bit32.lshift(14,8),1);
   os.execute("sleep 1");
end

--node = aura.open("gpio", 0x1d50, 0x6032, "www.ncrmnt.org");
node = aura.open("simpleusb", "./simpleusbconfigs/pw-ctl.conf");

function cb(arg) 
   print("whoohoo");
end

node.ping = function(arg)
   print("ping",arg);
end

evloop = aura.eventloop(node)

--print(node:echo_u16(34));

--while true do
   evloop:handle_events(1500);
   --end
   evloop:handle_events(1500);
   evloop:handle_events(1500);

aura.close(node);

