#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# WarbandVR headless test suite: unit + integration + parity.
# Usage: sh tests/run_all.sh   (from the repo root)
set -e
cd "$(dirname "$0")/.."

echo "== unit tests (pure logic)"
g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/test_main.cpp -o tests/run_tests
./tests/run_tests

echo "== integration tests (fake device, real hooks + renderer)"
g++ -std=c++17 -Wall -Wextra -Werror -Itests/winshim -Itests -Isrc \
    tests/test_integration.cpp tests/fake_device.cpp tests/winshim_impl.cpp \
    src/Logger.cpp src/Config.cpp src/Capture.cpp src/CameraScan.cpp \
    src/StereoRenderer.cpp src/DeviceHooks.cpp src/OpenXRBridge.cpp src/MouseMap.cpp src/MotionInput.cpp \
    -o tests/run_integration
./tests/run_integration

echo "== reset/robustness fuzzing (random calls + injected HRESULT failures)"
g++ -std=c++17 -Wall -Wextra -Werror -Itests/winshim -Itests -Isrc \
    tests/test_fuzz.cpp tests/fake_device.cpp tests/winshim_impl.cpp \
    src/Logger.cpp src/Config.cpp src/Capture.cpp src/CameraScan.cpp \
    src/StereoRenderer.cpp src/DeviceHooks.cpp src/OpenXRBridge.cpp src/MouseMap.cpp src/MotionInput.cpp \
    -o tests/run_fuzz
./tests/run_fuzz

echo "== motion-input tests (fake OpenXR actions + recording input sink)"
g++ -std=c++17 -Wall -Wextra -Werror -DWBVR_WITH_OPENXR \
    -Itests/winshim -Itests/xr_fake -Itests -Isrc \
    tests/test_motion_input.cpp tests/xr_fake.cpp tests/winshim_impl.cpp \
    src/Config.cpp src/Logger.cpp src/MotionInput.cpp \
    -o tests/run_motion_input
./tests/run_motion_input

echo "== host compile gates (remaining proxy/entry TUs)"
mkdir -p tests/obj
for f in src/D3D9Proxy.cpp src/D3D9ExProxy.cpp src/DeviceProxy.cpp; do
    g++ -std=c++17 -Wall -Wextra -Werror -Itests/winshim -Isrc \
        -c "$f" -o "tests/obj/$(basename "$f" .cpp).o"
done
g++ -std=c++17 -Wall -Wextra -Werror -DWBVR_SHIM_NO_EXPORT_DECLS \
    -Itests/winshim -Isrc -c src/dllmain.cpp -o tests/obj/dllmain.o

echo "== zig cross-compile gates (stub + OpenXR, x86 Windows, warnings fatal)"
python3 -m ziglang version >/dev/null
mkdir -p tests/obj/zig-stub tests/obj/zig-openxr
for f in src/*.cpp; do
    o="tests/obj/zig-stub/$(basename "$f" .cpp).o"
    python3 -m ziglang c++ -target x86-windows-gnu -c -std=c++17 \
        -Wall -Wextra -Werror -Isrc "$f" -o "$o"
done
for f in src/*.cpp; do
    o="tests/obj/zig-openxr/$(basename "$f" .cpp).o"
    python3 -m ziglang c++ -target x86-windows-gnu -c -std=c++17 \
        -DWBVR_WITH_OPENXR -Wall -Wextra -Werror \
        -Isrc -Itests/xr_fake "$f" -o "$o"
done

echo "== companion module-system source checks"
python3 tools/check_companion.py

echo "== license audit"
python3 tools/check_license.py
python3 tools/check_workflow.py
python3 -m py_compile tools/check_license.py tools/check_workflow.py tools/package_release.py

echo "== parity gates"
python3 tools/check_parity.py

echo "ALL GREEN"
