#!/usr/bin/env bash
set -euo pipefail

if ! command -v apt-get >/dev/null 2>&1; then
    echo "This script currently supports Ubuntu/Debian systems with apt-get." >&2
    exit 1
fi

required_packages=(
    build-essential
    clang
    cmake
    ccache
    ninja-build
    pkg-config
    git
    curl
    ca-certificates
    libsdl2-dev
)

maplibre_packages=(
    libcurl4-openssl-dev
    libglfw3-dev
    libuv1-dev
    libpng-dev
    libicu-dev
    libjpeg-turbo8-dev
    libwebp-dev
    xvfb
)

optional_packages=(
    doxygen
    graphviz
)

echo "==> Updating apt package index"
sudo apt-get update

echo "==> Installing required build packages"
sudo apt-get install -y "${required_packages[@]}"

echo "==> Installing required MapLibre Native Qt system packages"
sudo apt-get install -y "${maplibre_packages[@]}"

if [[ "${INSTALL_OPTIONAL:-0}" == "1" ]]; then
    echo "==> Installing optional documentation packages"
    sudo apt-get install -y "${optional_packages[@]}"
else
    echo "==> Skipping optional packages: ${optional_packages[*]}"
fi

cat <<'EOF'

System packages are installed.

This project expects Qt 6.11 from the official Qt installer rather than distro Qt packages.
After installing Qt, export QT_ROOT to the gcc_64 directory, for example:

  export QT_ROOT="$HOME/Qt/6.11.0/gcc_64"

Then build MapLibre Native Qt with:

  ./scripts/setup_maplibre_qt6.sh

EOF
