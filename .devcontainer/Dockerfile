
FROM monkeyx/retro_builder:arm64

RUN apt-get update && apt-get install -y \
    ffmpeg \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswscale-dev \
    libavfilter-dev \
    libavdevice-dev \
    libfontconfig1 \
    libfontconfig1-dev \
    zip \
    rsync \
    sshpass \
    && rm -rf /var/lib/apt/lists/*


RUN useradd -ms /bin/bash vscode && echo "vscode ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers
USER root
WORKDIR /workspace
