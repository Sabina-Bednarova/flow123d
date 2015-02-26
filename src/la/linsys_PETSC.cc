/*!
 *
 * Copyright (C) 2007 Technical University of Liberec.  All rights reserved.
 *
 * Please make a following refer to Flow123d on your project site if you use the program for any purpose,
 * especially for academic research:
 * Flow123d, Research Centre: Advanced Remedial Technologies, Technical University of Liberec, Czech Republic
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License version 3 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program; if not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 021110-1307, USA.
 *
 *
 * $Id: la_linsys.hh 1299 2011-08-23 21:42:50Z jakub.sistek $
 * $Revision: 1299 $
 * $LastChangedBy: jakub.sistek $
 * $LastChangedDate: 2011-08-23 23:42:50 +0200 (Tue, 23 Aug 2011) $
 *
 * @file
 * @brief   Solver based on the original PETSc solver using MPIAIJ matrix and succesive Schur complement construction
 * @author  Jakub Sistek
 *
 *
 */

// derived from base linsys
#include "la/linsys_PETSC.hh"
#include "petscvec.h"
#include "petscksp.h"
#include "petscmat.h"
#include "system/sys_profiler.hh"


//#include <boost/bind.hpp>

namespace it = Input::Type;

it::Record LinSys_PETSC::input_type = it::Record("Petsc", "Solver setting.")
    .derive_from(LinSys::input_type)
    .declare_key("a_tol", it::Double(0.0), it::Default("1.0e-9"), "Absolute residual tolerance.")
    .declare_key("options", it::String(), it::Default(""),  "Options passed to PETSC before creating KSP instead of default setting.");


LinSys_PETSC::LinSys_PETSC( const Distribution * rows_ds)
        : LinSys( rows_ds ),
          init_guess_nonzero(false),
          matrix_(0)
{
    // create PETSC vectors:
    PetscErrorCode ierr;
    // rhs
    v_rhs_= new double[ rows_ds_->lsize() + 1 ];
    ierr = VecCreateMPIWithArray( comm_, 1, rows_ds_->lsize(), PETSC_DECIDE, v_rhs_, &rhs_ ); CHKERRV( ierr );
    ierr = VecZeroEntries( rhs_ ); CHKERRV( ierr );

    params_ = "";
    matrix_ = NULL;
    solution_precision_ = std::numeric_limits<double>::infinity();
    matrix_changed_ = true;
    rhs_changed_ = true;
}

LinSys_PETSC::LinSys_PETSC( LinSys_PETSC &other )
	: LinSys(other), params_(other.params_), v_rhs_(NULL), solution_precision_(other.solution_precision_)
{
	MatCopy(other.matrix_, matrix_, DIFFERENT_NONZERO_PATTERN);
	VecCopy(other.rhs_, rhs_);
	VecCopy(other.on_vec_, on_vec_);
	VecCopy(other.off_vec_, off_vec_);
}

void LinSys_PETSC::start_allocation( )
{
    PetscErrorCode ierr;

    ierr = VecCreateMPI( comm_, rows_ds_->lsize(), PETSC_DECIDE, &(on_vec_) ); CHKERRV( ierr ); 
    ierr = VecDuplicate( on_vec_, &(off_vec_) ); CHKERRV( ierr ); 
    status_ = ALLOCATE;
}

void LinSys_PETSC::start_add_assembly()
{
    switch ( status_ ) {
        case ALLOCATE:
            this->preallocate_matrix( );
            break;
        case INSERT:
            this->finish_assembly( MAT_FLUSH_ASSEMBLY );
            break;
        case ADD:
        case DONE:
            break;
        default:
            ASSERT( false, "Can not set values. Matrix is not preallocated.\n");
    }
    status_ = ADD;
}

void LinSys_PETSC::start_insert_assembly()
{
    switch ( status_ ) {
        case ALLOCATE:
            this->preallocate_matrix();
            break;
        case ADD:
            this->finish_assembly( MAT_FLUSH_ASSEMBLY );
            break;
        case INSERT:
        case DONE:
            break;
        default:
            ASSERT( false, "Can not set values. Matrix is not preallocated.\n");
    }
    status_ = INSERT;
}

void LinSys_PETSC::mat_set_values( int nrow, int *rows, int ncol, int *cols, double *vals )
{
    PetscErrorCode ierr;

    // here vals would need to be converted from double to PetscScalar if it was ever something else than double :-)
    switch (status_) {
        case INSERT:
        case ADD:
            ierr = MatSetValues(matrix_,nrow,rows,ncol,cols,vals,(InsertMode)status_); CHKERRV( ierr ); 
            break;
        case ALLOCATE:
            this->preallocate_values(nrow,rows,ncol,cols); 
            break;
        default: DBGMSG("LS SetValues with non allowed insert mode.\n");
    }

    matrix_changed_ = true;
}

void LinSys_PETSC::rhs_set_values( int nrow, int *rows, double *vals )
{
    PetscErrorCode ierr;

    switch (status_) {
        case INSERT:
        case ADD:
            ierr = VecSetValues(rhs_,nrow,rows,vals,(InsertMode)status_); CHKERRV( ierr ); 
            break;
        case ALLOCATE: 
            break;
        default: ASSERT(false, "LinSys's status disallow set values.\n");
    }

    rhs_changed_ = true;
}

void LinSys_PETSC::preallocate_values(int nrow,int *rows,int ncol,int *cols)
{
    int i,j;
    int col;
    PetscInt row;

    for (i=0; i<nrow; i++) {
        row=rows[i];
        for(j=0; j<ncol; j++) {
            col = cols[j];
            if (rows_ds_->get_proc(row) == rows_ds_->get_proc(col))
                VecSetValue(on_vec_,row,1.0,ADD_VALUES);
            else
                VecSetValue(off_vec_,row,1.0,ADD_VALUES);
        }
    }
}

void LinSys_PETSC::preallocate_matrix()
{
    ASSERT(status_ == ALLOCATE, "Linear system has to be in ALLOCATE status.");

    PetscErrorCode ierr;
    PetscInt *on_nz, *off_nz;
    PetscScalar *on_array, *off_array;

    // assembly and get values from counting vectors, destroy them
    VecAssemblyBegin(on_vec_);
    VecAssemblyBegin(off_vec_);

    on_nz  = new PetscInt[ rows_ds_->lsize() ];
    off_nz = new PetscInt[ rows_ds_->lsize() ];

    VecAssemblyEnd(on_vec_);
    VecAssemblyEnd(off_vec_);

    VecGetArray( on_vec_,  &on_array );
    VecGetArray( off_vec_, &off_array );

    for ( unsigned int i=0; i<rows_ds_->lsize(); i++ ) {
        on_nz[i]  = static_cast<PetscInt>( on_array[i]+0.1  );  // small fraction to ensure correct rounding
        off_nz[i] = static_cast<PetscInt>( off_array[i]+0.1 );
    }

    VecRestoreArray(on_vec_,&on_array);
    VecRestoreArray(off_vec_,&off_array);
    VecDestroy(&on_vec_);
    VecDestroy(&off_vec_);

    // create PETSC matrix with preallocation
    if (matrix_ != NULL)
    {
    	ierr = MatDestroy(&matrix_); CHKERRV( ierr );
    }
    ierr = MatCreateAIJ(PETSC_COMM_WORLD, rows_ds_->lsize(), rows_ds_->lsize(), PETSC_DETERMINE, PETSC_DETERMINE,
                           0, on_nz, 0, off_nz, &matrix_); CHKERRV( ierr );

    if (symmetric_) MatSetOption(matrix_, MAT_SYMMETRIC, PETSC_TRUE);
    MatSetOption(matrix_, MAT_NEW_NONZERO_ALLOCATION_ERR, PETSC_TRUE);

    delete[] on_nz;
    delete[] off_nz;
}

void LinSys_PETSC::finish_assembly( )
{
    MatAssemblyType assemblyType = MAT_FINAL_ASSEMBLY;
    this->finish_assembly( assemblyType );
}

void LinSys_PETSC::finish_assembly( MatAssemblyType assembly_type )
{
    PetscErrorCode ierr;

    if (status_ == ALLOCATE) {
        xprintf(Warn, "Finalizing linear system without setting values.\n");
        this->preallocate_matrix();
    }
    ierr = MatAssemblyBegin(matrix_, assembly_type); CHKERRV( ierr ); 
    ierr = VecAssemblyBegin(rhs_); CHKERRV( ierr ); 
    ierr = MatAssemblyEnd(matrix_, assembly_type); CHKERRV( ierr ); 
    ierr = VecAssemblyEnd(rhs_); CHKERRV( ierr ); 

    if (assembly_type == MAT_FINAL_ASSEMBLY) status_ = DONE;

    matrix_changed_ = true;
    rhs_changed_ = true;
}

void LinSys_PETSC::apply_constrains( double scalar )
{
    PetscErrorCode ierr;

    // check that system matrix is assembled
    ASSERT ( status_ == DONE, "System matrix and right-hand side are not assembled when applying constraints." );

    // number of constraints
    PetscInt numConstraints = static_cast<PetscInt>( constraints_.size() );

    // Additional multiplier for numerical reasons (criterion to be established)
    const PetscScalar diagScalar = static_cast<PetscScalar>( scalar );

    std::vector<PetscInt> globalDofs;
    std::vector<PetscScalar>  values;

    // Constraint iterators
    ConstraintVec_::const_iterator cIter = constraints_.begin( );
    ConstraintVec_::const_iterator cEnd  = constraints_.end( );
    // collect global dof indices and the correpsonding values
    for ( ; cIter != cEnd; ++cIter ) {
        globalDofs.push_back( static_cast<PetscInt>( cIter -> first ) );
        values.push_back( static_cast<PetscScalar>( cIter -> second ) * diagScalar );
    }

    // prepare pointers to be passed to PETSc
    PetscInt * globalDofPtr = this->makePetscPointer_( globalDofs );
    PetscScalar * valuePtr  = this->makePetscPointer_( values );

    // set matrix rows to zero 
    ierr = MatZeroRows( matrix_, numConstraints, globalDofPtr, diagScalar, PETSC_NULL, PETSC_NULL ); CHKERRV( ierr ); 
    matrix_changed_ = true;

    // set RHS entries to values (crashes if called with NULL pointers)
    if ( numConstraints ) {
        ierr = VecSetValues( rhs_, numConstraints, globalDofPtr, valuePtr, INSERT_VALUES ); CHKERRV( ierr ); 
        rhs_changed_ = true;
    }

    // perform communication in the rhs vector
    ierr = VecAssemblyBegin( rhs_ ); CHKERRV( ierr ); 
    ierr = VecAssemblyEnd(   rhs_ ); CHKERRV( ierr ); 
}


void LinSys_PETSC::set_initial_guess_nonzero(bool set_nonzero)
{
	init_guess_nonzero = set_nonzero;
}


int LinSys_PETSC::solve()
{
    KSP                system;
    KSPConvergedReason reason;

    const char *petsc_dflt_opt;
    int nits;
    
    // -mat_no_inode ... inodes are usefull only for
    //  vector problems e.g. MH without Schur complement reduction	
    if (rows_ds_->np() > 1) {
        // parallel setting
       if (this->is_positive_definite())
           petsc_dflt_opt="-ksp_type cg -ksp_diagonal_scale_fix -pc_type asm -pc_asm_overlap 4 -sub_pc_type icc -sub_pc_factor_levels 3  -sub_pc_factor_fill 6.0";
           //petsc_dflt_opt="-ksp_type bcgs -ksp_diagonal_scale_fix -pc_type asm -pc_asm_overlap 4 -sub_pc_type ilu -sub_pc_factor_levels 3  -sub_pc_factor_fill 6.0";
       else
           petsc_dflt_opt="-ksp_type bcgs -ksp_diagonal_scale_fix -pc_type asm -pc_asm_overlap 4 -sub_pc_type ilu -sub_pc_factor_levels 3 -sub_pc_factor_fill 6.0";
    
    } 
    else {
        // serial setting
       if (this->is_positive_definite())
           petsc_dflt_opt="-ksp_type cg -pc_type icc  -pc_factor_levels 3 -ksp_diagonal_scale_fix -pc_factor_fill 6.0";
       else
           petsc_dflt_opt="-ksp_type bcgs -pc_type ilu -pc_factor_levels 5 -ksp_diagonal_scale_fix -pc_factor_fill 6.0";
    }

    if (params_ == "") params_ = petsc_dflt_opt;
    xprintf(MsgLog,"inserting petsc options: %s\n",params_.c_str());
    PetscOptionsInsertString(params_.c_str()); // overwrites previous options values
    
    MatSetOption( matrix_, MAT_USE_INODES, PETSC_FALSE );
    
    KSPCreate( comm_, &system );
    KSPSetOperators(system, matrix_, matrix_, DIFFERENT_NONZERO_PATTERN);

    // TODO take care of tolerances - shall we support both input file and command line petsc setting
    KSPSetTolerances(system, r_tol_, a_tol_, PETSC_DEFAULT,PETSC_DEFAULT);
    KSPSetFromOptions(system);
    // We set the KSP flag set_initial_guess_nonzero
    // unless KSP type is preonly.
    // In such case PETSc fails (version 3.4.1)
    if (init_guess_nonzero)
    {
    	KSPType type;
    	KSPGetType(system, &type);
    	if (strcmp(type, KSPPREONLY) != 0)
    		KSPSetInitialGuessNonzero(system, PETSC_TRUE);
    }

    {
		START_TIMER("PETSC linear solver");
		START_TIMER("PETSC linear iteration");
		KSPSolve(system, rhs_, solution_ );
		KSPGetConvergedReason(system,&reason);
		KSPGetIterationNumber(system,&nits);
		ADD_CALLS(nits);
    }
    // substitute by PETSc call for residual
    VecNorm(rhs_, NORM_2, &residual_norm_);
    
    xprintf(MsgLog,"convergence reason %d, number of iterations is %d\n", reason, nits);

    // get residual norm
    KSPGetResidualNorm(system, &solution_precision_);

    // TODO: I do not understand this 
    //Profiler::instance()->set_timer_subframes("SOLVING MH SYSTEM", nits);

    KSPDestroy(&system);

    return static_cast<int>(reason);

}

void LinSys_PETSC::view( )
{
    std::string matFileName = "flow123d_matrix.m";
    std::string rhsFileName = "flow123d_rhs.m";
    std::string solFileName = "flow123d_sol.m";

    PetscViewer myViewer;

    PetscViewerASCIIOpen(comm_,matFileName.c_str(),&myViewer);
    PetscViewerSetFormat(myViewer,PETSC_VIEWER_ASCII_MATLAB);
    MatView( matrix_, myViewer );
    PetscViewerDestroy(&myViewer);

    PetscViewerASCIIOpen(comm_,rhsFileName.c_str(),&myViewer);
    PetscViewerSetFormat(myViewer,PETSC_VIEWER_ASCII_MATLAB);
    VecView( rhs_, myViewer );
    PetscViewerDestroy(&myViewer);

    if ( solution_ != NULL ) {
        PetscViewerASCIIOpen(comm_,solFileName.c_str(),&myViewer);
        PetscViewerSetFormat(myViewer,PETSC_VIEWER_ASCII_MATLAB);
        VecView( solution_, myViewer );
        PetscViewerDestroy(&myViewer);
    }
}

LinSys_PETSC::~LinSys_PETSC( )
{
    PetscErrorCode ierr;

    if (matrix_ != NULL) { ierr = MatDestroy(&matrix_); CHKERRV( ierr ); }
    ierr = VecDestroy(&rhs_); CHKERRV( ierr );

    if (v_rhs_ != NULL) delete[] v_rhs_;
}

void LinSys_PETSC::set_from_input(const Input::Record in_rec)
{
	if (! in_rec.is_empty()) {
		// common values
		LinSys::set_from_input( in_rec );

		// PETSc specific setting
		a_tol_  = in_rec.val<double>("a_tol");
		params_ = in_rec.val<string>("options");
	}
}


double LinSys_PETSC::get_solution_precision()
{
	return solution_precision_;
}

