package.path = "test/?.lua;"

local luna = require "luna"

local function test_normal()
    local obj = luna.new();
    obj.nId = 5;
    assert(obj.GetId() == 5);
    luna.delete(obj);
end

local function test_diff_coroutine()
	local obj = nil;
	local co1 = coroutine.create(function ()
		obj = luna.new();
		obj.nId = 5;
	end)

	local co2 = coroutine.create(function ()
		assert(obj.GetId() == 5);
    	luna.delete(obj);
	end)

	coroutine.resume(co1);
	coroutine.resume(co2);
end

test_normal();
test_diff_coroutine();
print("test finish!");



