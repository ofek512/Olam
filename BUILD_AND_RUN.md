cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
.\build\Release\Olam.exe --seed 1234
