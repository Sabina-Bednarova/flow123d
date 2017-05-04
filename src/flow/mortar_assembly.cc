/*
 * mortar_assembly.cc
 *
 *  Created on: Feb 22, 2017
 *      Author: jb
 */

#include "flow/mortar_assembly.hh"
#include "quadrature/quadrature_lib.hh"
#include "quadrature/intersection_quadrature.hh"
#include "la/linsys.hh"
//#include "mesh/intersection.hh"
#include "intersection/mixed_mesh_intersections.hh"
#define ARMA_USE_CXX11
#include <armadillo>

P0_CouplingAssembler::P0_CouplingAssembler(AssemblyDataPtr data)
: MortarAssemblyBase(data),
  tensor_average_(16),
  col_average_(4),
  quadrature_(*(data->mesh)),
  slave_ac_(data->mh_dh)
{
    isec_data_list.reserve(30);


    for(uint row_dim=0; row_dim<4; row_dim ++)
        for(uint col_dim=0; col_dim<4; col_dim ++) {
            arma::vec row_avg = arma::vec(row_dim+1);
            row_avg.fill(1.0 / (row_dim+1));
            arma::vec col_avg = arma::vec(col_dim+1);
            col_avg.fill(1.0 / (col_dim+1));
            arma::mat avg = row_avg * col_avg.t(); // tensor product
            tensor_average(row_dim, col_dim) = avg;
            col_average_[row_dim] = row_avg;
        }
}


void P0_CouplingAssembler::pressure_diff(LocalElementAccessorBase<3> ele_ac, double delta) {
    isec_data_list.push_back(IsecData());
    IsecData &i_data = isec_data_list.back();

    i_data.dim= ele_ac.dim();
    i_data.delta = delta;
    i_data.dofs.resize(ele_ac.n_sides());
    i_data.vel_dofs.resize(ele_ac.n_sides());
    i_data.dirichlet_dofs.resize(ele_ac.n_sides());
    i_data.dirichlet_sol.resize(ele_ac.n_sides());
    i_data.n_dirichlet=0;
    i_data.ele_z_coord_=ele_ac.centre()[2];

    for(unsigned int i_side=0; i_side < ele_ac.n_sides(); i_side++ ) {
        i_data.dofs[i_side]=ele_ac.edge_row(i_side);
        i_data.vel_dofs[i_side] = ele_ac.side_row(i_side);
        //i_data.z_sides[i_side]=ele_ac.side(i_side)->centre()[2];
        //DebugOut().fmt("edge: {} {}", i_side, ele_ac.edge_row(i_side));
        Boundary * bcd = ele_ac.full_iter()->side(i_side)->cond();
        if (bcd) {
            ElementAccessor<3> b_ele = bcd->element_accessor();
            auto type = (DarcyMH::EqData::BC_Type)data_->bc_type.value(b_ele.centre(), b_ele);
            //DebugOut().fmt("bcd id: {} sidx: {} type: {}\n", ele->id(), i_side, type);
            if (type == DarcyMH::EqData::dirichlet) {
                //DebugOut().fmt("Dirichlet: {}\n", ele->index());
                double bc_pressure = data_->bc_pressure.value(b_ele.centre(), b_ele);
                i_data.dirichlet_dofs[i_data.n_dirichlet] = i_side;
                i_data.dirichlet_sol[i_data.n_dirichlet] = bc_pressure;
                i_data.n_dirichlet++;
            }
        }
    }

}





/**
 * Works well but there is large error next to the boundary.
 */
void P0_CouplingAssembler::assembly(LocalElementAccessorBase<3> master_ac)
{


    // on the intersection element we consider
    // * intersection dofs for master and slave
    //   those are dofs of the space into which we interpolate
    //   base functions from individual master and slave elements
    //   For the master dofs both are usualy eqivalent.
    // * original dofs - for master same as intersection dofs, for slave
    //   all dofs of slave elements

    // form list of intersection dofs, in this case pressures in barycenters
    // but we do not use those form MH system in order to allow second schur somplement. We rather map these
    // dofs to pressure traces, i.e. we use averages of traces as barycentric values.

    /**
     *  TODO:
     *  - pass through the master and all slaves and collect global dofs , bcd, solution.
     *    I.e. call Nx pressure_diff not NxNx.
     *
     *  - Is it safe to have duplicate rows in local_system?
     *  - Is it better to have more smaller local system then single big one?
     *  - use one big or more smaller local systems to set.
     */


    if (master_ac.dim() > 2) return; // supported only for 1D and 2D master elements
    auto &isec_list = mixed_mesh_.element_intersections_[master_ac.ele_global_idx()];
    if (isec_list.size() == 0) return; // skip empty masters

    //slave_ac_.setup(master_ac);

    ElementFullIter ele = master_ac.full_iter();
    arma::vec3 ele_centre = ele->centre();
    double m_sigma = data_->sigma.value( ele_centre, ele->element_accessor());
    double m_conductivity = data_->conductivity.value( ele_centre, ele->element_accessor());
    double m_crossection = data_->cross_section.value( ele_centre, ele->element_accessor() );

    double master_sigma = 2*m_sigma*m_conductivity *
                    2/ m_crossection;
                    /**
                     * ?? How to deal with anisotropy ??
                     * 3d-2d : compute nv of 2d triangle
                     * 2d-2d : interpret as 2d-1d-2d, should be symmetric master-slave
                     * 2d-1d : nv is tangent to 2d and normal to 1d
                    arma::dot(data_->anisotropy.value( ele_centre, ele->element_accessor())*nv, nv)
                    */







    isec_data_list.clear();
    double cs_sqr_avg = 0.0;
    double isec_sum = 0.0;
    uint i = 0;
    for(; i < isec_list.size(); ++i) {
        bool non_zero = quadrature_.reinit(isec_list[i].second);
        slave_ac_.reinit( quadrature_.slave_idx() );
        if (slave_ac_.dim() == master_ac.dim()) break;
        if (! non_zero) continue; // skip quadratures close to zero

        double cs = data_->cross_section.value(slave_ac_.full_iter()->centre(), slave_ac_.full_iter()->element_accessor());
        double isec_measure = quadrature_.measure();
        //DebugOut() << print_var(cs) << print_var(isec_measure);
        cs_sqr_avg += cs*cs*isec_measure;
        isec_sum += isec_measure;
        //DebugOut().fmt("Assembly23: {} {} {} ", ele->id(), slave_ac_.full_iter()->id(), isec_measure);
        pressure_diff(slave_ac_, isec_measure);
    }
    if ( ! (slave_ac_.dim() == 2 && master_ac.dim() ==2 ) ) {
        if( fabs(isec_sum - ele->measure()) > 1E-5) {
            string out;
            for(auto & isec : isec_list) {
                slave_ac_.reinit(isec.second->bulk_ele_idx());
                out += fmt::format(" {}", slave_ac_.full_iter()->id());
            }

            double diff = (isec_sum - ele->measure())/ele->measure();
            WarningOut() << print_var(diff)
                    << print_var(ele->id())
                    << endl
                    << out;

        }
    }
    pressure_diff(master_ac, -isec_sum);

    //DebugOut().fmt( "cs2: {} d0: {}", cs_sqr_avg, delta_0);
    master_sigma = master_sigma * (cs_sqr_avg / isec_sum)
            / isec_sum;


    add_to_linsys(master_sigma);


    // 2d-2d
    //DebugOut() << "2d-2d";
    if (i < isec_list.size()) {
        isec_data_list.clear();
        isec_sum = 0.0;
        for(; i < isec_list.size(); ++i) {
                quadrature_.reinit(isec_list[i].second);
                slave_ac_.reinit( quadrature_.slave_idx() );
                double isec_measure = quadrature_.measure();
                isec_sum += isec_measure;
                //DebugOut().fmt("Assembly22: {} {} {}", ele->id(), slave_ac_.full_iter()->id(), isec_measure);
                pressure_diff(slave_ac_, isec_measure);
        }
        pressure_diff(master_ac, -isec_sum);

        master_sigma = 100 * m_conductivity/ m_crossection / isec_sum;

        add_to_linsys(master_sigma);
    }
}

void P0_CouplingAssembler::add_to_linsys(double scale)
{
    // rows
     for(IsecData &row_ele : isec_data_list) {
         //columns
         for(IsecData &col_ele : isec_data_list) {


             double s =  -scale * row_ele.delta * col_ele.delta;
             //DebugOut().fmt("s: {} {} {} {}", s, scale, row_ele.delta, col_ele.delta);
             product_ = s * tensor_average(row_ele.dim, col_ele.dim);

             loc_system_.reset(row_ele.dofs, col_ele.dofs);

             for(uint i=0; i< row_ele.n_dirichlet; i++)
                 loc_system_.set_solution_row(row_ele.dirichlet_dofs[i], row_ele.dirichlet_sol[i], -1.0);
             for(uint i=0; i< col_ele.n_dirichlet; i++) loc_system_.set_solution_col(col_ele.dirichlet_dofs[i], col_ele.dirichlet_sol[i]);
             //ASSERT( arma::norm(product_,2) == 0.0 );
             loc_system_.set_matrix(product_);
             // Must have z-coords for every side, can not use averaging vector
             loc_system_.set_rhs( -s * col_average_[row_ele.dim] * col_ele.ele_z_coord_ );
             loc_system_.eliminate_solution();

             if (fix_velocity_flag) {
                 this->fix_velocity_local(row_ele, col_ele);
             } else {
                 data_->lin_sys->set_local_system(loc_system_);
             }
         }
     }
}


void P0_CouplingAssembler::fix_velocity_local(const IsecData &row_ele, const IsecData & col_ele)
{

    uint n_rows = row_ele.vel_dofs.n_rows;
    uint n_cols = col_ele.dofs.n_rows;
    arma::vec pressure(n_cols);
    arma::vec add_velocity(n_rows);
    double * solution = data_->lin_sys->get_solution_array();
    for(uint icol=0; icol < n_cols; icol++ ) pressure[icol] = solution[col_ele.dofs[icol]];
    add_velocity =  loc_system_.get_matrix() * pressure - loc_system_.get_rhs();
    //DebugOut() << "fix_velocity\n" << pressure << add_velocity;
    for(uint irow=0; irow < n_rows; irow++ ) solution[row_ele.vel_dofs[irow]] += add_velocity[irow] ;
}












P1_CouplingAssembler::P1_CouplingAssembler(AssemblyDataPtr data)
: MortarAssemblyBase(data),
  slave_ac_(data->mh_dh),
  master_ac_(data->mh_dh)
{
    /// TODO: Use RefElement numberings to make it independent.
    P1_for_P1e[0] = arma::mat({{1}});
    P1_for_P1e[1] = arma::eye(2,2);

    //{{1, 1, -1}, {1, -1, 1}, {-1, 1, 1}}
    P1_for_P1e[2] = arma::ones(3,3);
    P1_for_P1e[2].at(0,2)=-1;
    P1_for_P1e[2].at(1,1)=-1;
    P1_for_P1e[2].at(2,0)=-1;

    //{{1, 1, 1, -1}, {1, 1, -1, 1}, {1, -1, 1, 1}, {-1, 1, 1, 1}}
    P1_for_P1e[3] = arma::ones(4,4);
    P1_for_P1e[3].at(0,3)=-2;
    P1_for_P1e[3].at(1,2)=-2;
    P1_for_P1e[3].at(2,1)=-2;
    P1_for_P1e[3].at(3,0)=-2;

}


void P1_CouplingAssembler::add_sides(LocalElementAccessorBase<3> ele_ac, unsigned int shift, arma::uvec &dofs)
{
    for(unsigned int i_side=0; i_side < ele_ac.n_sides(); i_side++ ) {
        uint loc_dof = shift+i_side;
        dofs[loc_dof] =  ele_ac.edge_row(i_side);
        Boundary * bcd = ele_ac.full_iter()->side(i_side)->cond();

        if (bcd) {
            ElementAccessor<3> b_ele = bcd->element_accessor();
            auto type = (DarcyMH::EqData::BC_Type)data_->bc_type.value(b_ele.centre(), b_ele);
            //DebugOut().fmt("bcd id: {} sidx: {} type: {}\n", ele->id(), i_side, type);
            if (type == DarcyMH::EqData::dirichlet) {
                double bc_pressure = data_->bc_pressure.value(b_ele.centre(), b_ele);
                loc_system_.set_solution(loc_dof, bc_pressure, -1.0);
            }
        }
    }
}


template<uint qdim, uint master_dim, uint slave_dim>
void P1_CouplingAssembler::isec_assembly(
        double master_sigma,
        const IntersectionLocal<master_dim, slave_dim> &il,
        std::array<uint, qdim+1 > subdiv)
{
    // mappings from quadrature ref. el (bary) to intersection ref. elements (local coords)
    arma::mat master_map(master_dim, qdim + 1);
    arma::mat slave_map(slave_dim, qdim+1);
    for(uint i_col=0; i_col < qdim+1; i_col++) {
        uint ip = subdiv[i_col];
        master_map.col(i_col) = il[ip].comp_coords();
        slave_map.col(i_col) = il[ip].bulk_coords();
    }

    uint n_dofs=master_ac_.n_sides() + slave_ac_.n_sides();
    arma::uvec dofs(n_dofs);
    loc_system_.reset(n_dofs, n_dofs);
    add_sides(master_ac_, 0, dofs);
    add_sides(slave_ac_, master_ac_.n_sides(), dofs);
    loc_system_.set_dofs(dofs, dofs);

    QGauss<qdim> q(2);
    arma::vec values(n_dofs);

    for(uint ip=0; ip < q.size(); ip++) {
        arma::vec bary_qp =  RefElement<qdim>::local_to_bary(q.point(ip));

        arma::vec qp_master = RefElement<master_dim>::local_to_bary(master_map * bary_qp);
        arma::vec qp_slave = RefElement<slave_dim>::local_to_bary(slave_map * bary_qp);
        double JxW = q.weight(ip) * il.compute_measure() * master_jac;
        uint shift = 0;
        values.rows(shift, shift + master_dim) = P1_for_P1e[master_dim] * qp_master;
        shift=master_dim + 1;
        values.rows(shift, shift + slave_dim ) = -P1_for_P1e[slave_dim] * qp_slave;

        double scale = -master_sigma * JxW;
        arma::mat x = scale * ( values * values.t() );

        //DebugOut() << print_var(scale);
        //DebugOut() << print_var(values);
        //DebugOut() << x;
        loc_system_.get_matrix() += x;

    }

    loc_system_.eliminate_solution();
    if (fix_velocity_flag) {
        //this->fix_velocity_local(row_ele, col_ele);
    } else {
        data_->lin_sys->set_local_system(loc_system_);
    }


}



/**
 * P1 connection of different dimensions
 *
 * - 20.11. 2014 - very poor convergence, big error in pressure even at internal part of the fracture
 */

void P1_CouplingAssembler::assembly(LocalElementAccessorBase<3> master_ac) {

    if (master_ac.dim() > 2) return; // supported only for 1D and 2D master elements
    auto &isec_list = mixed_mesh_.element_intersections_[master_ac.ele_global_idx()];
    if (isec_list.size() == 0) return; // skip empty masters

    //slave_ac_.setup(master_ac);

    ElementFullIter master_ele = master_ac.full_iter();
    master_ac_.reinit(master_ac.ele_global_idx());
    arma::vec3 ele_centre = master_ele->centre();
    double m_sigma = data_->sigma.value( ele_centre, master_ele->element_accessor());
    double m_conductivity = data_->conductivity.value( ele_centre, master_ele->element_accessor());
    double m_crossection = data_->cross_section.value( ele_centre, master_ele->element_accessor() );

    double master_sigma = 2*m_sigma*m_conductivity *
                    2/ m_crossection;
                    /**
                     * ?? How to deal with anisotropy ??
                     * 3d-2d : compute nv of 2d triangle
                     * 2d-2d : interpret as 2d-1d-2d, should be symmetric master-slave
                     * 2d-1d : nv is tangent to 2d and normal to 1d
                    arma::dot(data_->anisotropy.value( ele_centre, ele->element_accessor())*nv, nv)
                    */

    master_jac = master_ele->measure() * master_ele->dim();

    for(auto &isec_pair : isec_list) {
        IntersectionLocalBase *isec = isec_pair.second;
        ASSERT_EQ_DBG(master_ac.ele_global_idx(), isec->component_ele_idx());

        //ElementFullIter slave_ele = data_->mesh->element(isec->bulk_ele_idx());
        slave_ac_.reinit(isec->bulk_ele_idx());


        if (typeid(*isec) == typeid(IntersectionLocal<1,2>)) {
            auto il = static_cast<const IntersectionLocal<1,2> *>(isec);
            ASSERT_EQ_DBG( il->size(), 2);
            DebugOut() << "il12";

            this->isec_assembly<1,1,2>(master_sigma, *il, {0,1});

        } else
        if (typeid(*isec) == typeid(IntersectionLocal<2,2>)) {
            auto il = static_cast<const IntersectionLocal<2,2> *>(isec);
            ASSERT_EQ_DBG( il->size(), 2);
            DebugOut() << "il22";

            this->isec_assembly<1,2,2>(master_sigma, *il, {0,1});

        } else if (typeid(*isec) == typeid(IntersectionLocal<2,3>)) {
            auto il = static_cast<const IntersectionLocal<2,3> *>(isec);
            if (il->size() <= 2) continue; // skip degenerated intersections
            const uint master_dim = 2;
            const uint slave_dim = 3;
            const uint qdim = 2;

            uint n_dofs=master_ac_.n_sides() + slave_ac_.n_sides();
            arma::uvec dofs(n_dofs);
            loc_system_.reset(n_dofs, n_dofs);
            add_sides(master_ac_, 0, dofs);
            add_sides(slave_ac_, master_ac_.n_sides(), dofs);
            loc_system_.set_dofs(dofs, dofs);

            double isec_area = 0;
            double isec_measure = il->compute_measure();

            //DebugOut() << "subdivision, n: " << il->size();

            for(uint i_vtx=2; i_vtx< il->size();  i_vtx++) {
                //this->isec_assembly<2,2,3>(master_sigma, *il, {0, i_vtx1, i_vtx1+1});
                uint subdiv[3] = {0, i_vtx-1, i_vtx};

                // mappings from quadrature ref. el (bary) to intersection ref. elements (local coords)
                arma::mat master_map(master_dim, qdim + 1);
                arma::mat slave_map(slave_dim, qdim+1);
                for(uint i_col=0; i_col < qdim+1; i_col++) {
                    uint ip = subdiv[i_col];
                    master_map.col(i_col) = (*il)[ip].comp_coords();
                    slave_map.col(i_col) = (*il)[ip].bulk_coords();
                }

                double m_jac =
                    master_map.at(0,0)*(master_map.at(1,1) - master_map.at(1,2)) +
                    master_map.at(0,1)*(master_map.at(1,2) - master_map.at(1,0)) +
                    master_map.at(0,2)*(master_map.at(1,0) - master_map.at(1,1));

                double master_map_jac = arma::det( master_map.cols(1,2) - master_map.col(0) * arma::ones(1,2) );
                //DebugOut().fmt("jac1: {} jac2: {}", m_jac, master_map_jac);

                QGauss<qdim> q(2);
                arma::vec values(n_dofs);

                double q_area = 0;
                for(uint ip=0; ip < q.size(); ip++) {
                    arma::vec bary_qp =  RefElement<qdim>::local_to_bary(q.point(ip));

                    arma::vec qp_master = RefElement<master_dim>::local_to_bary(master_map * bary_qp);
                    arma::vec qp_slave = RefElement<slave_dim>::local_to_bary(slave_map * bary_qp);
                    double JxW = q.weight(ip) * isec_measure * master_jac;
                    uint shift = 0;
                    values.rows(shift, shift + master_dim) = P1_for_P1e[master_dim] * qp_master;
                    shift=master_dim + 1;
                    values.rows(shift, shift + slave_dim ) = -P1_for_P1e[slave_dim] * qp_slave;

                    double scale = -master_sigma * JxW;
                    arma::mat x = scale * ( values * values.t() );

                    //DebugOut() << print_var(scale);
                    //DebugOut() << print_var(values);
                    //DebugOut().fmt("2d one: {} 3d one: {}", arma::sum(values.rows(0,2)), arma::sum(values.rows(3,6)) );
                    //DebugOut() << x;
                    loc_system_.get_matrix() += x;

                    isec_area += q.weight(ip) * master_map_jac;
                    q_area += q.weight(ip);
                }


                //DebugOut().fmt("isub: {} isec m: {} a: {} qa: {} jac: {}\n",
                //        i_vtx, isec_measure, isec_area, q_area, master_map_jac);

            }

            loc_system_.eliminate_solution();
            if (fix_velocity_flag) {
                //this->fix_velocity_local(row_ele, col_ele);
            } else {
                data_->lin_sys->set_local_system(loc_system_);
            }

        } else {
            ASSERT(false).error("Impossible case.");
        }


    }
}




