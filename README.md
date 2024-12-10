# clang-format
Clang format as a python function

## Build
Build clang-format following the Dockerfile or build the docker file and try it:
```bash
docker build -t clang-format .
docker run -it clang-format
# Input your code and end it with Ctrl-D
```

## Usage

Put clang_format.so in the directory and run:
```python
from clang_format import format as clang_format
formated = clang_format("<your code>")
```
