# Known minor cosmetic observation — foliage alpha fringe

Status: separate, non-blocking visual issue; not BUG-001.

The D4 zoom image shows a very thin pale/grey line on several broad leaves behind
the small conifer near the veranda. The line follows individual leaf edges. It
does not form the rectangular boundary of a texture quad and does not produce the
old depth/occlusion failure.

The most likely class is transparent-edge colour bleed under filtered sampling:
very small non-zero alpha samples survive the live `GL_GREATER, ref=0` state while
RGB from transparent border texels contributes a pale fringe. A similar faint edge
was already visible before D4.

Do not change the stable repair by guessing a higher alpha cutoff. The exact live
state on the validated scene was `GL_GREATER 0`; raising it could clip fine foliage
and would stop being exact legacy-state emulation.

If this cosmetic issue is investigated later, treat it as a separate bug/branch
focused on texture alpha bleed, premultiplication or atlas/sampler edge handling.
The PZF23D4 producer/compositor fix remains the stable baseline.

