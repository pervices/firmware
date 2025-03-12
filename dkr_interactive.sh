#!/bin/bash

docker run -it --volume $(pwd):$(pwd) --workdir $(pwd) pvtesting/archlinux:20230619.0.159280 /bin/bash
