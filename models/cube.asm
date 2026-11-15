format binary as 'msh'
include 'model.inc'

repeat 3
pass %

vertex -50, -50, -50
vertex 50, -50, -50
vertex 50, 50, -50
vertex -50, 50, -50
vertex -50, -50, 50
vertex 50, -50, 50
vertex 50, 50, 50
vertex -50, 50, 50

quad PURPLE, 0, 3, 2, 1
quad GREEN, 4, 5, 6, 7
quad CYAN, 0, 4, 7, 3
quad RED, 1, 2, 6, 5
quad BLUE, 0, 1, 5, 4
quad BROWN, 3, 7, 6, 2

end repeat
mesh_end
