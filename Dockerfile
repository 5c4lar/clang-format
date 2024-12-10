FROM ubuntu:24.04 AS clang-format
RUN apt update && apt install -y clang libclang-cpp-dev libclang-dev llvm-dev swig python3.12 ninja-build cmake python3.12-dev
ADD . /work
WORKDIR /work
RUN cmake -GNinja -Bbuild . -DCMAKE_BUILD_TYPE=Release && ninja -C build
CMD ["/work/build/clang_format_main"]