; about 190 pixels across the middle

format binary as 'msh'
include 'model.inc'

repeat 3
pass %

lathe 8, RED
ring 0, -78
ring 42, -86
ring 80, -56
ring 96, -8
ring 88, 42
ring 58, 76
ring 22, 70
ring 0, 56

; the stem, a square tube leaning out to the right
vertex 7, 50, 0
vertex 0, 50, 7
vertex -7, 50, 0
vertex 0, 50, -7
vertex 19, 102, 0
vertex 12, 102, 7
vertex 5, 102, 0
vertex 12, 102, -7
quad BROWN, 50, 54, 55, 51
quad BROWN, 51, 55, 56, 52
quad BROWN, 52, 56, 57, 53
quad BROWN, 53, 57, 54, 50
triangle BROWN, 54, 56, 55
triangle BROWN, 54, 57, 56

; one leaf, lit from both sides
vertex 10, 86, 0
vertex 42, 104, 14
vertex 72, 100, 4
vertex 42, 88, -6
triangle GREEN + TWO_SIDED, 58, 60, 59
triangle GREEN + TWO_SIDED, 58, 61, 60

end repeat
mesh_end
