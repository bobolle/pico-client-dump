# pico-client-dump

exploratory space for iot group project.

## requirements

- pico-sdk
- pico-w
- stuff

## building

```shell
git clone git@github.com:bobolle/pico-client-dump.git
git submodule update --init --recursive

# edit CMakeLists.txt

cmake -B build
make -C build

# start pico in bootsel mode & mount the .uf2
sudo mount /path/to/pico /path/to/mountpoint
sudo cp .uf2 /path/to/mountpoint
sudo umount /path/to/mountpoint
```
