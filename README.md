This is the migration tool to convert only the state file of QEarn SC.

How to work this tool

Please make the build file using cmake(you need to install the cmake in your PC. please download in here(https://cmake.org/download/)).
   - you need to make the `build` directory inside `qubic-stateFile-migration-QEarn-SC--tool` directory at first.
   - please open the `build` directory.
   - please open the cmd and write `cmake ../` command in cli. then it will be created the build files.
   - please open the file `MigrationTool.sln` using Mocrosoft Visual Studio 2022.
   - please complie with release mode. then `qubic-stateFile-migration-QEarn-SC--tool.exe` file would be created in release directory.
   - please move the contract0009.144 file to release directory. and run the `qubic-stateFile-migration-QEarn-SC--tool.exe` file.
   - you need to check the changes(date, size) of contract0009.144 files.
