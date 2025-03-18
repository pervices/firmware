#!/bin/bash

DOCKER_ID=$(docker run -dit --volume $(pwd):$(pwd) --workdir $(pwd) pvtesting/archlinux:20230619.0.159280 /bin/bash)
docker exec -d $DOCKER_ID git config --global --add safe.directory $(pwd)
docker attach $DOCKER_ID
