local dat_obj = {}
local match_key = nil

local function dat_lexer(f, fname)
    local line, err = f:read("*l")
    local location = {line_no = 1, column = 1, fname = fname}
    return function()
        local tok = nil
        while not tok do
            if not line then
                return nil
            end
            pre_space, tok, line = string.match(line, "^(%s*)(..-)([()]*%s.*)")
            if tok and string.match(tok, "^\"") then
                tok, line = string.match(tok..line, "^\"([^\"]-)\"(.*)")
            elseif tok and string.match(tok, "^[()]") then
                line = tok:sub(2) .. line
                tok = tok:sub(1,1)
            end
            location.column = location.column  + #(pre_space or "")
            tok_loc = {
                line_no = location.line_no,
                column = location.column,
                fname = location.fname
            }
            if not line then
                line = f:read("*l")
                location.line_no = location.line_no + 1
                location.column = 1
            else
                location.column = location.column + #tok
            end
        end
        -- print(tok)
        return tok, tok_loc
    end
end

local function dat_parse_table(lexer, start_loc)
    local res = {}
    local state = "key"
    local key = nil
    for tok, loc in lexer do
        if state == "key" then
            if tok == ")" then
                return res
            elseif tok == "(" then
                error(string.format(
                    "%s:%d:%d: fatal error: Unexpected '(' instead of key",
                    loc.fname,
                    loc.line_no,
                    loc.column
                ))
            else
                key = tok
                state = "value"
            end
        else
            if tok == "(" then
                res[key] = dat_parse_table(lexer, loc)
            elseif tok == ")" then
                error(string.format(
                    "%s:%d:%d: fatal error: Unexpected ')' instead of value",
                    loc.fname,
                    loc.line_no,
                    loc.column
                ))
            else
                res[key] = tok
            end
            state = "key"
        end
    end
    error(string.format(
        "%s:%d:%d: fatal error: Missing ')' for '('",
        start_loc.fname,
        start_loc.line_no,
        start_loc.column
    ))
end

local function dat_parser(lexer)
    local res = {}
    local state = "key"
    local key = nil
    local skip = true
    for tok, loc in lexer do
        if state == "key" then
            if tok == "game" then
                skip = false
            end
            state = "value"
        else
            if tok == "(" then
                local v = dat_parse_table(lexer, loc)
                if not skip then
                    table.insert(res, v)
                    skip = true
                end
            else
                error(string.format(
                    "%s:%d:%d: fatal error: Expected '(' found '%s'",
                    loc.fname,
                    loc.line_no,
                    loc.column,
                    tok
                ))
            end
            state = "key"
        end
    end
    return res
end

local function unhex(s)
    if not s then return nil end
    return (s:gsub('..', function (c)
        return string.char(tonumber(c, 16))
    end))
end

local function get_match_key(mk, t)
    for p in string.gmatch(mk, "(%w+)[.]?") do
        if p == nil or t == nil then
            error("Invalid match key '"..mk.."'")
        end
        t = t[p]
    end
    return t
end

table.update = function(a, b)
    for k,v in pairs(b) do
        a[k] = v
    end
end

function init(...)
    local args = {...}
    table.remove(args, 1)
    if #args == 0 then
        assert(dat_path, "dat file argument is missing")
    end

    if #args > 1 then
        match_key = table.remove(args, 1)
    end

    local dat_hash = {}
    for _, dat_path in ipairs(args) do
        local dat_file, err = io.open(dat_path, "r")
        if err then
            error("  could not open dat file '" .. dat_path .. "':" .. err)
        end

        print("  " .. dat_path)
        local objs = dat_parser(dat_lexer(dat_file, dat_path))
        dat_file:close()
        for _, obj in pairs(objs) do
            if match_key then
                local mk = get_match_key(match_key, obj)
                if mk == nil then
                    error("  missing match key '" .. match_key .. "' in one of the entries")
                end
                if dat_hash[mk] == nil then
                    dat_hash[mk] = {}
                    table.insert(dat_obj, dat_hash[mk])
                end
                table.update(dat_hash[mk], obj)
            else
                table.insert(dat_obj, obj)
            end
        end
    end
end

function get_value()
    local t = table.remove(dat_obj)
    if not t then
        return
    else
        return {
            name = t.name,
            description = t.description,
            rom_name = t.rom.name,
            size = uint(tonumber(t.rom.size)),
            users = uint(tonumber(t.users)),
            releasemonth = uint(tonumber(t.releasemonth)),
            releaseyear = uint(tonumber(t.releaseyear)),
            rumble = uint(tonumber(t.rumble)),
            analog = uint(tonumber(t.analog)),

            famitsu_rating = uint(tonumber(t.famitsu_rating)),
            edge_rating = uint(tonumber(t.edge_rating)),
            edge_issue = uint(tonumber(t.edge_issue)),
            edge_review = t.edge_review,

            enhancement_hw = t.enhancement_hw,
            barcode = t.barcode,
            esrb_rating = t.esrb_rating,
            elspa_rating = t.elspa_rating,
            pegi_rating = t.pegi_rating,
            cero_rating = t.cero_rating,
            franchise   = t.franchise,

            developer = t.developer,
            publisher = t.publisher,
            origin = t.origin,

            crc = binary(unhex(t.rom.crc)),
            md5 = binary(unhex(t.rom.md5)),
            sha1 = binary(unhex(t.rom.sha1)),
            serial = binary(t.serial or t.rom.serial),
        }
    end
end
