# Height integration still requiring evidence

The earlier wildlife/pickup source observations are addressed by this milestone's controlled Jolt, game-loop and native-view verification. This is not completion of all height consumers.

- `src/game.cpp`, `interactionOptions` and `carryDrop`: pedestrian corpse selection still checks XZ distance and `clearLine`, while dropping uses `solid`. Reproduce a surface corpse over an underground player, then use the corpse's actual pose/support height and incoming geometry. Keep ragdoll/snapshot, carried pose and safe drop behavior coherent.
- `src/ai.cpp`, armed Attack strafing: tactical destinations still pass through `solid(destination,12)`, although movement follows supported floors. Test actual armed combat beneath retained roofs and around mined/placed cover before claiming complete combat integration.
- Vehicle road pursuit, distant actor behavior, player/actor queries in other game systems and navigation after arbitrary world edits remain broader integration requirements. The existing controlled pedestrian restart fixtures cover their particular saved pit/tunnel routes, rather than all actor state persistence.
- Wildlife now uses actual height and species-sized static movement sweeps, but complex route planning, dense crowds and arbitrary dynamic support need broader tests. Its existing approximate projectile/reticle silhouettes are still narrower than some modeled head/tail extents; full oriented body targeting deserves a separate reproduction and review.
- Exceptional actor escape failures still need transactional layer reconstruction handling. Interrupted-process save/version import recovery, face/collider merging, memory use with dense scenery edits and continuous exploratory acquisition/balance remain unfinished.

These are source-derived next checks, not assertions that new runtime failure reproductions have already passed. Preserve the root save, run save-writing tests and GUI fixtures sequentially in the separate runtime, and match completion claims to actual verification scope.
