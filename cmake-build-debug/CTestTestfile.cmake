# CMake generated Testfile for 
# Source directory: /Users/arcueid/program/LevelDB-KV-Separation
# Build directory: /Users/arcueid/program/LevelDB-KV-Separation/cmake-build-debug
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(leveldb_tests "/Users/arcueid/program/LevelDB-KV-Separation/cmake-build-debug/leveldb_tests")
set_tests_properties(leveldb_tests PROPERTIES  _BACKTRACE_TRIPLES "/Users/arcueid/program/LevelDB-KV-Separation/CMakeLists.txt;365;add_test;/Users/arcueid/program/LevelDB-KV-Separation/CMakeLists.txt;0;")
add_test(c_test "/Users/arcueid/program/LevelDB-KV-Separation/cmake-build-debug/c_test")
set_tests_properties(c_test PROPERTIES  _BACKTRACE_TRIPLES "/Users/arcueid/program/LevelDB-KV-Separation/CMakeLists.txt;391;add_test;/Users/arcueid/program/LevelDB-KV-Separation/CMakeLists.txt;394;leveldb_test;/Users/arcueid/program/LevelDB-KV-Separation/CMakeLists.txt;0;")
add_test(env_posix_test "/Users/arcueid/program/LevelDB-KV-Separation/cmake-build-debug/env_posix_test")
set_tests_properties(env_posix_test PROPERTIES  _BACKTRACE_TRIPLES "/Users/arcueid/program/LevelDB-KV-Separation/CMakeLists.txt;391;add_test;/Users/arcueid/program/LevelDB-KV-Separation/CMakeLists.txt;402;leveldb_test;/Users/arcueid/program/LevelDB-KV-Separation/CMakeLists.txt;0;")
subdirs("third_party/googletest")
subdirs("third_party/benchmark")
