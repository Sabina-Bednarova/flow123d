/*!
 *
﻿ * Copyright (C) 2015 Technical University of Liberec.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 3 as published by the
 * Free Software Foundation. (http://www.gnu.org/licenses/gpl-3.0.en.html)
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 *
 * @file    surface_depth.cc
 * @brief
 */

#include <vector>
#include <armadillo>
#include <limits>

#include "fields/surface_depth.hh"
#include "mesh/mesh.h"


SurfaceDepth::SurfaceDepth(const Mesh *mesh, std::string surface_region, std::string surface_direction)
{
    this->create_projection_matrix( arma::vec3(surface_direction) );
    this->construct_bih_tree( const_cast<Mesh*>(mesh), surface_region);
}


void SurfaceDepth::create_projection_matrix(arma::vec3 surface_vec)
{
    if (surface_vec(2)==0) {
        THROW( ExcSurfaceProjection() << EI_Message("Vertical plane of surface is forbidden!") );
    }
    surface_norm_vec_ = surface_vec / arma::norm(surface_vec, 2); // normalize to size == 1

    arma::vec3 ex("1 0 0"), ey("0 1 0");
    arma::vec3 ta = ex - arma::dot(ex, surface_norm_vec_) * surface_norm_vec_;
    ta /= arma::norm(ta, 2); // normalize
    arma::vec3 tb = ey - arma::dot(ey, surface_norm_vec_) * surface_norm_vec_;
    tb /= arma::norm(tb, 2); // normalize
    m_ = arma::mat(2,3);
    m_.row(0) = ta.t();
    m_.row(1) = tb.t();
}


void SurfaceDepth::construct_bih_tree(Mesh *mesh, std::string surface_region)
{
	std::vector<BoundingBox> boxes;
	nodes_coords_.clear();

	RegionSet region_set = mesh->region_db().get_region_set(surface_region);
    if (region_set.size() == 0)
        THROW( RegionDB::ExcUnknownSet() << RegionDB::EI_Label(surface_region) );

    // make element boxes
    unsigned int i=0;
    unsigned int i_node;
    arma::vec3 project_node("0 0 0");
    for( ElementFullIter it = mesh->bc_elements.begin(); it != mesh->bc_elements.end(); ++it) {
        if (it->region().is_in_region_set(region_set)) {
        	if (it->dim()==3) { // dimension 3 is forbidden for surface elements
        		THROW( ExcSurfaceProjection()
        		    			<< EI_Message("Surface region contains one or more elements of dimension 3. Maximal dimension is 2!") );
        	} else if (it->dim()<2) continue; // dimension 0,1 has no effect
        	ASSERT_EQ(it->n_nodes(), 3);

        	arma::mat coords(3,3);
        	coords.col(0) = it->node[0]->point();
        	arma::vec projection = m_ * it->node[0]->point();
        	project_node(0) = projection(0); project_node(1) = projection(1);
            BoundingBox bb(project_node);
            for(i_node=1; i_node<it->n_nodes(); i_node++) {
            	coords.col(i_node) = it->node[i_node]->point();
                arma::vec project_coords = m_ * it->node[i_node]->point();
                project_node(0) = project_coords(0); project_node(1) = project_coords(1);
                bb.expand(project_node);
            }
            boxes.push_back(bb);
            nodes_coords_.push_back(coords);
        }
        i++;
    }

    if ( boxes.size() == 0) {
    	THROW( ExcSurfaceProjection()
    			<< EI_Message("Region " + surface_region + " contains no boundary element! Probably bulk region was set.") );
    }

    bih_tree_.add_boxes( boxes );
    bih_tree_.construct();
}

double SurfaceDepth::compute_distance(arma::vec3 point)
{
	double distance = std::numeric_limits<double>::max();
	arma::vec projection = m_ * point;
	arma::vec3 project_point;
	project_point(0) = projection(0); project_point(1) = projection(1); project_point(2) = 0;

	std::vector<unsigned int> searched_elements;
	bih_tree_.find_point(project_point, searched_elements);
	for (std::vector<unsigned int>::iterator it = searched_elements.begin(); it!=searched_elements.end(); it++) {
		auto coords = nodes_coords_[(*it)];
		arma::vec3 va = coords.col(1) - coords.col(0);
		arma::vec3 vb = coords.col(2) - coords.col(0);

		arma::mat a_mat(3,3);
		a_mat.col(0) = va;
		a_mat.col(1) = vb;
		a_mat.col(2) = surface_norm_vec_;
		arma::vec3 b_vec = point - coords.col(0);
		arma::vec3 x;
		arma::solve(x, a_mat, b_vec);
		if ( (x(0)>=0) && (x(1)>=0) && (x(0)+x(1)<=1) ) { // point is in triangle
			double new_distance = -x(2);
			if ( (new_distance>=0) && (new_distance<distance) ) distance = new_distance;
		}
	}

	return distance;
}