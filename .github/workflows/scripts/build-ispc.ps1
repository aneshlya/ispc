cmake -B build -DISPC_PREPARE_PACKAGE=ON -DISPC_CROSS=ON -DISPC_INCLUDE_BENCHMARKS=ON -DCMAKE_BUILD_TYPE="${env:BUILD_TYPE}" -DISPC_GNUWIN32_PATH="${env:CROSS_TOOLS_GNUWIN32}" -DISPC_PACKAGE_NAME=ispc-trunk-windows @args
cmake --build build --target package -j4 --config ${env:BUILD_TYPE}
