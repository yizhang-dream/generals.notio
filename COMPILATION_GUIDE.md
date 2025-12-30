# Static Compilation Guide

This document provides instructions for compiling the project statically so that the resulting executable (`game.exe`) can run on other Windows machines without needing separate DLLs (like `libgcc_s_dw2-1.dll` or `libstdc++-6.dll`).

## Requirements
- **Compiler**: MinGW-w64 (g++) or compatible GCC compiler.
- **Libraries**:
  - `easyx`: Graphics library (headers in `include/`, static lib `libeasyx.a` in `lib/`).
  - `ws2_32`: Windows Sockets for networking.
  - `winmm`: Windows Multimedia for sound.
  - `gdi32`, `user32`: Standard Windows GDI/User libraries (required by EasyX).

## Compilation Command

Run the following command in the terminal within the project root directory (`c:\project_v5\project_v4\project_v4`):

```bash
g++ main.cpp ai.cpp ai0.1.cpp ai0.2.cpp ai0.3.cpp ai0.4.cpp ai0.5.cpp ai0.6.cpp -o game.exe -I include -L lib -leasyx -lws2_32 -lwinmm -lgdi32 -luser32 -static-libgcc -static-libstdc++
```

### Flag Explanation

*   **Source Files**: `main.cpp ai.cpp ai0.1.cpp ... ai0.6.cpp` - Compiles the main game logic and all AI implementations.
*   **`-o game.exe`**: Specifies the output filename. **ALWAYS compile to `game.exe`.**
*   **`-I include`**: Adds the local `include` directory to the header search path.
*   **`-L lib`**: Adds the local `lib` directory to the library search path.
*   **`-leasyx`**: Links against the static EasyX library (`libeasyx.a`).
*   **`-lws2_32`**: Links against Windows Sockets 2 (system library).
*   **`-lwinmm`**: Links against Windows Multimedia (system library).
*   **`-lgdi32` const `-luser32`**: Required system dependencies for EasyX.
*   **`-static-libgcc`**: Link `libgcc` statically (removes dependency on `libgcc_*.dll`).
*   **`-static-libstdc++`**: Link `libstdc++` statically (removes dependency on `libstdc++-6.dll`).

> [!NOTE]
> If you add new source files (e.g., `ai0.7.cpp`), remember to include them in the `g++` command list.
> 
> [!IMPORTANT]
> Always compile the final executable as `game.exe`. Statically linked libraries are required for portability.
