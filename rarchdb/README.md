# rarchdb
A small read only database
Mainly to be used by retroarch

# Usage
To convert a dat file use `dat_converter <dat file> <db file>`

To list out the content of a db `rarchdb_tool <db file> list`
To create an index `rarchdb_tool <db file> create-index <index name> <field name>`
To find an entry with an index `rarchdb_tool <db file> find <index name> <value>`

The util `mkdb.sh <dat file> <db file>` will create a db file with indexes for crc sha1 and md5

