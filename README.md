indri-http-server
====================

A simplistic Indri server over HTTP that supports JSON output

### Installation ###

This package depends on:

1. An update-to-date version of g++ that supports C++0x 
2. Boost library >= 1.46
3. [cpp-netlib][cpp-netlib] 0.11.2

To install this package to your home directory, do:

    ./configure --prefix=$HOME
    make && make install

(Use options `--with-indri` and `--with-boost` to specify alternative installation paths for Indri and Boost.)

[Indri]: http://www.lemurproject.org/indri.php
[cpp-netlib]: http://cpp-netlib.org/
