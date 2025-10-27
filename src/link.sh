cd /mnt/c/raylib/dib/src

rm libdibapp.so
g++ -Wl,--no-as-needed *.so -o libdibapp.so -shared -fPIC