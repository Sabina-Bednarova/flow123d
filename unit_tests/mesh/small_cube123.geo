cl1 = 2;
Point(1) = {1, 1, 1, cl1};
Point(2) = {-1, 1, 1, cl1};
Point(3) = {-1, -1, 1, cl1};
Point(4) = {1, -1, 1, cl1};
Point(5) = {1, -1, -1, cl1};
Point(6) = {-1, -1, -1, cl1};
Point(7) = {1, 1, -1, cl1};
Point(8) = {-1, 1, -1, cl1};
Line(1) = {2, 1};
Line(2) = {1, 7};
Line(3) = {8, 2};
Line(4) = {8, 7};
Line(7) = {3, 1};
Line(8) = {2, 3};
Line(12) = {3, 4};
Line(13) = {1, 4};
Line(16) = {8, 6};
Line(17) = {7, 6};
Line(18) = {6, 5};
Line(19) = {7, 5};
Line(20) = {7, 3};
Line(21) = {3, 6};
Line(22) = {4, 5};
Line Loop(6) = {2, -4, 3, 1};
Plane Surface(6) = {6};
Line Loop(11) = {7, -1, 8};
Plane Surface(11) = {11};
Line Loop(15) = {13, -12, 7};
Plane Surface(15) = {15};
Line Loop(24) = {22, -19, -2, 13};
Plane Surface(24) = {24};
Line Loop(26) = {12, 22, -18, -21};
Plane Surface(26) = {26};
Line Loop(28) = {21, -16, 3, 8};
Plane Surface(28) = {28};
Line Loop(30) = {17, -16, 4};
Plane Surface(30) = {30};
Line Loop(32) = {17, 18, -19};
Plane Surface(32) = {32};
Line Loop(34) = {17, -21, -20};
Plane Surface(34) = {34};
Line Loop(36) = {20, 7, 2};
Plane Surface(36) = {36};
Surface Loop(40) = {36, 34, 30, 28, 6, 11};
Volume(40) = {40};
Surface Loop(42) = {32, 26, 15, 24, 34, 36};
Volume(42) = {42};

// 1d_diagonal=43
Physical Line("1d_diagonal") = {20};
// 2d_surfaces=44
Physical Surface("2d_surfaces") = {34, 36};
// main_volume=45
Physical Volume("main_volume") = {40, 42};
