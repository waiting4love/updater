REM Current VCPKG cannot sucessfully build libgit2 static library on Windows (error on build pcre ), so I have to build it manually
REM Download libgit2 sourcecode, makedir release, run below commands
set BUILD_TYPE=Release
cmake -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTS=OFF -DBUILD_CLI=OFF -DCMAKE_INSTALL_PREFIX=/libs/install/libgit2/%BUILD_TYPE% ..
cmake --build . --config %BUILD_TYPE% --target install
