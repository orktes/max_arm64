
FROM ghcr.io/orktes/max_amd64/retro_builder:arm64

RUN apt-get update && apt-get install -y \
    zip \
    rsync \
    sshpass \
    jq \
    && rm -rf /var/lib/apt/lists/*


RUN useradd -ms /bin/bash vscode && echo "vscode ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers
USER root
WORKDIR /workspace
