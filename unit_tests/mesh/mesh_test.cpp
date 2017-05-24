/*
 * mesh_test.cpp
 *
 *  Created on: Jan 26, 2013
 *      Author: jb
 */

#define TEST_USE_PETSC
#define FEAL_OVERRIDE_ASSERTS
#include <flow_gtest_mpi.hh>
#include <mesh_constructor.hh>

#include "mesh/mesh.h"
#include <iostream>
#include <vector>
#include "mesh/accessors.hh"
#include "input/reader_to_storage.hh"
#include "system/sys_profiler.hh"



using namespace std;

class MeshTest :  public testing::Test, public Mesh {
public:
    MeshTest()
    : Mesh(*mesh_constructor())
    {
    }

    ~MeshTest()
    {
    }
};


// data structures represent mesh stored in UNIT_TESTS_SRC_DIR+/mesh/simplest_cube.msh
PhysicalNamesDataTable physical_names_data = {
		{1, 37, "1D diagonal"},
		{2, 38, "2D XY diagonal"},
		{2, 101, ".top side"},
		{2, 102, ".bottom side"},
		{3, 39, "3D back"},
		{3, 40, "3D front"}
};
NodeDataTable node_data = {
		{1, {1, 1, 1} },
		{2, {-1, 1, 1} },
		{3, {-1, -1, 1} },
		{4, {1, -1, 1} },
		{5, {1, -1, -1} },
		{6, {-1, -1, -1} },
		{7, {1, 1, -1} },
		{8, {-1, 1, -1} }
};
ElementDataTable element_data = {
		{ 1, 1, 37, 0, 7, 3 },
		{ 2, 2, 38, 0, 6, 3, 7 },
		{ 3, 2, 38, 0, 3, 1, 7 },
		{ 4, 3, 39, 0, 3, 7, 1, 2 },
		{ 5, 3, 39, 0, 3, 7, 2, 8 },
		{ 6, 3, 39, 0, 3, 7, 8, 6 },
		{ 7, 3, 40, 0, 3, 7, 6, 5 },
		{ 8, 3, 40, 0, 3, 7, 5, 4 },
		{ 9, 3, 40, 0, 3, 7, 4, 1 },
		{10, 2, 101, 0, 1, 2, 3 },
		{11, 2, 101, 0, 1, 3, 4 },
		{12, 2, 102, 0, 6, 7, 8 },
		{13, 2, 102, 0, 7, 6, 5 }
};


TEST_F(MeshTest, intersect_nodes_lists) {
	node_elements.resize(3);
	node_elements[0]={ 0, 1, 2, 3, 4};
	node_elements[1]={ 0, 2, 3, 4};
	node_elements[2]={ 0, 1, 2, 4};

    vector<unsigned int> node_list={0,1,2};
    vector<unsigned int> result;
    intersect_element_lists(node_list, result);
    EXPECT_EQ( vector<unsigned int>( {0,2,4} ), result );

    node_list={0,1};
    intersect_element_lists(node_list, result);
    EXPECT_EQ( vector<unsigned int>( {0,2,3,4} ), result );

    node_list={0};
    intersect_element_lists(node_list, result);
    EXPECT_EQ( vector<unsigned int>( {0,1,2,3,4} ), result );

}



TEST(MeshTopology, make_neighbours_and_edges) {
    Profiler::initialize();
    
    Mesh * mesh = mesh_constructor();
    mesh->init_from_input(physical_names_data, node_data, element_data);

    EXPECT_EQ(9, mesh->n_elements());
    EXPECT_EQ(18, mesh->bc_elements.size());

    // check bc_elements
    EXPECT_EQ(101 , mesh->bc_elements[0].region().id() );
    EXPECT_EQ(101 , mesh->bc_elements[1].region().id() );
    EXPECT_EQ(102 , mesh->bc_elements[2].region().id() );
    EXPECT_EQ(102 , mesh->bc_elements[3].region().id() );
    EXPECT_EQ( -3 , int( mesh->bc_elements[4].region().id() ) );
    EXPECT_EQ( -3 , int( mesh->bc_elements[17].region().id() ) );

    //check edges
    EXPECT_EQ(28,mesh->n_edges());

    //check neighbours
    EXPECT_EQ(6, mesh->n_vb_neighbours() );

    delete mesh;

}


const string mesh_input = R"YAML(
mesh_file: "mesh/simplest_cube.msh"
regions:
 - !From_Elements
   id: 3000
   name: new region
   element_list:
    - 6
    - 7
 - !From_Id
   id: 37
   name: 1D rename
 - !Union
   name: 3D
   region_ids:
    - 39
    - 40
)YAML";

TEST(Mesh, init_from_input) {
    FilePath::set_io_dirs(".",UNIT_TESTS_SRC_DIR,"",".");

    Mesh * mesh = mesh_constructor(mesh_input, Input::FileFormat::format_YAML);
    mesh->init_from_input(physical_names_data, node_data, element_data);

    EXPECT_EQ( 37, mesh->element_accessor(0).region().id() );
    EXPECT_EQ( "1D rename", mesh->element_accessor(0).region().label() );

    EXPECT_EQ( 38, mesh->element_accessor(1).region().id() );
    EXPECT_EQ( 38, mesh->element_accessor(2).region().id() );
    EXPECT_EQ( 39, mesh->element_accessor(3).region().id() );
    EXPECT_EQ( 39, mesh->element_accessor(4).region().id() );
    EXPECT_EQ( 3000, mesh->element_accessor(5).region().id() );
    EXPECT_EQ( 3000, mesh->element_accessor(6).region().id() );
    EXPECT_EQ( 40, mesh->element_accessor(7).region().id() );
    EXPECT_EQ( 40, mesh->element_accessor(8).region().id() );

    RegionSet set = mesh->region_db().get_region_set("3D");
    EXPECT_EQ( 39, set[0].id() );
    EXPECT_EQ( 40, set[1].id() );

    delete mesh;
}


TEST(Mesh, decompose_problem) {
    Mesh * mesh = mesh_constructor();
    PhysicalNamesDataTable physical_names_dec;
    NodeDataTable node_dec = {
    		{1, {-1, 1, 1} },
    		{2, {-1, -1, 1} },
    		{3, {1, 1, -1} },
    		{4, {-1, -1, -1} }
    };
    ElementDataTable element_dec = {
    		{ 1, 3, 39, 0, 2, 3, 1, 4 }
    };
    EXPECT_THROW_WHAT( { mesh->init_from_input(physical_names_dec, node_dec, element_dec); }, Partitioning::ExcDecomposeMesh,
    		"greater then number of elements 1. Can not make partitioning of the mesh");
    delete mesh;
}
