#!/usr/bin/python3
#
# Get list of repo contributors from GitHub using v4 GraphQL API
#
# Copyright (C) 2016-2019 - Brad Parker
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

from urllib.request import urlopen, Request
import json

### SETTINGS ###
# https://help.github.com/articles/creating-an-access-token-for-command-line-use/
TOKEN = 'Your access token here'
orgName = 'libretro'
repoName = 'RetroArch'
### END SETTINGS ###

######

lines = []

def get_contributors(after=None):
  global lines
  headers = {'Authorization': 'bearer ' + TOKEN}
  url = 'https://api.github.com/graphql'

  dataStr = """{
    repository(owner: \"""" + orgName + """\", name: \"""" + repoName + """\") {
"""

  if after:
    dataStr += """
      mentionableUsers(first: 100 after:\"""" + after + """\") {
"""
  else:
    dataStr += """
      mentionableUsers(first: 100) {
"""

  dataStr += """
        edges {
          node {
            name
            login
          }
          cursor
        }
      }
    }
  }
"""

  d = {'query': dataStr}

  data = json.dumps(d).encode('utf-8')

  req = Request(url, data, headers)

  with urlopen(req) as resp:
    d = resp.read()

  j = json.loads(d)

  if len(j['data']['repository']['mentionableUsers']['edges']) == 0:
    return None

  cursor = None

  for key in j['data']['repository']['mentionableUsers']['edges']:
    line = ''
    name = None
    login = None
    node = key['node']

    if 'name' in node and node['name'] and len(node['name']) > 0:
      name = node['name']

    if 'login' in node and node['login'] and len(node['login']) > 0:
      login = node['login']

    if 'cursor' in key and key['cursor'] and len(key['cursor']) > 0:
      cursor = key['cursor']

    if name:
      line = name

      if login and login.lower() != name.lower():
        line += ' (' + login + ')'
    elif login:
      line = login
    else:
      continue

    if len(line) > 0:
      lines.append(line)

  if cursor and len(cursor) > 0:
    return cursor

def doit(cont):
  after = get_contributors(cont)

  if after:
    doit(after)

doit(None)

print('\n'.join(sorted(lines, key=str.lower)))
