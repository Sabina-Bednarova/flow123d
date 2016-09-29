#include <flow_gtest.hh>
#include "arma_expect.hh"

#include "system/global_defs.h"

#include "intersection/simplex.hh"
#include "intersection/plucker.hh"

using namespace std;
using namespace computeintersection;

TEST(simplex, all) {
    
	arma::vec3 Point0;Point0[0] = 1;Point0[1] = 2;Point0[2] = 3;
	arma::vec3 Point1;Point1[0] = 2;Point1[1] = 3;Point1[2] = 4;
	arma::vec3 Point2;Point2[0] = 3;Point2[1] = 4;Point2[2] = 1;
	arma::vec3 Point3;Point3[0] = 4;Point3[1] = 1;Point3[2] = 2;

	arma::vec3 PointA;PointA[0] = 5;PointA[1] = 6;PointA[2] = 7;
	arma::vec3 PointB;PointB[0] = 6;PointB[1] = 7;PointB[2] = 5;
	arma::vec3 PointC;PointC[0] = 7;PointC[1] = 5;PointC[2] = 6;


	arma::vec3 *pole_pp[] = {&Point0,&Point1,&Point2,&Point3};
	arma::vec3 *pole_p[] = {&PointA, &PointB, &PointC};

    Simplex<0> s0d(pole_p);
    Simplex<2> s2d(pole_p);
    Simplex<3> s3d(pole_pp);
    
    MessageOut() << s0d << "\n\n" << s2d << "\n\n" << s3d << endl;
    
    // check point
    EXPECT_ARMA_EQ(PointA, s0d.point_coordinates());

    // set and check point
    s0d.set_simplices(pole_pp);
    EXPECT_ARMA_EQ(Point0, s0d.point_coordinates());
    
    EXPECT_EQ(Point0[0],s3d.abscissa(0)[0].point_coordinates()[0]);
    EXPECT_EQ(Point0[0],s3d.abscissa(1)[0].point_coordinates()[0]);
    
    
    arma::vec3 a,b; 
    a[0]=-1; a[1]=2; a[2]=3; 
    b[0]=4; b[1]=-5; b[2]=6;
    
    Plucker pc1, pc2(a,b), pc3(pc2);
    MessageOut() << pc1 << "\n" << pc2 << "\n" << pc3 << endl;
    
    EXPECT_FALSE(pc1.is_computed());
    EXPECT_TRUE(pc2.is_computed());
    EXPECT_TRUE(pc3.is_computed());
    
    // test coordinates getters
    EXPECT_EQ(pc2[4], pc2.get_plucker_coords()[4]);
    EXPECT_EQ(-27, pc2[3]);
    EXPECT_EQ(-18, pc2[4]);
    EXPECT_EQ(3, pc2[5]);
    
    EXPECT_ARMA_EQ(arma::vec({5,-7,3}), pc2.get_u_vector());
    EXPECT_ARMA_EQ(arma::vec({-27,-18,3}), pc2.get_ua_vector());
    
    // clear coords
    pc3.clear();
    EXPECT_FALSE(pc3.is_computed());
    
    pc1.compute(a,b);
    EXPECT_TRUE(pc2.is_computed());
    MessageOut() << pc1 << endl;
    
    EXPECT_EQ(pc2[1], pc1[1]);
    EXPECT_EQ(pc2[4], pc1[4]);

    // test Plucker scalar product
    EXPECT_EQ(0,pc1*pc2);
}