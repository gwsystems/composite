-- file 'foo.lua'
function fib(n)
	if n < 2 then
		return n
	else
		return fib(n-1) + fib(n-2)
	end
end

list = 'list'

function linked_list(n)
	list = nil
	for i=1,n do
		list = {next = list, value = 'foobar'}
	end
end

function clear_list()
	list = nil
end

function echo_line()
	return 'hello'
end

function f(y)
	local x = y + 1
	return function() return f(x) end
end