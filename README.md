# BCM Server

## Compile

### pull code

```bash
git clone https://github.com/bcmapp/bcm-server.git
git submodule update --init
```

### pull docker image

```bash
docker pull bcmapp/compiler-ubuntu:16.04
```
### run a compiler container

```bash
docker run -it -v ${path_to_code}:/code -w /code bcmapp/compiler-ubuntu:16.04 /bin/bash
```

### build code

```bash
make dir -p build
cd build
cmake ../
make bcm-server offline-server -j $(nproc)
```

