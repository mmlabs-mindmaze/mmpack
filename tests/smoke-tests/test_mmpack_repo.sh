#!/bin/bash

set -ex

. $(dirname $0)/test-mmpack-common.sh
prepare_env

trap cleanup EXIT
cleanup

mmpack mkprefix --name=my_name --url=my_url $PREFIX_TEST

# test mmpack repo add
mmpack repo add other_name other_url

diff $PREFIX_TEST/etc/mmpack-config.yaml <(cat << EOF
repositories:
  - my_name:
        url: my_url
        enabled: 1
  - other_name:
        url: other_url
        enabled: 1
EOF
)

mmpack repo add other_name dum_url && false || echo "Failed as expected"

# test mmpack repo list
diff <(mmpack repo list | $dos2unix) <(cat << EOF
other_name (enabled)	other_url
my_name (enabled)	my_url
EOF
)

# test mmpack repo rename
mmpack repo rename other_name name

diff $PREFIX_TEST/etc/mmpack-config.yaml <(cat << EOF
repositories:
  - my_name:
        url: my_url
        enabled: 1
  - name:
        url: other_url
        enabled: 1
EOF
)

mmpack repo rename other new && false || echo "Failed as expected"

# test mmpack repo remove
mmpack repo remove other && false || echo "Failed as expected"

mmpack repo remove my_name

diff $PREFIX_TEST/etc/mmpack-config.yaml <(cat << EOF
repositories:
  - name:
        url: other_url
        enabled: 1
EOF)
