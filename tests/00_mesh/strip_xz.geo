cl1 = 0.02;
lx=3*cl1;
lz=1;

Point(1) = {0, 0, 0, cl1};
Point(2) = {lx, 0, 0, cl1};
Point(3) = {lx, 0, lz, cl1};
Point(4) = {0, 0, lz, cl1};
Line(20) = {1,2};
Line(21) = {2,3};
Line(22) = {3,4};
Line(23) = {4,1};
Line Loop(30) = {20, 21, 22, 23};
Plane Surface(30) = {30};
Physical Line(".left") = {23};
Physical Line(".right") = {21};
Physical Line(".top") = {22};
Physical Line(".bottom") = {20};
Physical Surface("plane") = {30};
