; models for gfx3d written as source. apple (or fasm on the build machine) turns one
; straight into a mesh file, nothing to run:
;
;   apple /share/models/cube.asm cube.msh
;
; the mesh wants every vertex before the first triangle and both counts up front, so the
; body goes through three times: count, then vertices, then triangles
;
;   repeat 3
;   pass %
;       ...
;   end repeat
;   mesh_end
;
; whole pixels, spin3d sizes the mesh to fit. corners go counter clockwise around the
; outside (right hand rule), the back of a face lights dark unless the color has TWO_SIDED

SPT_MESH = 24
SPT_END = 0

BLACK = 0
BLUE = 1
GREEN = 2
CYAN = 3
RED = 4
PURPLE = 5
BROWN = 6
LIGHT_GRAY = 7
HALF_RANGE = 0x1000
TWO_SIDED = 0x2000

macro pass n
{
    if n = 2
        db SPT_MESH
        dd MODEL_VERTICES, MODEL_TRIANGLES
    end if
    MODEL_PASS = n
    MODEL_VERTICES = 0
    MODEL_TRIANGLES = 0
}

macro vertex x, y, z
{
    if MODEL_PASS = 2
        dd x, y, z
    end if
    MODEL_VERTICES = MODEL_VERTICES + 1
}

macro triangle color, a, b, c
{
    if MODEL_PASS = 3
        dd color, a, b, c
    end if
    MODEL_TRIANGLES = MODEL_TRIANGLES + 1
}

macro quad color, a, b, c, d
{
    triangle color, a, b, c
    triangle color, a, c, d
}

; bhaskara's sine from the 7th century, 1000 is 1.0, off by at most 2. whole degrees and
; integer math in an assembler, so no taylor series here
macro model_sine result, degrees
{
    MODEL_ANGLE = (degrees) mod 360
    MODEL_SIGN = 1
    if MODEL_ANGLE >= 180
        MODEL_ANGLE = MODEL_ANGLE - 180
        MODEL_SIGN = -1
    end if
    MODEL_ARCH = MODEL_ANGLE * (180 - MODEL_ANGLE)
    result = MODEL_SIGN * 4000 * MODEL_ARCH / (40500 - MODEL_ARCH)
}

; a profile turned around the y axis like a pot on a wheel. lathe starts it, then one ring
; per point of the profile from bottom to top. radius 0 is a single point on the axis
macro lathe sides, color
{
    LATHE_SIDES = sides
    LATHE_COLOR = color
    LATHE_RINGS = 0
}

macro ring r, y
{
    RING_FIRST = MODEL_VERTICES
    if r = 0
        vertex 0, y, 0
        RING_POLE = 1
    else
        RING_POLE = 0
        repeat LATHE_SIDES
            model_sine RING_SIN, 360 * (% - 1) / LATHE_SIDES
            model_sine RING_COS, 360 * (% - 1) / LATHE_SIDES + 90
            vertex r * RING_COS / 1000, y, r * RING_SIN / 1000
        end repeat
    end if
    if LATHE_RINGS > 0
        lathe_band
    end if
    LATHE_RINGS = LATHE_RINGS + 1
    LAST_FIRST = RING_FIRST
    LAST_POLE = RING_POLE
}

; the last ring to this one, two triangles a side, one where either end is a pole
macro lathe_band
{
    repeat LATHE_SIDES
        BAND_I = % - 1
        BAND_J = % mod LATHE_SIDES
        if LAST_POLE = 1
            triangle LATHE_COLOR, LAST_FIRST, RING_FIRST + BAND_I, RING_FIRST + BAND_J
        else if RING_POLE = 1
            triangle LATHE_COLOR, LAST_FIRST + BAND_I, RING_FIRST, LAST_FIRST + BAND_J
        else
            triangle LATHE_COLOR, LAST_FIRST + BAND_I, RING_FIRST + BAND_J, LAST_FIRST + BAND_J
            triangle LATHE_COLOR, LAST_FIRST + BAND_I, RING_FIRST + BAND_I, RING_FIRST + BAND_J
        end if
    end repeat
}

macro mesh_end
{
    db SPT_END
}
