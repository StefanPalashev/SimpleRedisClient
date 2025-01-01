# Simple Redis Client

## Overview
The Simple Redis Client is a command-line application that subscribes to a Redis channel and processes messages in real-time using the Publish/Subscribe messaging pattern. It receives and handles messages from the channel it is subscribed to, making it ideal for real-time communication between different parts of a system. With its configurable settings, the Simple Redis Client can be easily adapted to meet the needs of various use cases and environments. Additionally, the program supports monitoring the rate at which messages are processed, providing valuable insights into system performance. 

## Prerequisites
This project is built using C++17, and the following tools are required to successfully build it:
   ```
   clang++-11
   clang-format
   hiredis
   gtest
   cmake
   ```

On Ubuntu or other Debian-based systems they can be installed using the following command:
   ```
   sudo apt-get install clang-11 clang-format
   ```

More development tools (such as the __clang compiler, clang-format, clang-tidy__, etc.) can be installed with:
   ```
   sudo apt install clang-tools-11
   ```

The installation can be verified with the command:
   ```
   clang++-11 --version
   ```

The Hiredis library is required for some of the interactions with the Redis server. It can be installed with the following command:
   ```
   sudo apt-get install libhiredis-dev
   ```
The library can also be built locally. The steps to do so are explained in the next section - [Building and running the project](#Building-and-running-the-project)

The tests rely on the GTest library. It can be installed with the following command:
   ```
   sudo apt-get install libgtest-dev
   ```

CMake is required to build the tests and it can be installed with the following command:
   ```
   sudo apt-get install cmake
   ```

## Building and running the project
In order to build the project, execute the **format_and_build.sh** script, located in the src folder.
The script formats the code and builds the binary using the clang compiler and standard 17 of the C++ programming language.

The binary links against the "hiredis" library. The binary can be installed as part of the [prerequisites](#prerequisites), described in the section above or built and installed locally using the following commands:
```
$> git clone https://github.com/redis/hiredis.git
$> cd hiredis
$>    make
$>    sudo make install
```

Building the project:
```
$> cd src
$> ./format_and_build.sh [debug / release]
```

Execute the binary:
```
$> ./simple_redis_client
```

Execute the binary and provide a configuration file:
```
$> ./simple_redis_client <path_to_configuration_file/config.cfg>
```

Display help about the program:
```
$> ./simple_redis_client [-h / --help]
```

Alternatively the program and its tests can be built with CMake:
```
$> rm -rf build
$> mkdir build
$> cd build
$> cmake -DCMAKE_BUILD_TYPE=[Debug / Release] ..
$> make
```

With CMake, the code can be formatted with **clang-format** using the command:
```
$> make format
```

The project's tests can be executed with the command:
```
$> make run_tests
```