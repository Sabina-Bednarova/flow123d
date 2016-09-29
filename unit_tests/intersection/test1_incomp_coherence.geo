/*
  Mesh for Test1.
  Coherence - removes duplicate nodes in mesh.
*/
mesh = 0.35;
Point(1) = {0, 0, 0, mesh};
Point(2) = {1, 0, 0, mesh};
Point(3) = {1, 1, 0, mesh};
Point(4) = {0, 1, 0, mesh};
Point(5) = {0, 1, 1, mesh};
Point(6) = {1, 1, 1, mesh};
Point(7) = {0, 0, 1, mesh};
Point(8) = {1, 0, 1, mesh};
Point(9) = {0.25, 0, 0.5, mesh};
Point(10) = {0.25, 1, 0.5, mesh};
Point(11) = {0, 0.25, 0.5, mesh};
Point(12) = {1, 0.25, 0.5, mesh};
Line(1) = {5, 4};
Line(2) = {7, 1};
Line(3) = {1, 4};
Line(4) = {5, 7};
Line(5) = {7, 8};
Line(6) = {8, 6};
Line(7) = {6, 5};
Line(8) = {6, 3};
Line(9) = {3, 4};
Line(10) = {1, 2};
Line(11) = {2, 8};
Line(12) = {2, 3};
Line(13) = {5, 8};
Line(14) = {4, 2};
Line(15) = {6, 7};
Line(16) = {3, 1};
Line(17) = {9, 10};
Line(18) = {11, 12};
Line Loop(18) = {13, -11, -14, -1};
Plane Surface(19) = {18};
Line Loop(20) = {15, 2, -16, -8};
Plane Surface(21) = {20};
Line Loop(22) = {1, -3, -2, -4};
Plane Surface(23) = {22};
Line Loop(24) = {5, 6, 7, 4};
Plane Surface(25) = {24};
Line Loop(26) = {10, 11, -5, 2};
Plane Surface(27) = {26};
Line Loop(28) = {6, 8, -12, 11};
Plane Surface(29) = {28};
Line Loop(30) = {9, -1, -7, 8};
Plane Surface(31) = {30};
Line Loop(32) = {9, -3, 10, 12};
Plane Surface(33) = {32};
Surface Loop(34) = {23, 33, 31, 25, 27, 29};
Volume(35) = {34};
Physical Surface("2Dfracture1") = {21};
Physical Surface("2Dfracture2") = {19};
Physical Volume("volume") = {35};
Physical Line("channelY") = {17};
Physical Line("channelX") = {18};


Mesh 1;
Mesh 2;
Mesh 3;

Geometry.Tolerance = 1e-9;
Coherence Mesh;

Save "test1_incomp_coherence.msh";
Exit;
//*/