#!/bin/bash

set -ex

. $(dirname $0)/test-mmpack-common.sh
prepare_env

trap cleanup EXIT
cleanup

mmpack mkprefix --name=my_name --url=$REPO_URL $PREFIX_TEST

# test mmpack repo add
mmpack repo add other_name other_url

diff - $PREFIX_TEST/etc/mmpack-config.yaml << EOF
repositories:
  - my_name:
        url: $REPO_URL
        enabled: 1
  - other_name:
        url: other_url
        enabled: 1
EOF

mmpack repo add other_name dum_url && false || echo "Failed as expected"

# test mmpack repo list
diff - <(mmpack repo list | $dos2unix) << EOF
other_name (enabled)	other_url
my_name (enabled)	$REPO_URL
EOF

# test mmpack repo rename
mmpack repo rename other_name name

diff - $PREFIX_TEST/etc/mmpack-config.yaml << EOF
repositories:
  - my_name:
        url: $REPO_URL
        enabled: 1
  - name:
        url: other_url
        enabled: 1
EOF

mmpack repo rename other new && false || echo "Failed as expected"

# test mmpack repo disable
mmpack repo disable my_name

diff - $PREFIX_TEST/etc/mmpack-config.yaml << EOF
repositories:
  - my_name:
        url: $REPO_URL
        enabled: 0
  - name:
        url: other_url
        enabled: 1
EOF

diff - <(mmpack repo list | $dos2unix) << EOF
name (enabled)	other_url
my_name (disabled)	$REPO_URL
EOF

diff - <(mmpack list all | $dos2unix) << EOF
No package found
EOF

# test mmpack repo enable
mmpack repo enable my_name

diff - $PREFIX_TEST/etc/mmpack-config.yaml << EOF
repositories:
  - my_name:
        url: $REPO_URL
        enabled: 1
  - name:
        url: other_url
        enabled: 1
EOF

mmpack update

diff - <(mmpack list all | sort | $dos2unix) << EOF
[available] call-hello (1.0.0) from repositories: my_name
[available] hello (1.0.0) from repositories: my_name
[available] hello-data (1.0.0) from repositories: my_name
[available] hello-data (2.0.0) from repositories: my_name
EOF

# test mmpack repo remove
mmpack repo remove other && false || echo "Failed as expected"

mmpack repo remove my_name

diff - $PREFIX_TEST/etc/mmpack-config.yaml << EOF
repositories:
  - name:
        url: other_url
        enabled: 1
EOF
