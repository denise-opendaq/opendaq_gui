#!/bin/bash

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}======================================${NC}"
echo -e "${GREEN}OpenDAQ Qt GUI - DEB Builder${NC}"
echo -e "${GREEN}======================================${NC}"
echo ""

# Step 1: Build main application
echo -e "${YELLOW}Step 1: Building application...${NC}"

# Get the script directory and navigate to project root
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PACKAGE_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_ROOT="$(dirname "$PACKAGE_DIR")"

cd "$PROJECT_ROOT"

cmake --preset opendaq-qt-gui -DCMAKE_BUILD_TYPE=Release -DEXTRA_MODULE_PATH="../lib/opendaq-qt-gui"
cmake --build --preset opendaq-qt-gui --config Release

if [ $? -ne 0 ]; then
    echo -e "${RED}Failed to build application!${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Application built successfully${NC}"
echo ""

# Step 2: Configure package build
echo -e "${YELLOW}Step 2: Configuring DEB package...${NC}"
cd "$SCRIPT_DIR"
cmake -B "$PROJECT_ROOT/build/package" -S .

if [ $? -ne 0 ]; then
    echo -e "${RED}Failed to configure package!${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Package configured${NC}"
echo ""

# Step 3: Build package
echo -e "${YELLOW}Step 3: Preparing package...${NC}"
cmake --build "$PROJECT_ROOT/build/package"

if [ $? -ne 0 ]; then
    echo -e "${RED}Failed to prepare package!${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Package prepared${NC}"
echo ""

# Step 4: Create DEB
echo -e "${YELLOW}Step 4: Creating DEB package...${NC}"
cmake --build "$PROJECT_ROOT/build/package" --target create_deb

if [ $? -ne 0 ]; then
    echo -e "${RED}Failed to create DEB!${NC}"
    exit 1
fi

echo ""
echo -e "${GREEN}======================================${NC}"
echo -e "${GREEN}✓ DEB package created successfully!${NC}"
echo -e "${GREEN}======================================${NC}"
echo ""

# Detect architecture for output message
ARCH=$(uname -m)
case "$ARCH" in
    x86_64|amd64)
        ARCH_SUFFIX="amd64"
        ;;
    aarch64|arm64)
        ARCH_SUFFIX="arm64"
        ;;
    armv7*)
        ARCH_SUFFIX="armhf"
        ;;
    armv6*)
        ARCH_SUFFIX="armel"
        ;;
    i386|i686)
        ARCH_SUFFIX="i386"
        ;;
    *)
        ARCH_SUFFIX="$ARCH"
        ;;
esac

echo -e "DEB location: ${GREEN}build/package/opendaq-qt-gui_1.0_${ARCH_SUFFIX}.deb${NC}"
echo ""

