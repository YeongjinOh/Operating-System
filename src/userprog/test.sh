make clean;
export TEST_EMUL='bochs';
PINTOS_PRJ='userprog';
TEST_DIR='userprog';
make;
cd build;
../../utils/pintos-mkdisk filesys.dsk --filesys-size=2;
../../utils/pintos -f -q;
make check;
