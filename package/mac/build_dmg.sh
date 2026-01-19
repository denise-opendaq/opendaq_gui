#!/bin/bash

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}======================================${NC}"
echo -e "${GREEN}OpenDAQ Qt GUI - DMG Builder${NC}"
echo -e "${GREEN}======================================${NC}"
echo ""

# Step 1: Build main application as bundle
echo -e "${YELLOW}Step 1: Building application as macOS bundle...${NC}"

# Get the script directory and navigate to project root
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PACKAGE_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_ROOT="$(dirname "$PACKAGE_DIR")"

cd "$PROJECT_ROOT"

cmake --preset opendaq-qt-gui -DEXTRA_MODULE_PATH="../Frameworks" -DUSE_SYSTEM_QT=ON -DCMAKE_BUILD_TYPE=Release
cmake --build --preset opendaq-qt-gui --config Release

if [ $? -ne 0 ]; then
    echo -e "${RED}Failed to build application!${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Application built successfully${NC}"
echo ""

# Step 2: Configure package build
echo -e "${YELLOW}Step 2: Configuring DMG package...${NC}"
cd "$SCRIPT_DIR"
cmake -B "$PROJECT_ROOT/build/package" -S .

if [ $? -ne 0 ]; then
    echo -e "${RED}Failed to configure package!${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Package configured${NC}"
echo ""

# Step 3: Build package
echo -e "${YELLOW}Step 3: Preparing bundle...${NC}"
cmake --build "$PROJECT_ROOT/build/package"

if [ $? -ne 0 ]; then
    echo -e "${RED}Failed to prepare bundle!${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Bundle prepared${NC}"
echo ""

# Step 4: Create DMG
echo -e "${YELLOW}Step 4: Creating DMG...${NC}"
cmake --build "$PROJECT_ROOT/build/package" --target create_dmg

if [ $? -ne 0 ]; then
    echo -e "${RED}Failed to create DMG!${NC}"
    exit 1
fi

echo ""
echo -e "${GREEN}======================================${NC}"
echo -e "${GREEN}✓ DMG created successfully!${NC}"
echo -e "${GREEN}======================================${NC}"
echo ""

# Detect architecture for output message
ARCH=$(uname -m)
if [ "$ARCH" = "arm64" ] || [ "$ARCH" = "aarch64" ]; then
    ARCH_SUFFIX="arm64"
else
    ARCH_SUFFIX="x86_64"
fi

echo -e "DMG location: ${GREEN}build/package/OpenDAQ Qt GUI-1.0-${ARCH_SUFFIX}.dmg${NC}"
echo ""
