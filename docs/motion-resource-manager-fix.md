# Linux Motion ResourceManager compatibility

The exported Senren*Banka scripts contain a compiled `system/motion.tjs`.
Its first operation constructs `new Motion.ResourceManager(motionCacheSize)`.
The Linux bootstrap previously exposed `Motion.ResourceManager` as a plain
dictionary so TJS raised `Not a function or invalid method/property type` at
that constructor call.

The Linux compatibility layer now exposes a constructible native class. Its
constructor and lifecycle methods (`addRef`, `release`, `finalize`, `load`,
`unload`, and `clearCache`) are safe no-ops, as are the PSB callback setters.
This lets the game select its existing non-D3D path without claiming to
implement the Android Motion/E-mote renderer. A real renderer/plugin is still
needed for animated Motion assets.

The startup regression constructs the class and calls each lifecycle method.
The GUI smoke runner also treats `glxinfo` as optional: it records a skipped
probe when `mesa-utils` is absent instead of failing before the engine starts.
