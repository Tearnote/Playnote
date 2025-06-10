# Organization

The codebase is organized into directories as follows:

- [`cmake`](cmake): CMake includes.
- [`src/preamble`](src/preamble): A centralized module export of commonly used types and utilities, forming the project's standard vocabulary.
- [`src/macros`](src/macros): Headers for utilities that absolutely have to be preprocessor macros. Header files are not allowed anywhere else in the project.
- [`src/lib`](src/lib): Module wrappers of external libraries, simplifying their API and standardizing their naming and error handling. Includes of external headers are only allowed in here, `src/preamble` and `src/macros`. 
- [`src/dev`](src/dev): Handlers of devices and OS concepts.
- [`src/io`](src/io): File and virtual I/O handling.
- [`src/gfx`](src/gfx): Graphics rendering subsystem.
- [`src/gpu`](src/gpu): GPU-side code (shaders).
- [`src/bms`](src/bms): Parsing and processing of the BMS format.
- [`src/threads`](src/threads): Each file is one primary thread of execution.
