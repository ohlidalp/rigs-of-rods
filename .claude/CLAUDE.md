# Rigs of Rods — working notes

Gotchas learned the hard way on the OGRE 14.5 upgrade branch. Most cost a debugging
session each, and nearly all of them make a *code* bug look like the cause when it isn't.

## Rule zero: read `RoR.log` before theorising

The log lives in the *user profile* directory, which may be in 2 locations:
- User-created `<exedir>/config/` directory, log goes to `<exedir>/bin/config/logs/RoR.log`. 
- Default `Documents\My Games\Rigs of Rods\logs\RoR.log`.

When a material "has no supportable Techniques", the useful message is logged against the
*program* name, and for a `unified` program against its **delegates** — grep for those, not
for the name in the warning.

## Resources are deployed, and the deployment lies to you

Runtime resources live in `<build>/bin/resources`, not the source tree. Two mechanisms, each
with a trap:

- `fast_copy()` (`cmake/Macros.cmake`) copies file-by-file. It gained an opt-in `PRUNE` flag,
  enabled for `managed_materials` — without it, **files deleted from the repo linger in the
  build output**. OGRE material names are global, so an orphaned script can win a name over
  the current definition and produce failures nowhere near the cause.
- `recursive_zip_folder()` globs at **configure time with no `CONFIGURE_DEPENDS`**. Adding a
  *new* resource file therefore requires a CMake re-configure before it appears in the `.zip`.
  Editing an existing file is fine. This bites every time a shader is added.

`ContentManager::AddResourcePack()` prefers `<pack>.zip` over the `<pack>/` directory.
`managed_materials` is the exception — it is added with `addResourceLocation(..., "FileSystem")`
per content bundle, so there the *directory* is authoritative and `managed_materials.zip` is
unused.

To find orphans:

```sh
cd <build>/bin/resources && find . -type f | while read -r f; do
  [ -e "<repo>/resources/${f#./}" ] || echo "ORPHAN: ${f#./}"
done
```

Ignore top-level `*.zip` — those are legitimate build output.

## Shaders

**Cross-platform source, single-language declaration.** `OgreUnifiedShader.h` makes a shader
*source* work everywhere (it branches on `OGRE_HLSL` / `OGRE_GLSL` and supplies
`MAIN_PARAMETERS`, `IN`/`OUT`, `SAMPLER2D`, `mul`, `saturate`, `vec3_splat`, `texture1D`…).
A program *declaration* still names one language. Two ways to cover several:

```
vertex_program Foo glsl hlsl     # OGRE picks the first SUPPORTED language - preferred
```

`profiles` is Cg-only; HLSL needs `target`, which takes exactly **one** profile. Supply both.
The HLSL entry point is `main`, because that is what `MAIN_DECLARATION` expands to.


**Porting from Cg:** Cg tolerates things GLSL rejects. Expect to fix mixed `float3`/`float4`
operands (add an explicit `.xyz`), writes to uniforms, and writes to input varyings. Decide
deliberately whether to preserve odd original behaviour — e.g. `normalize()` on a `float4`
normal whose `w` is 1 is probably an original bug, but "fixing" it changes the visuals.

## RTSS (RTShaderSystem)

- **`Material::compile()` must run before RTSS registration.** `findSourceTechnique()` iterates
  `getSupportedTechniques()`, which only `compile()` populates. Register too early and RTSS
  silently finds nothing.
- `ShaderGenerator::getRenderState(scheme, mat, pass)` returns **NULL** for a material that has
  no shader-based technique yet; call `createShaderBasedTechnique()` first (it is idempotent).
  It can also legitimately return NULL, so guard it.
- `ShaderGenerator::_markNonFFP(tu)` excludes a texture unit from `FFPTexturing`, letting a
  custom sub render state claim units the material script still declares by name.
- **Light count is uncapped.** `synchronizeWithLightSettings()` sizes the per-light uniform
  arrays from lights-in-frustum and only ever grows them; one vehicle's flares took it to 79,
  overrunning the ps_3_0 constant register budget. `RTSSManager` pins it to
  `OGRE_MAX_SIMULTANEOUS_LIGHTS`, which is all OGRE ever binds anyway.
- **Every viewport needs `setMaterialScheme(MSN_SHADERGEN)`.** `Viewport` defaults to whatever
  `RenderSystem::_getDefaultViewportMaterialScheme()` returns, which is `MSN_SHADERGEN` only for
  render systems *without* `RSC_FIXED_FUNCTION`. D3D9 has it, so its viewports fall back to
  `MSN_DEFAULT` and render targets come out empty. GL3+/D3D11 are unaffected, so this hides.
- **Generated shaders are live-editable.** RTSS writes them to the shader cache
  (`<user dir>/cache`) and *reads them back if present*, so you can probe by assigning to
  `oColor_0.xyz` and restarting — no rebuild. The filename is a hash of the generated source,
  so re-locate it after any change (`grep -l <SRSname> *_FS.hlsl`).

## Vertex colour on flexbodies is not a colour

`FlexBody::updateBlend()` writes `VET_COLOUR_ARGB` **simulation flags**: alpha = node had
contact (damaged), blue = node is wet. They start at `0x00000000` — black, zero alpha.

So a material with `diffuse vertexcolour` hands black to the lighting stage and darkens the
whole vehicle. Read these deliberately in a shader; do not feed them to vertex colour tracking.

Only flexbodies get a `VES_DIFFUSE` buffer. Plain meshes (`managed/mesh_*`) have none, so a
shader reading a `COLOR` input there gets whatever the render system supplies for a missing
attribute — the legacy Cg shader had separate no-vertex-colour entry points for exactly this.

## Line endings

`core.autocrlf=true` with no `.gitattributes`, so blobs are normally LF — **but some tracked
files are stored CRLF** (`cmake/Macros.cmake`, `source/main/**/*.cpp`). Scripted edits must
detect and preserve each file's existing endings, or a two-line change shows up as a
whole-file rewrite.

## Build tips

Compile a single translation unit with the project's real flags, pulled from the build tree:

```
<build>/source/main/RoR.dir/Debug/RoR.tlog/CL.command.1.tlog   (UTF-16LE)
```

Strip `/Yu` and `/Fp` when a header changed, otherwise the stale precompiled header reports
phantom errors such as an enum value it cannot see. Use the toolchain matching the CMake
generator — VS 2022 here; VS 18 fails with `C1853` on the PCH.

`createManual()` has two overloads differing by a single `depth` argument. Inserting a
parameter silently selects the other one and yields a zero-sized texture at runtime.
