local testlib = require 'testlib'

local DB_FILE = "/tmp/tmp.rdb"

local function create_db(data)
    local i = 1;
    testlib.create_db(DB_FILE, function()
        if i > #data then
            return
        end
        res = data[i]
        i = i + 1
        return res
    end)
    local db, err = testlib.RarchDB(DB_FILE)
    if err then
        error(err)
    end
    return db
end

local function assert_equals(a, b)
    if type(a) ~= type(b) then
        return false
    end
    if type(a) == "table" then
        for k, v in pairs(a) do
            if not assert_equals(v, b[k]) then
                return
            end
        end
        return
    else
        return assert(a == b, tostring(a) .. " != " .. tostring(b))
    end
end

function query_test(data, result, query)
    return function()
        local db = create_db(data)
        local c, err = db:query(query)
        if err then
            error(err)
        end
        local i = 0
        for item in c:iter() do
            i = i + 1
            assert_equals(item, data[i])
        end
        assert(i == #result, "expected " .. tostring(#result) .. " results got " .. tostring(i))
    end
end

tests = {
    test_list_all = function()
        data = {
            {field=true},
            {field=false},
        }
        local db = create_db(data)
        local c = db:list_all()
        local i = 1
        for item in c:iter() do
            assert_equals(item, data[i])
            i = i + 1
        end
    end,
    test_boolean_field = query_test({{a=true},{a=false}}, {{a=true}}, "{a:true}"),
    test_number_field = query_test({{a=3}, {a=4}}, {{a=3}}, "{'a':3}"),
    test_empty_query = query_test({{a=3}, {a=4}}, {{a=3}, {a=4}}, " {} "),
    test_string_field = query_test({{a="test"}, {a=4}}, {{a="test"}}, "{'a':'test'}"),
    test_or_operator = query_test({{a="test"}, {a=4}, {a=5}}, {{a="test"}, {a=4}}, "{'a':or('test', 4)}"),
    test_or_between = query_test({{a="test"}, {a=4}, {a=5}, {}}, {{a="test"}, {a=4}, {a=5}}, "{'a':or('test', between(2, 7))}"),
    test_glob = query_test({{a="abc"}, {a="acd"}}, {{a="abc"}}, "{'a':glob('*b*')}"),
    test_root_function = query_test({{a=1}, {b=4}, {a=5}, {}}, {{a=1}, {b=4}}, "or({a:1},{b:4})"),
}
for name, cb in pairs(tests) do
    local ok, err = pcall(cb)
    if ok then
        print("V", name)
    else
        print("X", name, ":", err)
    end
end
