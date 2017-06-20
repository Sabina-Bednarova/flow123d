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
 * @file    output_mesh.cc
 * @brief   Classes for auxiliary output mesh.
 */

#include "output_mesh.hh"
#include "output_element.hh"
#include "mesh/mesh.h"
#include "fields/field.hh"
#include <fields/field_set.hh>

namespace IT=Input::Type;

const IT::Record & OutputMeshBase::get_input_type() {
    return IT::Record("OutputMesh", "Parameters of the refined output mesh.")
        .declare_key("max_level", IT::Integer(1,20),IT::Default("3"),
            "Maximal level of refinement of the output mesh.")
        .declare_key("refine_by_error", IT::Bool(), IT::Default("false"),
            "Set true for using error_control_field. Set false for global uniform refinement to max_level.")
        .declare_key("error_control_field",IT::String(), IT::Default::optional(),
            "Name of an output field, according to which the output mesh will be refined. The field must be a SCALAR one.")
        .close();
}

OutputMeshBase::OutputMeshBase(Mesh &mesh)
: 
	orig_mesh_(&mesh),
    max_level_(0)
{
}


OutputMeshBase::OutputMeshBase(Mesh &mesh, const Input::Record &in_rec)
: 
    input_record_(in_rec), 
    orig_mesh_(&mesh),
    max_level_(input_record_.val<int>("max_level"))
{
}

OutputMeshBase::~OutputMeshBase()
{
}

OutputElementIterator OutputMeshBase::begin()
{
    ASSERT_PTR(offsets_);
    return OutputElementIterator(OutputElement(0, shared_from_this()));
}

OutputElementIterator OutputMeshBase::end()
{
    ASSERT_PTR(offsets_);
    return OutputElementIterator(OutputElement(offsets_->n_values(), shared_from_this()));
}

void OutputMeshBase::select_error_control_field(FieldSet &output_fields)
{
    bool use_field = input_record_.val<bool>("refine_by_error");
    
    if(use_field)
    {
        std::string error_control_field_name = "";
        // Read optional error control field name
        auto it = input_record_.find<std::string>("error_control_field");
        if(it) error_control_field_name = *it;

        FieldCommon* field =  output_fields.field(error_control_field_name);
        // throw input exception if the field is unknown
        if(field == nullptr){
            THROW(FieldSet::ExcUnknownField()
                    << FieldCommon::EI_Field(error_control_field_name)
                    << input_record_.ei_address());
            return;
        }
        
        // throw input exception if the field is not scalar
        if( typeid(*field) == typeid(Field<3,FieldValue<3>::Scalar>) ) {
            
            error_control_field_ = static_cast<Field<3,FieldValue<3>::Scalar>*>(field);
            DebugOut() << "Output mesh will be refined according to field " << error_control_field_name << ".";
        }
        else{
            THROW(ExcFieldNotScalar()
                    << FieldCommon::EI_Field(error_control_field_name)
                    << input_record_.ei_address());
        }
    }
    else
    {
        error_control_field_ = nullptr;
    }
}

unsigned int OutputMeshBase::n_elements()
{
    ASSERT_PTR(offsets_);
    return offsets_->n_values();
}

unsigned int OutputMeshBase::n_nodes()
{
    ASSERT_PTR(nodes_);
    return nodes_->n_values();
}


void OutputMeshBase::create_data_caches()
{
    nodes_ = std::make_shared<ElementDataCache<double>>("", ElementDataCacheBase::N_VECTOR, 1, 1);
    connectivity_ = std::make_shared<ElementDataCache<unsigned int>>("connectivity", ElementDataCacheBase::N_SCALAR, 1, 1);
	offsets_ = std::make_shared<ElementDataCache<unsigned int>>("offsets", ElementDataCacheBase::N_SCALAR, 1, 1);

}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////


OutputMesh::OutputMesh(Mesh  &mesh)
: OutputMeshBase(mesh)
{
}

OutputMesh::OutputMesh(Mesh &mesh, const Input::Record& in_rec)
: OutputMeshBase(mesh, in_rec)
{
}


OutputMesh::~OutputMesh()
{
}


void OutputMesh::create_identical_mesh()
{
	nodes_.reset();
	connectivity_.reset();
	offsets_.reset();

	DebugOut() << "Create outputmesh identical to computational one.";

    const unsigned int n_elements = orig_mesh_->n_elements(),
                       n_nodes = orig_mesh_->n_nodes();

    nodes_ = std::make_shared<ElementDataCache<double>>("", ElementDataCacheBase::N_VECTOR, 1, n_nodes);
    unsigned int coord_id = 0,  // coordinate id in vector
                 node_id = 0;   // node id
    auto &node_vec = *( nodes_->get_component_data(0).get() );
    FOR_NODES(orig_mesh_, node) {
        node->aux = node_id;   // store node index in the auxiliary variable

        // use store value
        node_vec[coord_id] = node->getX();  coord_id++;
        node_vec[coord_id] = node->getY();  coord_id++;
        node_vec[coord_id] = node->getZ();  coord_id++;
        node_id++;
    }
    
    orig_element_indices_ = std::make_shared<std::vector<unsigned int>>(n_elements);

    offsets_ = std::make_shared<ElementDataCache<unsigned int>>("offsets", ElementDataCacheBase::N_SCALAR, 1, n_elements);
    Node* node;
    unsigned int ele_id = 0,
                 connect_id = 0,
                 offset = 0,    // offset of node indices of element in node vector
                 li;            // local node index
    auto &offset_vec = *( offsets_->get_component_data(0).get() );
    FOR_ELEMENTS(orig_mesh_, ele) {
        // increase offset by number of nodes of the simplicial element
        offset += ele->dim() + 1;
        offset_vec[ele_id] = offset;
        (*orig_element_indices_)[ele_id] = ele_id;
        ele_id++;
    }

    const unsigned int n_connectivities = offset_vec[offset_vec.size()-1];
    connectivity_ = std::make_shared<ElementDataCache<unsigned int>>("connectivity", ElementDataCacheBase::N_SCALAR, 1, n_connectivities);
    auto &connect_vec = *( connectivity_->get_component_data(0).get() );
    FOR_ELEMENTS(orig_mesh_, ele) {
        FOR_ELEMENT_NODES(ele, li) {
            node = ele->node[li];
            connect_vec[connect_id] = node->aux;
            connect_id++;
        }
        
    }
    connectivity_->get_component_data(0)->shrink_to_fit();
}

void OutputMesh::create_refined_mesh()
{
    ASSERT(0).error("Not implemented yet.");
}


bool OutputMesh::refinement_criterion()
{
    ASSERT(0).error("Not implemented yet.");
    return false;
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OutputMeshDiscontinuous::OutputMeshDiscontinuous(Mesh &mesh)
: OutputMeshBase(mesh)
{
}

OutputMeshDiscontinuous::OutputMeshDiscontinuous(Mesh &mesh, const Input::Record& in_rec)
: OutputMeshBase(mesh, in_rec)
{
}


OutputMeshDiscontinuous::~OutputMeshDiscontinuous()
{
}


void OutputMeshDiscontinuous::create_mesh(shared_ptr< OutputMesh > output_mesh)
{
	nodes_.reset();
	connectivity_.reset();
	offsets_.reset();

    ASSERT_DBG(output_mesh->nodes_->n_values() > 0);   //continuous data already computed
    
    if (nodes_) return;          //already computed
    
    DebugOut() << "Create discontinuous outputmesh.";
    
    // connectivity = for every element list the nodes => its length corresponds to discontinuous data
    const unsigned int n_corners = output_mesh->connectivity_->n_values();

    // these are the same as in continuous case, so we copy the pointer.
    offsets_ = output_mesh->offsets_;
    orig_element_indices_ = output_mesh->orig_element_indices_;
    
    nodes_ = std::make_shared<ElementDataCache<double>>("", ElementDataCacheBase::N_VECTOR, 1, n_corners);
    connectivity_ = std::make_shared<ElementDataCache<unsigned int>>("connectivity", ElementDataCacheBase::N_SCALAR, 1, n_corners);

    unsigned int coord_id = 0,  // coordinate id in vector
                 corner_id = 0, // corner index (discontinous node)
                 li;            // local node index

    auto &node_vec = *( nodes_->get_component_data(0).get() );
    auto &conn_vec = *( connectivity_->get_component_data(0).get() );
    for(const auto & ele : *output_mesh)
    {
        unsigned int n = ele.n_nodes(), 
                     ele_idx = ele.idx(),
                     con_off = (* offsets_)[ele_idx];
                     
        for(li = 0; li < n; li++)
        {
            // offset of the first coordinate of the first node of the element in nodes_ vector
            unsigned int off = spacedim * (* output_mesh->connectivity_)[con_off - n + li];
            auto &d = *( output_mesh->nodes_->get_component_data(0).get() );
            
            node_vec[coord_id] = d[off];   ++coord_id;
            node_vec[coord_id] = d[off+1]; ++coord_id;
            node_vec[coord_id] = d[off+2]; ++coord_id;
            
            conn_vec[corner_id] = corner_id;
            corner_id++;
        }
    }
}


void OutputMeshDiscontinuous::create_refined_mesh()
{
    ASSERT(0).error("Not implemented yet.");
}

bool OutputMeshDiscontinuous::refinement_criterion()
{
    ASSERT(0).error("Not implemented yet.");
    return false;
}

