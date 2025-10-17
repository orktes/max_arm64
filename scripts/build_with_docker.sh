#!/bin/bash

cd .devcontainer
docker build --platform linux/arm64 -t maxpayne_builder .

cd ..
docker run --rm -v $(pwd):/workspace -w /workspace maxpayne_builder /bin/bash -c "cd /workspace && cmake . && make archive"