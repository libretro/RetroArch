local dat_obj = nil

local function dat_lexer(f)
    local line, err = f:read("*l")
    return function()
        local tok = nil
        while not tok do
            if not line then
                return nil
            end
            tok, line = string.match(line, "^%s*(..-)(%s.*)")
            if tok and string.match(tok, "^\"") then
                tok, line = string.match(tok..line, "^\"([^\"]-)\"(.*)")
            end
            if not line then
                line = f:read("*l")
            end
        end
        return tok
    end
end

local function dat_parse_table(lexer)
    local res = {}
    local state = "key"
    local key = nil
    for tok in lexer do
        if state == "key" then
            if tok == ")" then
                return res
            else
                key = tok
                state = "value"
            end
        else
            if tok == "(" then
                res[key] = dat_parse_table(lexer)
            else
                res[key] = tok
            end
            state = "key"
        end
    end
    return res
end

local function dat_parser(lexer)
    local res = {}
    local state = "key"
    local key = nil
    local skip = true
    for tok in lexer do
        if state == "key" then
            if tok == "game" then
                skip = false
            end
            state = "value"
        else
            if tok == "(" then
                local v = dat_parse_table(lexer)
                if not skip then
                    table.insert(res, v)
                    skip = true
                end
            else
                error("Expected '(' found '"..tok.."'")
            end
            state = "key"
        end
    end
    return res
end

local function unhex(s)
    if not s then return nil end
    return (s:gsub('.', function (c)
        return string.format('%02X', string.byte(c))
    end))
end

function init(...)
    local args = {...}
    local dat_path = args[2]
    assert(dat_path, "dat file argument is missing")
    local dat_file, err = io.open(dat_path, "r")
    if err then
        error("could not open dat file '" .. dat_path .. "':" .. err)
    end

    print("Parsing dat file '" .. dat_path .. "'...")
    dat_obj = dat_parser(dat_lexer(dat_file))
    dat_file:close()
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

            esrb_rating = t.esrb_rating,
            elspa_rating = t.elspa_rating,
            pegi_rating = t.pegi_rating,
            cero_rating = t.cero_rating,

            developers = t.developers,
            publisher = t.publisher,
            origin = t.origin,

            crc = binary(unhex(t.rom.crc)),
            md5 = binary(unhex(t.rom.md5)),
            sha1 = binary(unhex(t.rom.sha1)),
            serial = binary(t.rom.serial),
        }
    end
end

