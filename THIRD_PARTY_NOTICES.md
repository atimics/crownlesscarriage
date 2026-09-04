# Third-party notices

This file records the third-party software and supplied media found in
Crownless Carriage. It was reviewed on 3 September 2026 against the build
files and raylib 6.0 at commit
`dbc56a87da87d973a9c5baa4e7438a9d20121d28`.

## Shipped software

| Component | Use | Terms |
| --- | --- | --- |
| raylib 6.0 | Graphics, input, models, and audio | zlib/libpng license |
| GLFW 3.4.0 | Desktop windows and input through raylib | zlib/libpng license |
| SQLite | Saved campaigns and command journal | Public domain |
| Emscripten 5.0.4 runtime and system libraries | Browser build | MIT, University of Illinois/NCSA, and component terms |
| raylib support libraries | File loading, audio, compression, and geometry | MIT, MIT-0, zlib/libpng, CC0, public domain, or WTFPL v2 |

raylib builds the following support libraries into its default modules:

- `rprand` and `rltexgpu` under the zlib/libpng license;
- `sdefl`, `sinfl`, the stb libraries, `miniaudio`, `dr_wav`, and `dr_mp3`
  under a choice of permissive terms stated in their source headers;
- `qoi`, `qoa`, `qoaplay`, `tinyobj_loader_c`, `cgltf`, `m3d`,
  `par_shapes`, and `vox_loader` under the MIT license;
- `jar_xm` and `jar_mod` under WTFPL v2 or public-domain terms; and
- `glad` and Khronos definitions under the terms stated in their source
  headers.

The complete dependency source and its per-file notices are available in the
[pinned raylib source](https://github.com/raysan5/raylib/tree/dbc56a87da87d973a9c5baa4e7438a9d20121d28).
The raylib project also keeps a
[dependency license list](https://github.com/raysan5/raylib/wiki/raylib-dependencies).

## Supplied media

`assets/video/introduction/the-predator-clause.mp3` has embedded metadata that
credits **The Predator Clause** to **ratimics** and says it was made with
[Suno](https://suno.com/song/2542d5c2-8755-4000-8764-ba8ad055d591) on
27 August 2026. Suno grants different rights based on the creator's plan at
the time of creation. Keep proof of the applicable plan with release records
before commercial use. The current
[Suno rights guide](https://help.suno.com/en/articles/2416769) explains the
plan rules.

This track is part of the source repository and the introduction video. It is
outside the Crownless project license grant.

## Build tools

Blender, CMake, compilers, and other build tools keep their own licenses. A
tool's license applies to the tool. Project output keeps the terms that apply
to its source material and linked components.

## License texts

### raylib 6.0 — zlib/libpng license

Copyright (c) 2013-2026 Ramon Santamaria (@raysan5)

This software is provided "as-is", without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the
use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim
   that you wrote the original software. If you use this software in a
   product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

### GLFW 3.4.0 — zlib/libpng license

Copyright (c) 2002-2006 Marcus Geelnard

Copyright (c) 2006-2019 Camilla Löwy

This software is provided "as-is", without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the
use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim
   that you wrote the original software. If you use this software in a
   product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

### MIT license group

This license applies to the MIT-selected raylib support libraries listed
above. Their source headers carry these copyright notices:

- Copyright (c) 2020-2023 Micha Mettke (`sdefl`, `sinfl`)
- Copyright (c) 2017 Sean Barrett (stb libraries)
- Copyright (c) 2021 Dominic Szablewski (`qoi`)
- Copyright (c) 2023 Dominic Szablewski (`qoa`, `qoaplay`)
- Copyright (c) 2016-2019 Syoyo Fujita and many contributors
  (`tinyobj_loader_c`)
- Copyright (c) 2018-2021 Johannes Kuhlmann (`cgltf`)
- Copyright (c) 2020 bzt (`m3d`)
- Copyright (c) 2019 Philip Rideout (`par_shapes`)
- Copyright (c) 2021 Johann Nadalutti (`vox_loader`)
- Copyright (c) 2008-2018 The Khronos Group Inc. (Khronos definitions)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

### MIT-0 group

The MIT-0 choice applies to `miniaudio`, `dr_wav`, and `dr_mp3`.

Copyright 2025 David Reid (`miniaudio`)

Copyright 2023 David Reid (`dr_wav`, `dr_mp3`)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

### Emscripten — MIT license

Copyright (c) 2010-2014 Emscripten authors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

### Emscripten — University of Illinois/NCSA license

Copyright (c) 2010-2014 Emscripten authors. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimers.

Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimers in the documentation
and/or other materials provided with the distribution.

Neither the names of Mozilla, nor the names of its contributors may be used to
endorse or promote products derived from this Software without specific prior
written permission.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

The full Emscripten license also includes notices for bundled code. See the
[official Emscripten license](https://github.com/emscripten-core/emscripten/blob/main/LICENSE).

### SQLite — public domain

SQLite's authors have dedicated the deliverable code and documentation to the
public domain. The official
[SQLite copyright page](https://www.sqlite.org/copyright.html) records that
dedication.
