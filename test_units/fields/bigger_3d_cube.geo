cl2 = 0.5;
Point(1) = {2, 2, 2, cl2};
Point(2) = {-2, 2, 2, cl2};
Point(3) = {-2, -2, 2, cl2};
Point(4) = {2, -2, 2, cl2};
Point(5) = {2, -2, -2, cl2};
Point(6) = {-2, -2, -2, cl2};
Point(7) = {2, 2, -2, cl2};
Point(8) = {-2, 2, -2, cl2};
Line(1) = {2, 1};
Line(2) = {1, 7};
Line(3) = {7, 8};
Line(4) = {8, 2};
Line(5) = {2, 3};
Line(6) = {3, 4};
Line(7) = {4, 5};
Line(8) = {5, 6};
Line(9) = {6, 3};
Line(10) = {6, 8};
Line(11) = {7, 5};
Line(12) = {1, 4};
Line Loop(13) = {1, 12, -6, -5};
Plane Surface(14) = {13};
Line Loop(15) = {12, 7, -11, -2};
Plane Surface(16) = {15};
Line Loop(17) = {3, 4, 1, 2};
Plane Surface(18) = {17};
Line Loop(19) = {3, -10, -8, -11};
Plane Surface(20) = {19};
Line Loop(21) = {10, 4, 5, -9};
Plane Surface(22) = {21};
Line Loop(23) = {6, 7, 8, 9};
Plane Surface(24) = {23};
Surface Loop(25) = {14, 18, 20, 22, 24, 16};
Volume(26) = {25};


Physical Volume("main_volume") = {26};
