Breve ayuda para el linter C/C++

Si el linter/IntelliSense no encuentra headers como `gba/m4a_internal.h`:

1. Recarga la ventana de VS Code: "Developer: Reload Window" (Ctrl+Shift+P).
2. Asegúrate de que la extensión C/C++ de Microsoft esté instalada.
3. El archivo `c_cpp_properties.json` ya incluye las rutas típicas del proyecto (`include`, `include/gba`, `agbcc/ginclude`, `src`).

Si usas `clangd` u otro LSP, considera generar `compile_commands.json` con tu sistema de build (por ejemplo, usando Bear) y colocarlo en la raíz del workspace. `clangd` usará ese archivo y la indexación será exacta.

Ejemplo rápido con Bear (si está disponible):

  # desde la raíz del repo
  bear -- make modern

Esto genera `compile_commands.json` y mejora la precisión del LSP.
