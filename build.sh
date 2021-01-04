rm build_output
make -C ../linux-5.4 M=`pwd` modules 2> build_output
