# libretrodb
A small read only database
Mainly to be used by retroarch

# Usage
Files specified later in the chain **will override** earlier ones if the same key exists multiple times.

* To list out the content of a db `libretrodb_tool <db file> list`
* To create an index `libretrodb_tool <db file> create-index <index name> <field name>`
* To find an entry with an index `libretrodb_tool <db file> find <index name> <value>`

# Compiling a single DAT into a single RDB with `c_converter`
```
git clone https://github.com/libretro/libretro-super.git
cd libretro-super
./libretro-fetch.sh retroarch
cd retroarch
./configure
cd libretro-db
c_converter "NAME_OF_RDB_FILE.rdb" "NAME_OF_SOURCE_DAT.dat"
```

# Compiling multiple DATs into a single RDB with `c_converter`
Specify `rom.crc` as the second parameter to use CRC as the unique fingerprint.
```
git clone https://github.com/libretro/libretro-super.git
cd libretro-super
./libretro-fetch.sh retroarch
cd retroarch
./configure
cd libretro-db
c_converter "NAME_OF_RDB_FILE.rdb" "rom.crc" "NAME_OF_SOURCE_DAT_1.dat" "NAME_OF_SOURCE_DAT_2.dat" "NAME_OF_SOURCE_DAT_3.dat"
```

# Compiling all RDBs with libretro-build-database.sh
**This approach builds and uses the `c_converter` program to compile the databases**

```
git clone https://github.com/libretro/libretro-super.git
cd libretro-super
./libretro-fetch.sh retroarch
./libretro-build-database.sh
```

# Lua DAT file converter
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

# Query examples
Some examples of queries you can use with libretrodbtool:

1) Glob pattern matching
Usecase : Search for all games starting with 'Street Fighter' in the 'name' field (glob pattern matching)

`libretrodb_tool <db file> find "{'name':glob('Street Fighter*')}"`

2) Combined number matching query
Usecase: Search for all games released on October 1995.

`libretrodb_tool <db file> find "{'releasemonth':10,'releaseyear':1995}"`

3) Hash matching query
Usecase: Search for any game matching a given crc32, in this case Soul Blazer (USA) for the SNES.  Also works with serial, md5, and sha1.

`libretrodb_tool <db file> find "{'crc':b'31B965DB'}"`

4) Names only search
Usecase: Search for all games released on October 1995, wont print checksums, filename or rom size, only the game name.

`libretrodb_tool <db file> get-names "{'releasemonth':10,'releaseyear':1995}"`

# Writing Lua converters
In order to write you own converter you must have a lua file that implements the following functions:

~~~.lua
-- this function gets called before the db is created and should validate the
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
