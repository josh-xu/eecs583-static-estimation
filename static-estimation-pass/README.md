Build:

    $ cd static-estimation
    $ mkdir build
    $ cd build
    $ cmake ..
    $ make
    $ cd ..

Run:

    $ clang -Xclang -load -Xclang build/static-estimation/libStaticEstimation.* something.c
