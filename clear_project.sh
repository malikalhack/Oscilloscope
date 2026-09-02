#!/usr/bin/env bash
#
# Remove generated build artifacts for the current project.
# Cleans the Output directory and optional docs output folder.
#
# Author: Anton Chernov
# Date: 2026-08-28

clear

out_app="./Output"
out_docs="./docs/out"
pycache="./__pycache__"

# Clean application output directory
if [ -d "$out_app" ]; then
    echo "Cleaning $out_app..."
    rm -rfv "$out_app"/*
fi

# Clean Python bytecode cache directory
if [ -d "$pycache" ]; then
    echo -e "\nCleaning $pycache..."
    rm -rfv "$pycache"
fi

# Clean docs output directory
if [ -d "$out_docs" ]; then
    echo -e "\nCleaning $out_docs..."
    rm -rfv "$out_docs"
fi

echo -e "\nCleanup complete!"
