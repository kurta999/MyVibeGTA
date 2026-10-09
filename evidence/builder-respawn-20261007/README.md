# Builder death-screen R fix — 2026-10-07

Builder shortcuts consumed R before the death-screen restart handler. The DX11 input handler now prioritizes a fresh R press while dead, including with the builder inventory open. It uses the existing new-game restart behavior, which returns to the normal world and clears current game progress. Living players retain the builder inventory's R repair action. Pause, mode transitions, and other menus retain their input gates.

The regression sends real WM_KEYDOWN/WM_KEYUP messages through input::windowProc and checks closed/open inventory, living-player repair, held-key repeat rejection, pause, restored health/spawn position, and cleared movement/mouse inputs.

MiniCity3D and simulation_smoke built successfully. Three focused CTest suites passed: builder_transition_scenarios (including the input regression), tool_work_scenarios, and builder_interaction_scenarios. See build.log and tests.log. The root save remained unchanged, and the runtime save was restored after testing. OpenGL rendering files and target were not changed or built.
