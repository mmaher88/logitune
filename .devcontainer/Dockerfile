FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Core build tools
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    git \
    gdb \
    clangd \
    # Qt 6 development
    qt6-base-dev \
    qt6-declarative-dev \
    qt6-svg-dev \
    qt6-tools-dev \
    qt6-tools-dev-tools \
    qt6-l10n-tools \
    qml6-module-qtquick \
    qml6-module-qtquick-controls \
    qml6-module-qtquick-layouts \
    qml6-module-qtquick-window \
    qml6-module-qtquick-templates \
    qml6-module-qtqml-workerscript \
    qml6-module-qtquick-dialogs \
    qml6-module-qt5compat-graphicaleffects \
    qt6-qpa-plugins \
    libqt6opengl6-dev \
    libqt6svg6-dev \
    libqt6dbus6 \
    libqt6widgets6 \
    libxkbcommon-dev \
    # Qt QML language server
    qml6-module-qttest \
    # System dependencies
    libudev-dev \
    # Google Test
    libgtest-dev \
    # Utilities
    sudo \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Build and install GTest (Ubuntu ships source only)
RUN cd /usr/src/googletest && cmake -B build && cmake --build build && cmake --install build

# Set up non-root user for Codespaces
# Non-root user for Codespaces (UID 1000 may exist as 'ubuntu' in base image)
RUN if ! id -u vscode >/dev/null 2>&1; then \
        useradd -m -s /bin/bash -u 1001 vscode; \
    fi \
    && echo "vscode ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers.d/vscode

# Environment for headless Qt
ENV QT_QPA_PLATFORM=offscreen
ENV CMAKE_EXPORT_COMPILE_COMMANDS=ON

USER vscode
WORKDIR /workspaces/logitune
