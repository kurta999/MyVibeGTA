# Added runtime dependencies

- Dear ImGui 1.92.6: https://github.com/ocornut/imgui/releases/tag/v1.92.6
  - Source archive: https://codeload.github.com/ocornut/imgui/tar.gz/refs/tags/v1.92.6
  - Archive SHA-256: `5b17c01f69545bde732b14936d89ce0f508adb83e8b56fa82448371845172bc3`
  - MIT license: `imgui-1.92.6/LICENSE.txt`.
  - Official Win32 and Direct3D 11 backends; statically linked into the DX11 game.
- Lua 5.4.8: https://www.lua.org/ftp/lua-5.4.8.tar.gz
  - Archive SHA-256: `4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae`
  - MIT license: `LUA-LICENSE.txt`, reproduced from upstream `doc/readme.html`.
  - Interpreter library sources are statically linked; command-line Lua/luac sources are excluded.

These sources are vendored so builds do not download Lua or ImGui. Packages retain both licenses. Existing Jolt and optional AMD FSR2 dependencies keep their original pins and licenses.
