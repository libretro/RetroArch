# rarchdb
A small read only database
Mainly to be used by retroarch

# Usage
Files specified later in the chain **will override** earlier ones if the same key exists multiple times.

To list out the content of a db `rarchdb_tool <db file> list`
To create an index `rarchdb_tool <db file> create-index <index name> <field name>`
To find an entry with an index `rarchdb_tool <db file> find <index name> <value>`

# lua converters
In order to write you own converter you must have a lua file that implements the following functions:

~~~.lua
-- this function gets called before the db is created and shuold validate the
-- arguments and set up the ground for db insertion
function init(...)
	local args = {...}
	local script_name = args[1]
end

-- this is in iterator function. It is called before each insert.
-- the function should return a table for insertion or nil when there are no
-- more records to insert.
function get_value()
	return {
		key = "value", -- will be saved as string
		num = 3, -- will be saved as int
		bin = binary("some string"), -- will be saved as binary
		unum = uint(3), -- will be saved as uint
		some_bool = true, -- will be saved as bool
	}
end
~~~

# dat file converter
To convert a dat file use:

~~~
dat_converter <db file> <dat file>
~~~

If you want to merge multiple dat files you need to run:

~~~
dat_converter <db file> <match key> <dat file> ...
~~~

for example:

~~~
dat_converter snes.rdb rom.crc snes1.dat snes2.dat
~~~

