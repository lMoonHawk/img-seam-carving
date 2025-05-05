# img-seam-carving

A hobby C project to play with seam carving (content-aware image resizing), following the classic Avidan & Shamir paper (https://www.cs.technion.ac.il/~gabr/resources/seamcarving.pdf) and inspired by Tsoding (https://twitch.tv/tsoding).

## Features

- **CLI options**:
  - `-i <path>` Input image (required)
  - `-o <path>` Output image (defaults to `inp_dir/img_name-seam-png`)  
  - `-r <count>` Number of seams to remove (required)
  - `-p` Launch painting GUI

- **Paiting GUI**:
  - Left click to paint a protected region (green)
  - Right click to pain a region to remove (red)
  - Mousewheel or +/- to increase/decrease brush size

## Building

- **Dependencies**:
  - `SDL2`
  - `SDL2-image`

- **Building**:
   ```bash
   git clone https://github.com/lmoonhawk/img-seam-carving.git
  cd img-seam-carving
  ./build.sh
  ```


## To-Do (might be never)

- Horizontal seam removal  
- Seam insertion (content-aware upscaling)  
- Precomputed index maps for real-time scaling  
- Gradient-domain seam blending  
- GUI
