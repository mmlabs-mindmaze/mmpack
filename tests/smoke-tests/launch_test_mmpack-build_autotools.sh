#!/bin/bash

for build_sys in 'autotools' 'meson'
do
	$(dirname $0)/test_mmpack-build.sh $build_sys
done
