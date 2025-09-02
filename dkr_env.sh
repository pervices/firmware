#!/bin/bash

PV_DOCKER=pvtesting/archlinux:20230619.0.159280

# The container will be used to compile the server, so we want the generated files accessable by the host.
# So, create a volume bound to this directory and set the workspace to it so all generated files are shared.
dkr () {
	CMD=$@
	echo $CMD
	docker run --rm -i \
                   -v $(pwd):$(pwd) \
                   -w $(pwd) \
                   -e GGID=$(id -g) \
                   -e UUID=$(id -u) \
                   $PV_DOCKER \
                   /bin/bash -c "$CMD"
}
