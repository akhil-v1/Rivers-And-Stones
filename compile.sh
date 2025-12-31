#!/bin/bash

# Build script for s1 C++ module
# This script compiles the complete C++ implementation of the Student Agent

set -e  # Exit on any error

echo "=========================================="
echo "Building s1 C++ Module for Student Agent"
echo "=========================================="

# Check if pybind11 is installed
echo "Checking pybind11 installation..."
python3 -c "import pybind11; print(f'pybind11 version: {pybind11.__version__}')" || {
    echo "Error: pybind11 is not installed!"
    echo "Please install it with: pip install pybind11"
    exit 1
}

# Check if cmake is available
echo "Checking cmake installation..."
cmake --version || {
    echo "Error: cmake is not installed!"
    echo "Please install cmake first."
    exit 1
}

# Check if g++ is available
echo "Checking C++ compiler..."
g++ --version || {
    echo "Error: g++ is not installed!"
    echo "Please install g++ first."
    exit 1
}

# Create build directory
echo "Creating build directory..."
if [ -d "build" ]; then
    echo "Removing existing build directory..."
    rm -rf build
fi
mkdir build
cd build

# Configure with cmake
echo "Configuring with cmake..."
cmake .. \
    -Dpybind11_DIR=$(python3 -m pybind11 --cmakedir) \
    -DCMAKE_C_COMPILER=gcc \
    -DCMAKE_CXX_COMPILER=g++ \
    -DCMAKE_BUILD_TYPE=Release

# Build the modules
echo "Building modules..."
make -j$(nproc 2>/dev/null || echo 4)

if ls student_agent_module*.so 1> /dev/null 2>&1; then
    echo "✓ student_agent_module built successfully!"
else
    echo "✗ student_agent_module build failed!"
    exit 1
fi

# Copy modules to parent directory for easy import
echo "Copying modules to parent directory..."
cp student_agent_module*.so .. 2>/dev/null || echo "Warning: Could not copy student_agent_module"

cd ..

echo ""
echo "=========================================="
echo "Build completed successfully!"
echo "=========================================="
echo ""
echo "Available modules:"
echo "  - student_agent_module: Original simple implementation"
echo ""
echo "Usage:"
echo "  python s1_cpp.py                    # Test the s1 module"
echo "  python gameEngine.py --mode aivai --circle s1_cpp --square random"
echo ""
echo "Files created:"
ls -la *.so 2>/dev/null || echo "  (No .so files found in current directory)"
echo ""
