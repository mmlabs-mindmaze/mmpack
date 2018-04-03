#!/usr/bin/env python3

import sys
from git import Repo
import yaml

# find current repo from current directory
repo = Repo(path=None, search_parent_directories=True)

# get last tag
tag = repo.tags[0]
print('using tag:', tag.name, tag.commit.hexsha)

# go through arc from tag to top of branch (tag..HEAD)
last_sha1 = repo.head.reference.log()[-1].newhexsha
tag_sha1 = tag.commit.hexsha

# '%cd': committer date (format respects --date= option)
# '%an': author name
# '%ae': author email
# '%s': subject
log_csv = repo.git.log(tag_sha1, last_sha1,
                       format='%cd, %an <%ae>, %s', date='short')

log_list_dict = []
for _entry in log_csv.split('\n'):
    entry = _entry.split(',')
    log_list_dict.append({'date': entry[0], 'author': entry[1], 'message': entry[2:]})


with open('changelog.yaml', 'w+') as f:
    yaml.dump(log_list_dict, f, default_flow_style=False)
