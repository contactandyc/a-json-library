# BUILDING

This project: **A JSON Library**
Version: **0.1.0**

## Local build

```bash
# one-shot build + install
./build.sh install
```

Or run the steps manually:

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j"$(nproc || sysctl -n hw.ncpu || echo 4)"
sudo cmake --install .
```



## Install dependencies (from `deps.libraries`)



### Development tooling (optional)

```bash
sudo apt-get update && sudo apt-get install -y autoconf automake gdb libtool perl python3 python3-pip python3-venv valgrind
```



### the-macro-library

Clone & build:

```bash
git clone --depth 1 --single-branch "https://github.com/contactandyc/the-macro-library.git" "the-macro-library"
cd "the-macro-library"
./build.sh clean
./build.sh install
cd ..
rm -rf "the-macro-library"
```


### a-memory-library

Clone & build:

```bash
git clone --depth 1 --single-branch "https://github.com/contactandyc/a-memory-library.git" "a-memory-library"
cd "a-memory-library"
./build.sh clean
./build.sh install
cd ..
rm -rf "a-memory-library"
```


### a-json-sax-library

Clone & build:

```bash
git clone --depth 1 --single-branch "https://github.com/contactandyc/a-json-sax-library.git" "a-json-sax-library"
cd "a-json-sax-library"
./build.sh clean
./build.sh install
cd ..
rm -rf "a-json-sax-library"
```

