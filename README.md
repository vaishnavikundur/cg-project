Flappy Fish — asset notes

If the fish or background images do not appear when running the game, check these:

- The game loads images from the working directory at runtime. When run from the repository root, assets are expected under `src/` (e.g. `src/fish.png` and `src/bg.png`).
- The code also tries `assets/fish.png` and `fish.png` as fallback paths for the fish image.
- If a texture file exists but fails to decode, replace it with a valid PNG/JPEG or use the included fallback images.

Diagnostics:
- Run the executable from the project root so the relative path `src/fish.png` resolves properly.
- You can inspect logs for messages about file loading and image decoding; the game writes INFO/WARNING logs at startup.

How this repo fixes the problem:
- The game now attempts multiple paths when loading the fish texture, has a safe scale calculation, log messages, and on-screen warnings to make it clear when assets are missing or invalid.

If you'd like, I can: (1) add sample art into `src/` that you can replace; (2) make the game search other folders; or (3) bundle assets into the executable installer.