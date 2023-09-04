lg3dControl

This is an old project I did for a stage lighting company way back

This code renders a realtime 3D scene that utilizes many spotlights and other lighting effects to simulate what a concert or show stage will look like.  It supports various spot lights that cast light effects with shadows.  It looks impressive to have 64 lights all shining on a scene with texture map effects, shadows, soft lighting, etc, all rotating and moving in realtime.

It makes use of several custom vertext and pixel shaders (the .fx files in the data dir), and renders the scene in multiple passes to correctly handle transparency, texture maps, shadows, etc.

Likely this won't run anymore.   Last time it was used, it compiled up on pixel shader 2.0 (was coded to 1.1 but was changed with one line).  So who knows...

