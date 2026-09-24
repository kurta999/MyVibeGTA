# Repository guidance

- Continue the implementation roadmap in `idea.md` through runnable, testable milestones.
- Leave the OpenGL fallback as it is. Do not spend implementation or cleanup effort on `renderer.cpp`, `textures.cpp`, or the `MiniCity3DGL` target unless a later user request explicitly changes this direction.
- Develop and verify new rendering features in the Direct3D 11 `MiniCity3D` target.
- Keep roadmap status factual: Jolt physics and ragdolls, live skeletal animation, and GPU instancing/LOD are complete only when their respective implementations are integrated and verified.
