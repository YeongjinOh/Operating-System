make clean;
make;
cd build;
../../utils/pintos-mkdisk ./build/filesys.dsk --filesys-size=2;
../../utils/pintos -f -q;
make check;
