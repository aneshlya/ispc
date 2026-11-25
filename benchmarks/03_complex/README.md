# Complex cases inspired by real algorithms

- ``01_gather_aos`` - moments of point records of 2, 3 and 4 floats, read packed, strided and indexed, with gathers, transposes (``aos_to_soa*``) and wide contiguous loads.
- ``02_inline_vector_gather`` - edge relaxation over 16-byte vertex records read and written through large ``inline`` helpers with an all-lanes-on fast path, with int32 indices and a caller-provided base. Sensitive to the order of inlining and gather/scatter lowering.
- ``03_rgba8_diff`` - difference of two RGBA8 images ([discussion #2943](https://github.com/ispc/ispc/discussions/2943)), with a reference on ``uint32`` pixels.
- ``04_bilinear_remap`` - bilinear remap in HW and HWC layouts ([discussion #2919](https://github.com/ispc/ispc/discussions/2919)), with a reference that reads the grid with vector loads and a deinterleave.
- ``05_struct_copy`` - copies of 8-, 12- and 16-byte structs of byte fields ([#2778](https://github.com/ispc/ispc/issues/2778)), contiguous, indexed and masked, with references through integer types of the same size.
- ``06_bc1_decode`` - decode of a BC1 (DXT1) compressed image to RGBA8, with lanes over blocks (stride-4 pixel stores), a reference that writes them with ``soa_to_aos4``, and lanes over the 16 pixels of a block.
- ``07_mlp_inference`` - a small per-pixel MLP (12-32-32-12, SiLU) as in neural texture decoders, with lanes over the outputs of a layer or over pixels, and a ``float16`` version.
- ``08_lanczos_resample`` - resampling to about a half of RGB8 and RGB float images with a 2D Lanczos-3 filter of a fixed 6x6 window (not widened by the scale): 36 taps of gathered interleaved channels and ``sin()`` weights per pixel.
