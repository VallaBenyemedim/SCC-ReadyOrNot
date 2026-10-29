# Vendored libraries

The overlay builds the following upstream sources directly; no runtime download is required.

| Library | Version | Commit | License |
| --- | --- | --- | --- |
| [Dear ImGui](https://github.com/ocornut/imgui/tree/v1.93.0) | v1.93.0 | `c1aaac65e896a45f81f47a4d68f9959e0f86c859` | MIT, `third_party/imgui/LICENSE.txt` |
| [MinHook](https://github.com/TsudaKageyu/minhook/tree/v1.3.4) | v1.3.4 | `c3fcafdc10146beb5919319d0683e44e3c30d537` | BSD 2-Clause and bundled component notices, `third_party/minhook/LICENSE.txt` |

Dear ImGui core, Win32, DX11 and DX12 backends are used. MinHook is used for the
graphics callbacks. Library sources are unchanged; integration is in
`RoN-Internal/GraphicsOverlay.cpp` and `RoN-Internal/Overlay.Build.props`.
Segoe UI fonts are loaded from the existing Windows installation and are not redistributed.
