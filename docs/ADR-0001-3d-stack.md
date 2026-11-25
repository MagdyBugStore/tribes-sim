# ADR-0001: Stack for the 3D viewer

Status: accepted (provisional, see "What is not measured"). Date: 2026-11-25.

## Decision

Use **three.js** (vendored, r187, MIT) in plain ES modules. No bundler, no npm step at runtime.
Characters are drawn with **one InstancedMesh per body type, with the walk cycle done in the vertex shader**.
Skinned meshes are kept only for the one selected or followed person. The server stays as it is: the viewer
**polls** (as the 2D viewer does) and **interpolates positions in time on the client** from the daily schedule.

## Options compared

| | three.js | Babylon.js | PlayCanvas engine | Godot 4 web export |
|---|---|---|---|---|
| License | MIT | Apache-2.0 | MIT | MIT |
| GitHub stars (checked 2026-11-25) | 119,297 | 26,326 | 17,336 | not checked |
| Latest release seen | r187, 30 Oct 2026 | 9.37.2, 23 Nov 2026 | 2.24.2, 18 Nov 2026 | n/a |
| Fits "static files + C++ server" | yes, one module file | yes, but the core package is 99 MB unpacked (npm install of 9.37.2) | yes | needs a separate export and a build of the Godot project |
| Notes | smallest, lowest-level, most examples | full engine, heavier | engine with editor | C# projects cannot export to web in Godot 4, so GDScript only; threads need SharedArrayBuffer and cross-origin isolation headers |

Sources:
- https://github.com/mrdoob/three.js/ and https://www.npmjs.com/package/three
- https://github.com/BabylonJS/Babylon.js/
- https://github.com/playcanvas/engine/
- https://docs.godotengine.org/en/latest/tutorials/export/exporting_for_web.html
- https://docs.godotengine.org/en/latest/tutorials/scripting/c%5Fsharp/c_sharp_basics.html

Godot is out because our server is a separate C++ process serving static files, and the web export adds
isolation-header requirements and a second toolchain for no gain. Babylon.js and PlayCanvas are not out on
technical grounds. They were **not prototyped** (see below).

## Measurements (three.js only)

Prototype: `web3d/proto/three-bench.html`, run as
`?mode=skinned|instanced&n=N&secs=S`. Scene: 512x512 terrain grid (about 520k triangles) plus N characters.

Machine: **a 1 vCPU, 2 GB cloud sandbox, headless Chrome with SwiftShader software GL**. There is no GPU.

| mode | N | fps | JS time per frame | draw calls | triangles |
|---|---|---|---|---|---|
| InstancedMesh, GPU walk cycle | 1000 | 3.0 | 0.9 ms | 2 | 570k |
| SkinnedMesh per character (3 bones) | 1000 | 3.1 | 13.2 ms | 720 | 557k |
| SkinnedMesh per character | 200 | 3.2 | 4.9 ms | 142 | 529k |

How to read this: the fps figures are **not representative**. Software rasterization of 500k+ triangles is the
limit here and it is the same in every row, so fps does not separate the modes. The **JS time per frame and the
draw-call count do** carry over to real hardware: 1000 skinned characters cost about 13 ms of CPU per frame
(too much for 60 fps alongside everything else) and 720 draw calls, while instancing costs under 1 ms and 2 calls.
That is the basis for the decision. A real fps number on a laptop or phone GPU still has to be measured.

## Polling or SSE, and time interpolation

- The 2D viewer already works from `/api/schedules` (each person's stops for the day) and clamps to the server
  clock from `/api/state`. The 3D viewer should reuse exactly that: positions are a pure function of
  (schedule, simulated time), so no per-frame stream is needed.
- Keep **polling**: `/api/state` about once a second for the clock and speed, `/api/people` every few seconds for
  needs and flags, `/api/schedules` once per simulated day. SSE would add a long-lived connection to the C++ server
  for data that changes once per simulated hour or day. Revisit only if events (births, deaths, caravans) must
  show up within a second; the 2D ticker polls `/api/dash` and has been fine.
- Interpolate on the client between stops with the simulated clock, as the 2D viewer does. Smooth the clock
  correction (the 2D code moves 30% of the difference each poll) so speed changes do not jump people.

## What is not measured

- No real-GPU fps. All numbers are from software GL on one vCPU.
- Babylon.js and PlayCanvas were not prototyped. The issue asked for the top two; time went into three.js,
  and I chose it before measuring the others based on size and fit. If a stakeholder wants the comparison
  done properly, the benchmark page is the template.
- Only Chrome was tested. No Firefox or Safari.
- Skinned characters used a 3-bone placeholder, not a real character model with a full skeleton.

## Consequences

- three.js is vendored in `web3d/vendor/three/` (with its MIT license). Upgrading means replacing those files.
- "Hello 3D": `python3 -m http.server 8000 --directory web3d` then open http://localhost:8000/hello.html
  (about 20 lines, one capsule walking on a plane).
- Next issues (assets, terrain, people, buildings, scene from the map, UI) build on this stack.
