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
 * @file    transport_dg.cc
 * @brief   Discontinuous Galerkin method for equation of transport with dispersion.
 * @author  Jan Stebel
 */

#include "system/sys_profiler.hh"
#include "mechanics/elasticity.hh"

#include "io/output_time.hh"
#include "quadrature/quadrature_lib.hh"
#include "fem/mapping_p1.hh"
#include "fem/fe_values.hh"
#include "fem/fe_p.hh"
#include "fem/fe_rt.hh"
#include "fem/fe_system.hh"
#include "fields/field_fe.hh"
#include "la/linsys_PETSC.hh"
#include "coupling/balance.hh"
#include "mesh/neighbours.h"

#include "fields/multi_field.hh"
#include "fields/generic_field.hh"
#include "input/factory.hh"




using namespace Input::Type;


const Record & Elasticity::get_input_type() {
    std::string equation_name = std::string(Elasticity::EqData::name()) + "_FE";
	return IT::Record(
                std::string(equation_name),
                "FEM for linear elasticity.")
           .declare_key("time", TimeGovernor::get_input_type(), Default::obligatory(),
                    "Time governor setting for the secondary equation.")
           .declare_key("balance", Balance::get_input_type(), Default("{}"),
                    "Settings for computing balance.")
           .declare_key("output_stream", OutputTime::get_input_type(), Default::obligatory(),
                    "Parameters of output stream.")
           .declare_key("solver", LinSys_PETSC::get_input_type(), Default::obligatory(),
				"Linear solver for elasticity.")
		   .declare_key("input_fields", Array(
		        Elasticity::EqData()
		            .make_field_descriptor_type(equation_name)),
		        IT::Default::obligatory(),
		        "Input fields of the equation.")
           .declare_key("output",
                EqData().output_fields.make_output_type(equation_name, ""),
                IT::Default("{ \"fields\": [ "+Elasticity::EqData::default_output_field()+" ] }"),
                "Setting of the field output.")
		   .close();
}

const int Elasticity::registrar =
		Input::register_class< Elasticity, Mesh &, const Input::Record>(std::string(Elasticity::EqData::name()) + "_FE") +
		Elasticity::get_input_type().size();



namespace Mechanics {

FEObjects::FEObjects(Mesh *mesh_, unsigned int fe_order)
{
    unsigned int q_order;

	switch (fe_order)
	{
	case 1:
		q_order = 2;
        fe0_ = new FESystem<0>(std::make_shared<FE_P<0> >(1), FEVector, 3);
		fe1_ = new FESystem<1>(std::make_shared<FE_P<1> >(1), FEVector, 3);
        fe2_ = new FESystem<2>(std::make_shared<FE_P<2> >(1), FEVector, 3);
        fe3_ = new FESystem<3>(std::make_shared<FE_P<3> >(1), FEVector, 3);
		break;

	default:
	    q_order=0;
		xprintf(PrgErr, "Unsupported polynomial order %d for finite elements in Elasticity", fe_order);
		break;
	}

	fe_rt1_ = new FE_RT0<1>;
	fe_rt2_ = new FE_RT0<2>;
	fe_rt3_ = new FE_RT0<3>;
    
	q0_ = new QGauss<0>(q_order);
	q1_ = new QGauss<1>(q_order);
	q2_ = new QGauss<2>(q_order);
	q3_ = new QGauss<3>(q_order);

	map1_ = new MappingP1<1,3>;
	map2_ = new MappingP1<2,3>;
	map3_ = new MappingP1<3,3>;

    ds_ = std::make_shared<EqualOrderDiscreteSpace>(mesh_, fe0_, fe1_, fe2_, fe3_);
	dh_ = std::make_shared<DOFHandlerMultiDim>(*mesh_);

	dh_->distribute_dofs(ds_);
}


FEObjects::~FEObjects()
{
	delete fe1_;
	delete fe2_;
	delete fe3_;
	delete fe_rt1_;
	delete fe_rt2_;
	delete fe_rt3_;
	delete q0_;
	delete q1_;
	delete q2_;
	delete q3_;
	delete map1_;
	delete map2_;
	delete map3_;
}

template<> FiniteElement<0> *FEObjects::fe<0>() { return 0; }
template<> FiniteElement<1> *FEObjects::fe<1>() { return fe1_; }
template<> FiniteElement<2> *FEObjects::fe<2>() { return fe2_; }
template<> FiniteElement<3> *FEObjects::fe<3>() { return fe3_; }

template<> FiniteElement<0> *FEObjects::fe_rt<0>() { return 0; }
template<> FiniteElement<1> *FEObjects::fe_rt<1>() { return fe_rt1_; }
template<> FiniteElement<2> *FEObjects::fe_rt<2>() { return fe_rt2_; }
template<> FiniteElement<3> *FEObjects::fe_rt<3>() { return fe_rt3_; }

template<> Quadrature<0> *FEObjects::q<0>() { return q0_; }
template<> Quadrature<1> *FEObjects::q<1>() { return q1_; }
template<> Quadrature<2> *FEObjects::q<2>() { return q2_; }
template<> Quadrature<3> *FEObjects::q<3>() { return q3_; }

template<> MappingP1<1,3> *FEObjects::mapping<1>() { return map1_; }
template<> MappingP1<2,3> *FEObjects::mapping<2>() { return map2_; }
template<> MappingP1<3,3> *FEObjects::mapping<3>() { return map3_; }

std::shared_ptr<DOFHandlerMultiDim> FEObjects::dh() { return dh_; }



void VolumeChange::init(Mesh *mesh)
{
  mesh_ = mesh;
  values_.resize(mesh->get_el_ds()->lsize());
  old_values_.resize(mesh->get_el_ds()->lsize());
  time_ = 0;
  old_time_ = 0;
}

double VolumeChange::value(const ElementAccessor<3> &elem) const
{ 
  return (time_>old_time_)?
          (values_.data()[mesh_->get_row_4_el()[elem.index()] - mesh_->get_el_ds()->begin()]
          -old_values_.data()[mesh_->get_row_4_el()[elem.index()] - mesh_->get_el_ds()->begin()])
          /(time_-old_time_)
         :0;
}


void VolumeChange::set_values(const vector<double> &new_values, double new_time)
{
  for (unsigned int i=0; i<values_.data().size(); i++)
  {
    old_values_.data()[i] = values_.data()[i];
    values_.data()[i] = new_values[i];
  }
  old_time_ = time_;
  time_ = new_time;
}



} // namespace Mechanics







const Selection & Elasticity::EqData::get_bc_type_selection() {
    return Selection("Elasticity_BC_Type", "Types of boundary conditions for heat transfer model.")
            .add_value(bc_type_displacement, "displacement",
                  "Prescribed displacement.")
            .add_value(bc_type_traction, "traction",
                  "Prescribed traction.")
            .close();
}


Elasticity::EqData::EqData()
{
    *this+=bc_type
        .name("bc_type")
        .description(
        "Type of boundary condition.")
        .units( UnitSI::dimensionless() )
        .input_default("\"traction\"")
        .input_selection( get_bc_type_selection() )
        .flags_add(FieldFlag::in_rhs & FieldFlag::in_main_matrix);
        
    *this+=bc_displacement
        .name("bc_displacement")
        .description("Prescribed displacement.")
        .units( UnitSI().m() )
        .input_default("0.0")
        .flags_add(in_rhs);
        
    *this+=bc_traction
        .name("bc_traction")
        .description("Prescribed traction.")
        .units( UnitSI().Pa() )
        .input_default("0.0")
        .flags_add(in_rhs);
        
    *this+=load
        .name("load")
        .description("Prescribed load.")
        .units( UnitSI().N().m(-3) )
        .input_default("0.0")
        .flags_add(in_rhs);
    
    *this+=young_modulus
        .name("young_modulus")
        .description("Young modulus.")
        .units( UnitSI().Pa() )
         .input_default("0.0")
        .flags_add(in_main_matrix);
    
    *this+=poisson_ratio
        .name("poisson_ratio")
        .description("Poisson ratio.")
        .units( UnitSI().dimensionless() )
         .input_default("0.0")
        .flags_add(in_rhs);
        
    *this+=biot_alpha
        .name("biot_alpha")
        .description("Biot coefficient.")
        .units( UnitSI().dimensionless() )
         .input_default("0.0")
        .flags_add(in_rhs);
  
    *this+=fracture_sigma
            .name("fracture_sigma")
            .description(
            "Coefficient of diffusive transfer through fractures (for each substance).")
            .units( UnitSI::dimensionless() )
            .input_default("1.0")
            .flags_add(FieldFlag::in_main_matrix);

    *this += region_id.name("region_id")
    	        .units( UnitSI::dimensionless())
    	        .flags(FieldFlag::equation_external_output);
                
    *this += subdomain.name("subdomain")
      .units( UnitSI::dimensionless() )
      .flags(FieldFlag::equation_external_output);
      
    *this+=cross_section
      .name("cross_section")
      .units( UnitSI().m(3).md() )
      .flags(input_copy & in_time_term & in_main_matrix);

    *this+=output_field
            .name("displacement")
            .units( UnitSI().m() )
            .flags(equation_result);


    // add all input fields to the output list
    output_fields += *this;

}

Elasticity::Elasticity(Mesh & init_mesh, const Input::Record in_rec)
        : EquationBase(init_mesh, in_rec),
		  input_rec(in_rec),
		  allocation_done(false)
{
	// Can not use name() + "constructor" here, since START_TIMER only accepts const char *
	// due to constexpr optimization.
	START_TIMER(EqData::name());

	this->eq_data_ = &data_;
    time_ = new TimeGovernor(in_rec.val<Input::Record>("time"));


    // Set up physical parameters.
    data_.set_mesh(init_mesh);
    data_.region_id = GenericField<3>::region_id(*mesh_);
    data_.subdomain = GenericField<3>::subdomain(*mesh_);
    
    // create finite element structures and distribute DOFs
    feo = new Mechanics::FEObjects(mesh_, 1);
    DebugOut().fmt("Mechanics: solution size {}\n", feo->dh()->n_global_dofs());
    
    volume_change.init(mesh_);

}


void Elasticity::initialize()
{
    output_stream_ = OutputTime::create_output_stream("mechanics", input_rec.val<Input::Record>("output_stream"), time().get_unit_string());
    
    data_.set_components({"displacement"});
    data_.set_input_list( input_rec.val<Input::Array>("input_fields"), time() );
    
    balance_ = std::make_shared<Balance>("mechanics", mesh_);
    balance_->init_from_input(input_rec.val<Input::Record>("balance"), *time_);
    // initialization of balance object
    subst_idx = {balance_->add_quantity("momentum?")};
//     balance_->units(UnitSI().m(2).kg().s(-2));

    // Resize coefficient arrays
    int qsize = max(feo->q<0>()->size(), max(feo->q<1>()->size(), max(feo->q<2>()->size(), feo->q<3>()->size())));
    int max_edg_sides = max(mesh_->max_edge_sides(1), max(mesh_->max_edge_sides(2), mesh_->max_edge_sides(3)));
    ad_coef.resize(qsize);
    dif_coef.resize(qsize);
    ad_coef_edg.resize(max_edg_sides);
    dif_coef_edg.resize(max_edg_sides);
    for (int sd=0; sd<max_edg_sides; sd++)
    {
        ad_coef_edg[sd].resize(qsize);
        dif_coef_edg[sd].resize(qsize);
    }

	int rank;
	MPI_Comm_rank(PETSC_COMM_WORLD, &rank);
	unsigned int output_vector_size= (rank==0)?feo->dh()->n_global_dofs():0;
    output_vec.resize(output_vector_size);
    data_.output_type(OutputTime::NODE_DATA);

    // create shared pointer to a FieldFE, pass FE data and push this FieldFE to output_field on all regions
    std::shared_ptr<FieldFE<3, FieldValue<3>::VectorFixed> > output_field_ptr(new FieldFE<3, FieldValue<3>::VectorFixed>);
    output_field_ptr->set_fe_data(feo->dh(), 0, &output_vec);
    data_.output_field.set_field(mesh_->region_db().get_region_set("ALL"), output_field_ptr, 0.);
    data_.output_type(OutputTime::CORNER_DATA);

    // set time marks for writing the output
    data_.output_fields.initialize(output_stream_, mesh_, input_rec.val<Input::Record>("output"), this->time());

    // equation default PETSc solver options
    std::string petsc_default_opts;
    petsc_default_opts = "-ksp_type cg -pc_type hypre -pc_hypre_type boomeramg";
    
    // allocate matrix and vector structures
    ls = new LinSys_PETSC(feo->dh()->distr().get(), petsc_default_opts);
    ( (LinSys_PETSC *)ls )->set_from_input( input_rec.val<Input::Record>("solver") );
    ls->set_solution(NULL);

    ls_dt = new LinSys_PETSC(feo->dh()->distr().get(), petsc_default_opts);
    ( (LinSys_PETSC *)ls_dt )->set_from_input( input_rec.val<Input::Record>("solver") );

    // initialization of balance object
    balance_->allocate(feo->dh()->distr()->lsize(),
            max(feo->fe<1>()->n_dofs(), max(feo->fe<2>()->n_dofs(), feo->fe<3>()->n_dofs())));

}


Elasticity::~Elasticity()
{
//     delete time_;

    MatDestroy(&stiffness_matrix);
    MatDestroy(&mass_matrix);
    VecDestroy(&rhs);
    delete ls;
    delete ls_dt;
    delete feo;

}



void Elasticity::output_vector_gather()
{
    VecScatter output_scatter;
    VecScatterCreateToZero(ls->get_solution(), &output_scatter, PETSC_NULL);
    // gather solution to output_vec
    VecScatterBegin(output_scatter, ls->get_solution(), output_vec.petsc_vec(), INSERT_VALUES, SCATTER_FORWARD);
    VecScatterEnd(output_scatter, ls->get_solution(), output_vec.petsc_vec(), INSERT_VALUES, SCATTER_FORWARD);
    VecScatterDestroy(&(output_scatter));
}




void Elasticity::zero_time_step()
{
	START_TIMER(EqData::name());
	data_.mark_input_times( *time_ );
	data_.set_time(time_->step(), LimitSide::left);
	std::stringstream ss; // print warning message with table of uninitialized fields
	if ( FieldCommon::print_message_table(ss, "mechanics") ) {
		WarningOut() << ss.str();
	}


    // set initial conditions
    set_initial_condition();
    ( (LinSys_PETSC *)ls )->set_initial_guess_nonzero();

    // check first time assembly - needs preallocation
    if (!allocation_done) preallocate();

    // after preallocation we assemble the matrices and vectors required for mass balance
//     balance_->calculate_instant(subst_idx, ls->get_solution());

    output_data();
    
    update_volume_change();
}



void Elasticity::preallocate()
{
	// preallocate system matrix
    // preallocate system matrix
    ls->start_allocation();
    stiffness_matrix = NULL;
    rhs = NULL;

    // preallocate mass matrix
    ls_dt->start_allocation();
    mass_matrix = NULL;
	assemble_stiffness_matrix();
	set_sources();
	set_boundary_conditions();

	allocation_done = true;
}




void Elasticity::update_solution()
{
	START_TIMER("DG-ONE STEP");

	time_->next_time();
	time_->view("MECH");
    
    START_TIMER("data reinit");
    data_.set_time(time_->step(), LimitSide::left);
    END_TIMER("data reinit");
    
	// assemble stiffness matrix
    if (stiffness_matrix == NULL
    		|| data_.subset(FieldFlag::in_main_matrix).changed()
    		|| flux_changed)
    {
        // new fluxes can change the location of Neumann boundary,
        // thus stiffness matrix must be reassembled
        ls->start_add_assembly();
        ls->mat_zero_entries();
        assemble_stiffness_matrix();
        ls->finish_assembly();
//         ls->apply_constrains(1);

        if (stiffness_matrix == NULL)
            MatConvert(*( ls->get_matrix() ), MATSAME, MAT_INITIAL_MATRIX, &stiffness_matrix);
        else
            MatCopy(*( ls->get_matrix() ), stiffness_matrix, DIFFERENT_NONZERO_PATTERN);
    }

    // assemble right hand side (due to sources and boundary conditions)
    if (rhs == NULL
    		|| data_.subset(FieldFlag::in_rhs).changed()
    		|| flux_changed)
    {
        ls->start_add_assembly();
        ls->rhs_zero_entries();
    	set_sources();
    	set_boundary_conditions();
        ls->finish_assembly();
        ls->apply_constrains(1.0);

        if (rhs == nullptr) VecDuplicate(*( ls->get_rhs() ), &rhs);
        VecCopy(*( ls->get_rhs() ), rhs);
    }

    flux_changed = false;


    START_TIMER("solve");

    ls->solve();
    
    END_TIMER("solve");

    calculate_cumulative_balance();
    update_volume_change();
    
    output_data();

    END_TIMER("DG-ONE STEP");
}




void Elasticity::output_data()
{
    START_TIMER("MECH-OUTPUT");

    // gather the solution from all processors
    data_.output_fields.set_time( this->time().step(), LimitSide::left);
    //if (data_.output_fields.is_field_output_time(data_.output_field, this->time().step()) )
    output_vector_gather();
    data_.output_fields.output(this->time().step());
    output_stream_->write_time_frame();

//     START_TIMER("MECH-balance");
//     balance_->calculate_instant(subst_idx, ls->get_solution());
//     balance_->output();
//     END_TIMER("MECH-balance");

    END_TIMER("MECH-OUTPUT");
}



void Elasticity::calculate_cumulative_balance()
{
    if (balance_->cumulative())
    {
        balance_->calculate_cumulative(subst_idx, ls->get_solution());
    }
}








void Elasticity::assemble_stiffness_matrix()
{
  START_TIMER("assemble_stiffness");
   START_TIMER("assemble_volume_integrals");
    assemble_volume_integrals<1>();
    assemble_volume_integrals<2>();
    assemble_volume_integrals<3>();
   END_TIMER("assemble_volume_integrals");
   
  START_TIMER("assemble_fluxes_boundary");
    assemble_fluxes_boundary<1>();
    assemble_fluxes_boundary<2>();
    assemble_fluxes_boundary<3>();
   END_TIMER("assemble_fluxes_boundary");

   START_TIMER("assemble_fluxes_elem_side");
    assemble_fluxes_element_side<1>();
    assemble_fluxes_element_side<2>();
    assemble_fluxes_element_side<3>();
   END_TIMER("assemble_fluxes_elem_side");
  END_TIMER("assemble_stiffness");
}




template<unsigned int dim>
void Elasticity::assemble_volume_integrals()
{
//     FEValues<dim,3> fv_rt(*feo->mapping<dim>(), *feo->q<dim>(), *feo->fe_rt<dim>(),
//     		update_values | update_gradients);
    FEValues<dim,3> fe_values(*feo->mapping<dim>(), *feo->q<dim>(), *feo->fe<dim>(),
    		update_values | update_gradients | update_JxW_values | update_quadrature_points);
    const unsigned int ndofs = feo->fe<dim>()->n_dofs(), qsize = feo->q<dim>()->size();
    vector<int> dof_indices(ndofs);
    vector<arma::vec3> velocity(qsize);
    vector<double> young(qsize), poisson(qsize), csection(qsize);
    PetscScalar local_matrix[ndofs*ndofs];
    auto vec = fe_values.vector_view(0);

	// assemble integral over elements
    for (auto cell : feo->dh()->own_range())
    {
        if (cell.dim() != dim) continue;
        ElementAccessor<3> elm_acc = cell.elm();

        fe_values.reinit(elm_acc);
//         fv_rt.reinit(cell);
        cell.get_dof_indices(dof_indices);

//         calculate_velocity(cell, velocity, fv_rt);
        
        data_.cross_section.value_list(fe_values.point_list(), elm_acc, csection);
        data_.young_modulus.value_list(fe_values.point_list(), elm_acc, young);
        data_.poisson_ratio.value_list(fe_values.point_list(), elm_acc, poisson);
        
        // assemble the local stiffness matrix
        for (unsigned int i=0; i<ndofs; i++)
            for (unsigned int j=0; j<ndofs; j++)
                local_matrix[i*ndofs+j] = 0;

        for (unsigned int k=0; k<qsize; k++)
        {
          double mu = young[k]*0.5/(poisson[k]+1.);
          double lambda = young[k]*poisson[k]/((poisson[k]+1.)*(1.-2.*poisson[k]));
          
          for (unsigned int i=0; i<ndofs; i++)
          {
              for (unsigned int j=0; j<ndofs; j++)
                  local_matrix[i*ndofs+j] += csection[k]*(mu*arma::dot(vec.sym_grad(j,k), vec.sym_grad(i,k))
                                              +lambda*vec.divergence(j,k)*vec.divergence(i,k)
                                              )*fe_values.JxW(k);
          }
        }
        ls->mat_set_values(ndofs, &(dof_indices[0]), ndofs, &(dof_indices[0]), local_matrix);
    }
}



void Elasticity::set_sources()
{
  START_TIMER("assemble_sources");
    balance_->start_source_assembly(subst_idx);
	set_sources<1>();
	set_sources<2>();
	set_sources<3>();
	balance_->finish_source_assembly(subst_idx);
  END_TIMER("assemble_sources");
}


template<unsigned int dim>
void Elasticity::set_sources()
{
    FEValues<dim,3> fe_values(*feo->mapping<dim>(), *feo->q<dim>(), *feo->fe<dim>(),
    		update_values | update_gradients | update_JxW_values | update_quadrature_points);
    const unsigned int ndofs = feo->fe<dim>()->n_dofs(), qsize = feo->q<dim>()->size();
    vector<arma::vec3> load(qsize);
    vector<double> alpha(qsize), csection(qsize);
    vector<int> dof_indices(ndofs);
    PetscScalar local_rhs[ndofs];
    vector<PetscScalar> local_source_balance_vector(ndofs), local_source_balance_rhs(ndofs);
    auto vec = fe_values.vector_view(0);

	// assemble integral over elements
    for (auto cell : feo->dh()->own_range())
    {
        if (cell.dim() != dim) continue;
        ElementAccessor<3> elm_acc = cell.elm();

        fe_values.reinit(elm_acc);
        cell.get_dof_indices(dof_indices);

        // assemble the local stiffness matrix
        fill_n(local_rhs, ndofs, 0);
        local_source_balance_vector.assign(ndofs, 0);
        local_source_balance_rhs.assign(ndofs, 0);
        
        data_.cross_section.value_list(fe_values.point_list(), elm_acc, csection);
        data_.load.value_list(fe_values.point_list(), elm_acc, load);
        data_.biot_alpha.value_list(fe_values.point_list(), elm_acc, alpha);

        // compute sources
        for (unsigned int k=0; k<qsize; k++)
        {
            for (unsigned int i=0; i<ndofs; i++)
                local_rhs[i] += (arma::dot(load[k], vec.value(i,k))
                                 +alpha[k]*mh_dh->element_scalar(elm_acc)*vec.divergence(i,k)
                                )*csection[k]*fe_values.JxW(k);
        }
        ls->rhs_set_values(ndofs, &(dof_indices[0]), local_rhs);

        for (unsigned int i=0; i<ndofs; i++)
        {
            for (unsigned int k=0; k<qsize; k++)
                local_source_balance_vector[i] -= 0;//sources_sigma[k]*fe_values[vec].value(i,k)*fe_values.JxW(k);

            local_source_balance_rhs[i] += local_rhs[i];
        }
        balance_->add_source_matrix_values(subst_idx, elm_acc.region().bulk_idx(), dof_indices, local_source_balance_vector);
        balance_->add_source_vec_values(subst_idx, elm_acc.region().bulk_idx(), dof_indices, local_source_balance_rhs);
    }
}







template<unsigned int dim>
void Elasticity::assemble_fluxes_boundary()
{
    FESideValues<dim,3> fe_values_side(*feo->mapping<dim>(), *feo->q<dim-1>(), *feo->fe<dim>(),
    		update_values | update_gradients | update_side_JxW_values | update_normal_vectors | update_quadrature_points);
//     FESideValues<dim,3> fsv_rt(*feo->mapping<dim>(), *feo->q<dim-1>(), *feo->fe_rt<dim>(),
//     		update_values);
    const unsigned int ndofs = feo->fe<dim>()->n_dofs(), qsize = feo->q<dim-1>()->size();
    vector<int> side_dof_indices(ndofs);
    PetscScalar local_matrix[ndofs*ndofs];
    auto vec = fe_values_side.vector_view(0);
//     vector<arma::vec3> side_velocity;
//     vector<double> robin_sigma(qsize);
//     vector<double> csection(qsize);
//     arma::vec dg_penalty;
//     double gamma_l;
// 
    // assemble boundary integral
    for (unsigned int iedg=0; iedg<feo->dh()->n_loc_edges(); iedg++)
    {
    	Edge *edg = &mesh_->edges[feo->dh()->edge_index(iedg)];
    	if (edg->n_sides > 1) continue;
    	// check spatial dimension
    	if (edg->side(0)->dim() != dim-1) continue;
    	// skip edges lying not on the boundary
    	if (edg->side(0)->cond() == NULL) continue;

    	SideIter side = edg->side(0);
        ElementAccessor<3> cell = side->element();
        feo->dh()->get_dof_indices(cell, side_dof_indices);
        fe_values_side.reinit(cell, side->side_idx());
//         fsv_rt.reinit(cell, side->el_idx());
// 
//         calculate_velocity(cell, side_velocity, fsv_rt);
//         Model::compute_advection_diffusion_coefficients(fe_values_side.point_list(), side_velocity, ele_acc, ad_coef, dif_coef);
//         dg_penalty = data_.dg_penalty.value(cell->centre(), ele_acc);
        unsigned int bc_type = data_.bc_type.value(side->centre(), side->cond()->element_accessor());
//         data_.cross_section.value_list(fe_values_side.point_list(), ele_acc, csection);
// 
        	for (unsigned int i=0; i<ndofs; i++)
        		for (unsigned int j=0; j<ndofs; j++)
        			local_matrix[i*ndofs+j] = 0;

			ls->mat_set_values(ndofs, &(side_dof_indices[0]), ndofs, &(side_dof_indices[0]), local_matrix);
    }
}


arma::mat33 mat_t(const arma::mat33 &m, const arma::vec3 &n)
{
  arma::mat33 mt = m - m*arma::kron(n,n.t());
  return mt;
}


template<unsigned int dim>
void Elasticity::assemble_fluxes_element_side()
{
	if (dim == 1) return;
    FEValues<dim-1,3> fe_values_sub(*feo->mapping<dim-1>(), *feo->q<dim-1>(), *feo->fe<dim-1>(),
    		update_values | update_gradients | update_JxW_values | update_quadrature_points);
    FESideValues<dim,3> fe_values_side(*feo->mapping<dim>(), *feo->q<dim-1>(), *feo->fe<dim>(),
    		update_values | update_gradients | update_side_JxW_values | update_normal_vectors | update_quadrature_points);
//     FESideValues<dim,3> fsv_rt(*feo->mapping<dim>(), *feo->q<dim-1>(), *feo->fe_rt<dim>(),
//        		update_values);
//     FEValues<dim-1,3> fv_rt(*feo->mapping<dim-1>(), *feo->q<dim-1>(), *feo->fe_rt<dim-1>(),
//        		update_values);
// 
    vector<FEValuesSpaceBase<3>*> fv_sb(2);
    const unsigned int ndofs_side = feo->fe<dim>()->n_dofs();    // number of local dofs
    const unsigned int ndofs_sub  = feo->fe<dim-1>()->n_dofs();
    const unsigned int qsize = feo->q<dim-1>()->size();     // number of quadrature points
    vector<vector<int> > side_dof_indices(2);
    vector<unsigned int> n_dofs = { ndofs_sub, ndofs_side };
// 	vector<arma::vec3> velocity_higher, velocity_lower;
	vector<double> frac_sigma(qsize);
	vector<double> csection_lower(qsize), csection_higher(qsize), young(qsize), poisson(qsize), alpha(qsize);
    PetscScalar local_matrix[2][2][(ndofs_side)*(ndofs_side)];
    PetscScalar local_rhs[2][ndofs_side];
    auto vec_side = fe_values_side.vector_view(0);
    auto vec_sub = fe_values_sub.vector_view(0);

    // index 0 = element with lower dimension,
    // index 1 = side of element with higher dimension
    side_dof_indices[0].resize(ndofs_sub);
    side_dof_indices[1].resize(ndofs_side);
    fv_sb[0] = &fe_values_sub;
    fv_sb[1] = &fe_values_side;

    // assemble integral over sides
    for (unsigned int inb=0; inb<feo->dh()->n_loc_nb(); inb++)
    {
    	Neighbour *nb = &mesh_->vb_neighbours_[feo->dh()->nb_index(inb)];
        // skip neighbours of different dimension
        if (nb->element()->dim() != dim-1) continue;

		ElementAccessor<3> cell_sub = nb->element();
		feo->dh()->get_dof_indices(cell_sub, side_dof_indices[0]);
		fe_values_sub.reinit(cell_sub);

		ElementAccessor<3> cell = nb->side()->element();
		feo->dh()->get_dof_indices(cell, side_dof_indices[1]);
		fe_values_side.reinit(cell, nb->side()->side_idx());

		// Element id's for testing if they belong to local partition.
		int element_id[2];
		element_id[0] = cell_sub.index();
		element_id[1] = cell.index();
        
// 		fsv_rt.reinit(cell, nb->side()->el_idx());
// 		fv_rt.reinit(cell_sub);
// 		calculate_velocity(cell, velocity_higher, fsv_rt);
// 		calculate_velocity(cell_sub, velocity_lower, fv_rt);
		data_.cross_section.value_list(fe_values_sub.point_list(), cell_sub, csection_lower);
		data_.cross_section.value_list(fe_values_sub.point_list(), cell, csection_higher);
 		data_.fracture_sigma.value_list(fe_values_sub.point_list(), cell_sub, frac_sigma);
        data_.young_modulus.value_list(fe_values_sub.point_list(), cell_sub, young);
        data_.poisson_ratio.value_list(fe_values_sub.point_list(), cell_sub, poisson);
        data_.biot_alpha.value_list(fe_values_sub.point_list(), cell_sub, alpha);
        
        for (unsigned int n=0; n<2; ++n)
        {
            for (unsigned int i=0; i<ndofs_side; i++)
            {
                for (unsigned int m=0; m<2; ++m)
                {
                    for (unsigned int j=0; j<ndofs_side; j++)
                        local_matrix[n][m][i*(ndofs_side)+j] = 0;
                }
                local_rhs[n][i] = 0;
            }
        }

        // set transmission conditions
        for (unsigned int k=0; k<qsize; k++)
        {
            arma::vec3 nv = fe_values_side.normal_vector(k);
            double mu = young[k]*0.5/(poisson[k]+1.);
            double lambda = young[k]*poisson[k]/((poisson[k]+1.)*(1.-2*poisson[k]));
            
            for (int n=0; n<2; n++)
            {
                if (!feo->dh()->el_is_local(element_id[n])) continue;

                for (unsigned int i=0; i<n_dofs[n]; i++)
                {
                    arma::vec3 vi = (n==0)?arma::zeros(3):vec_side.value(i,k);
                    arma::vec3 vf = (n==1)?arma::zeros(3):vec_sub.value(i,k);
                    arma::mat33 gvf = (n==0)?mat_t(vec_sub.grad(i,k),nv):arma::zeros(3,3);
                    double divvf = (n==0)?arma::trace(mat_t(vec_sub.grad(i,k),nv)):0;
                    
                    for (int m=0; m<2; m++)
                        for (unsigned int j=0; j<n_dofs[m]; j++) {
                            arma::vec3 ui = (m==0)?arma::zeros(3):vec_side.value(j,k);
                            arma::vec3 uf = (m==1)?arma::zeros(3):vec_sub.value(j,k);
                            arma::mat33 gui = (m==1)?vec_side.grad(j,k):arma::zeros(3,3);
                            double divui = (m==1)?arma::trace(mat_t(vec_side.grad(j,k),nv)):0;
                            
                            local_matrix[n][m][i*n_dofs[m] + j] +=
                                    frac_sigma[k]*(arma::dot(vf-vi,
                                      2/csection_lower[k]*(mu*(uf-ui)+(mu+lambda)*(arma::dot(uf-ui,nv)*nv))
                                      + mu*(arma::trans(gui)*nv - arma::dot(gui*nv,nv)*nv)
                                      + lambda*divui*nv
                                     )
                                     + arma::dot(gvf, mu*arma::kron(ui,nv.t()) - lambda*arma::dot(ui,nv))
                                    )*fv_sb[0]->JxW(k);
                        }
                    
                    local_rhs[n][i] -=
                                    frac_sigma[k]*arma::dot(vi-vf,
                                       alpha[k]*mh_dh->element_scalar(cell_sub)*nv
                                    )*fv_sb[0]->JxW(k);
                }
            }
        }
        for (unsigned int n=0; n<2; ++n)
        {
            for (unsigned int m=0; m<2; ++m)
                ls->mat_set_values(n_dofs[n], side_dof_indices[n].data(), n_dofs[m], side_dof_indices[m].data(), local_matrix[n][m]);
            
            ls->rhs_set_values(n_dofs[n], side_dof_indices[n].data(), local_rhs[n]);
        }
    }
}







void Elasticity::set_boundary_conditions()
{
  START_TIMER("assemble_bc");
    balance_->start_flux_assembly(subst_idx);
	set_boundary_conditions<1>();
	set_boundary_conditions<2>();
	set_boundary_conditions<3>();
	balance_->finish_flux_assembly(subst_idx);
  END_TIMER("assemble_bc");
}



template<unsigned int dim>
void Elasticity::set_boundary_conditions()
{
    FESideValues<dim,3> fe_values_side(*feo->mapping<dim>(), *feo->q<dim-1>(), *feo->fe<dim>(),
    		update_values | update_gradients | update_normal_vectors | update_side_JxW_values | update_quadrature_points);
    FESideValues<dim,3> fsv_rt(*feo->mapping<dim>(), *feo->q<dim-1>(), *feo->fe_rt<dim>(),
           		update_values);
    const unsigned int ndofs = feo->fe<dim>()->n_dofs(), qsize = feo->q<dim-1>()->size();
    vector<int> side_dof_indices(ndofs);
    unsigned int loc_b=0;
    double local_rhs[ndofs];
    vector<PetscScalar> local_flux_balance_vector(ndofs);
    PetscScalar local_flux_balance_rhs;
    vector<double> bc_values(qsize);
    vector<double> bc_fluxes(qsize),
			bc_sigma(qsize),
			bc_ref_values(qsize);
    vector<double> csection(qsize);
	vector<arma::vec3> velocity;
    auto vec = fe_values_side.vector_view(0);

    for (unsigned int loc_el = 0; loc_el < mesh_->get_el_ds()->lsize(); loc_el++)
    {
        ElementAccessor<3> elm = mesh_->element_accessor(feo->dh()->el_index(loc_el));
        if (elm->boundary_idx_ == nullptr) continue;

        for (unsigned int si=0; si<elm->n_sides(); ++si)
        {
			const Edge *edg = elm.side(si)->edge();
			if (edg->n_sides > 1) continue;
			// skip edges lying not on the boundary
			if (edg->side(0)->cond() == NULL) continue;

			if (edg->side(0)->dim() != dim-1)
			{
				if (edg->side(0)->cond() != nullptr) ++loc_b;
				continue;
			}

			SideIter side = edg->side(0);
			ElementAccessor<3> cell = side->element();
			ElementAccessor<3> bc_cell = side->cond()->element_accessor();

 			unsigned int bc_type = data_.bc_type.value(side->centre(), bc_cell);

			fe_values_side.reinit(cell, side->side_idx());
// 			fsv_rt.reinit(cell, side->el_idx());
// 			calculate_velocity(cell, velocity, fsv_rt);

// 			compute_advection_diffusion_coefficients(fe_values_side.point_list(), velocity, cell, ad_coef, dif_coef);
			data_.cross_section.value_list(fe_values_side.point_list(), cell, csection);
			// The b.c. data are fetched for all possible b.c. types since we allow
			// different bc_type for each substance.
// 			data_.bc_dirichlet_value.value_list(fe_values_side.point_list(), bc_cell, bc_values);

			feo->dh()->get_dof_indices(cell, side_dof_indices);

            fill_n(local_rhs, ndofs, 0);
            local_flux_balance_vector.assign(ndofs, 0);
            local_flux_balance_rhs = 0;

//             double side_flux = 0;
//             for (unsigned int k=0; k<qsize; k++)
//                 side_flux += arma::dot(ad_coef[k], fe_values_side.normal_vector(k))*fe_values_side.JxW(k);
//             double transport_flux = side_flux/side->measure();

            if (bc_type == EqData::bc_type_displacement)
            {
                arma::mat d_mat(ndofs,ndofs);
                arma::vec d_vec(ndofs);
                for (unsigned int i=0; i<ndofs; i++)
                {
                    d_vec(i) = 0;
                    for (unsigned int k=0; k<qsize; k++)
                        d_vec(i) += arma::dot(vec.value(i,k),data_.bc_displacement.value(fe_values_side.point(k), bc_cell))*fe_values_side.JxW(k);
                    for (unsigned int j=i; j<ndofs; j++)
                    {
                        d_mat(i,j) = 0;
                        for (unsigned int k=0; k<qsize; k++)
                            d_mat(i,j) += arma::dot(vec.value(i,k),vec.value(j,k))*fe_values_side.JxW(k);
                        d_mat(j,i) = d_mat(i,j);
                    }
                }
                arma::vec d_val = pinv(d_mat)*d_vec;
                
                for (unsigned int i=0; i<ndofs; i++)
                    if (norm(d_mat.row(i)) > 0)
                        ls->add_constraint(side_dof_indices[i], d_val(i));
            }
            else if (bc_type == EqData::bc_type_traction)
            {
              for (unsigned int k=0; k<qsize; k++)
              {
                for (unsigned int i=0; i<ndofs; i++)
                  local_rhs[i] += csection[k]*arma::dot(vec.value(i,k),data_.bc_traction.value(fe_values_side.point(k), bc_cell))*fe_values_side.JxW(k);
              }
            }
//             else if (bc_type[sbi] == AdvectionDiffusionModel::abc_total_flux)
//             {
//                 Model::get_flux_bc_data(sbi, fe_values_side.point_list(), ele_acc, bc_fluxes, bc_sigma, bc_ref_values);
//                 for (unsigned int k=0; k<qsize; k++)
//                 {
//                     double bc_term = csection[k]*(bc_sigma[k]*bc_ref_values[k]+bc_fluxes[k])*fe_values_side.JxW(k);
//                     for (unsigned int i=0; i<ndofs; i++)
//                         local_rhs[i] += bc_term*fe_values_side.shape_value(i,k);
//                 }
// 
//                 for (unsigned int i=0; i<ndofs; i++)
//                 {
//                     for (unsigned int k=0; k<qsize; k++)
//                         local_flux_balance_vector[i] += csection[k]*bc_sigma[k]*fe_values_side.JxW(k)*fe_values_side.shape_value(i,k);
//                     local_flux_balance_rhs -= local_rhs[i];
//                 }
//             }
//             else if (bc_type[sbi] == AdvectionDiffusionModel::abc_diffusive_flux)
//             {
//                 Model::get_flux_bc_data(sbi, fe_values_side.point_list(), ele_acc, bc_fluxes, bc_sigma, bc_ref_values);
//                 for (unsigned int k=0; k<qsize; k++)
//                 {
//                     double bc_term = csection[k]*(bc_sigma[k]*bc_ref_values[k]+bc_fluxes[k])*fe_values_side.JxW(k);
//                     for (unsigned int i=0; i<ndofs; i++)
//                         local_rhs[i] += bc_term*fe_values_side.shape_value(i,k);
//                 }
// 
//                 for (unsigned int i=0; i<ndofs; i++)
//                 {
//                     for (unsigned int k=0; k<qsize; k++)
//                         local_flux_balance_vector[i] += csection[k]*(arma::dot(ad_coef[sbi][k], fe_values_side.normal_vector(k)) + bc_sigma[k])*fe_values_side.JxW(k)*fe_values_side.shape_value(i,k);
//                     local_flux_balance_rhs -= local_rhs[i];
//                 }
//             }
//             else if (bc_type[sbi] == AdvectionDiffusionModel::abc_inflow && side_flux >= 0)
//             {
//                 for (unsigned int k=0; k<qsize; k++)
//                 {
//                     for (unsigned int i=0; i<ndofs; i++)
//                         local_flux_balance_vector[i] += arma::dot(ad_coef[sbi][k], fe_values_side.normal_vector(k))*fe_values_side.JxW(k)*fe_values_side.shape_value(i,k);
//                 }
//             }
            ls->rhs_set_values(ndofs, &(side_dof_indices[0]), local_rhs);


            
            balance_->add_flux_matrix_values(subst_idx, loc_b, side_dof_indices, local_flux_balance_vector);
            balance_->add_flux_vec_value(subst_idx, loc_b, local_flux_balance_rhs);
			++loc_b;
        }
    }
}




template<unsigned int dim>
void Elasticity::calculate_velocity(const ElementAccessor<3> &cell, 
                                            vector<arma::vec3> &velocity, 
                                            FEValuesBase<dim,3> &fv)
{
	OLD_ASSERT(cell->dim() == dim, "Element dimension mismatch!");

    velocity.resize(fv.n_points());

    for (unsigned int k=0; k<fv.n_points(); k++)
    {
        velocity[k].zeros();
        for (unsigned int sid=0; sid<cell->n_sides(); sid++)
          for (unsigned int c=0; c<3; ++c)
            velocity[k][c] += fv.shape_value_component(sid,k,c) * mh_dh->side_flux( *(cell.side(sid)) );
    }
}












void Elasticity::set_initial_condition()
{
	START_TIMER("set_init_cond");
    ls->start_allocation();
	prepare_initial_condition<1>();
	prepare_initial_condition<2>();
	prepare_initial_condition<3>();

    ls->start_add_assembly();
	prepare_initial_condition<1>();
	prepare_initial_condition<2>();
	prepare_initial_condition<3>();

    ls->finish_assembly();
    ls->solve();
	END_TIMER("set_init_cond");
}


template<unsigned int dim>
void Elasticity::prepare_initial_condition()
{
	FEValues<dim,3> fe_values(*feo->mapping<dim>(), *feo->q<dim>(), *feo->fe<dim>(),
			update_values | update_JxW_values | update_quadrature_points);
    const unsigned int ndofs = feo->fe<dim>()->n_dofs(), qsize = feo->q<dim>()->size();
    vector<int> dof_indices(ndofs);
    double matrix[ndofs*ndofs], rhs[ndofs];
    std::vector<arma::vec3> init_values(qsize, arma::vec3({0., 0., 0.}));
    auto vec = fe_values.vector_view(0);

    for (unsigned int i_cell=0; i_cell<mesh_->get_el_ds()->lsize(); i_cell++)
    {
    	ElementAccessor<3> elem = mesh_->element_accessor(feo->dh()->el_index(i_cell));
    	if (elem->dim() != dim) continue;

    	feo->dh()->get_dof_indices(elem, dof_indices);
    	fe_values.reinit(elem);

//    		compute_init_cond(fe_values.point_list(), elem, init_values);

        for (unsigned int i=0; i<ndofs; i++)
        {
            rhs[i] = 0;
            for (unsigned int j=0; j<ndofs; j++)
                matrix[i*ndofs+j] = 0;
        }

        for (unsigned int k=0; k<qsize; k++)
        {
            arma::vec3 rhs_term = init_values[k]*fe_values.JxW(k);

            for (unsigned int i=0; i<ndofs; i++)
            {
                for (unsigned int j=0; j<ndofs; j++)
                    matrix[i*ndofs+j] += arma::dot(vec.value(i,k), vec.value(j,k))*fe_values.JxW(k);

                rhs[i] += arma::dot(vec.value(i,k),rhs_term);
            }
        }
        ls->set_values(ndofs, &(dof_indices[0]), ndofs, &(dof_indices[0]), matrix, rhs);
    }
}


void Elasticity::update_volume_change()
{
  vector<double> divergence(mesh_->get_el_ds()->lsize(), 0.);
  update_volume_change<1>(divergence);
  update_volume_change<2>(divergence);
  update_volume_change<3>(divergence);
  volume_change.set_values(divergence, time_->t());
}

template<unsigned int dim>
void Elasticity::update_volume_change(vector<double> &divergence)
{
    QGauss<dim> q(1);
    FEValues<dim,3> fe_values(*feo->mapping<dim>(), q, *feo->fe<dim>(), update_values | update_gradients);
    const unsigned int ndofs = feo->fe<dim>()->n_dofs();
    vector<int> dof_indices(ndofs);
    auto vec = fe_values.vector_view(0);

    // assemble integral over elements
    for (unsigned int i_cell=0; i_cell<mesh_->get_el_ds()->lsize(); i_cell++)
    {
        ElementAccessor<3> cell = mesh_->element_accessor(feo->dh()->el_index(i_cell));
        if (cell->dim() != dim) continue;

        fe_values.reinit(cell);
        feo->dh()->get_loc_dof_indices(cell, dof_indices);
        
        double alpha = data_.biot_alpha.value(cell.centre(), cell);
        
        // assemble the local stiffness matrix
        for (unsigned int i=0; i<ndofs; i++)
          divergence[i_cell] -= alpha*ls->get_solution_array()[dof_indices[i]]*vec.divergence(i,0);
    }
}



void Elasticity::get_par_info(int * &el_4_loc, Distribution * &el_ds)
{
	el_4_loc = mesh_->get_el_4_loc();
	el_ds = mesh_->get_el_ds();
}



int *Elasticity::get_row_4_el()
{
	return mesh_->get_row_4_el();
}











