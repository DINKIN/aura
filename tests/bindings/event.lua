#!/usr/bin/lua
package.cpath="./?.so;"..package.cpath
package.path="./../lua/?.lua;"..package.path

aura = require("aura");
n = aura.open("dummy", "blah");

exitcode = 1;

n.ping = function(self, arg)
    print("event", self, arg)
    exitcode = 0
end

l = aura.eventloop(n)
l:handle_events(1000);
aura.close(n);

l = nil;
n = nil;
