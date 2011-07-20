#ifndef TRANSPORT_OPERATOR_SPLITTING_HH_
#define TRANSPORT_OPERATOR_SPLITTING_HH_

#include "equation.hh"
#include "./reaction/linear_reaction.hh"
#include "./semchem/semchem_interface.hh"


/// external types:
//class LinSys;
//struct Solver;
class Mesh;
//class SchurComplement;
//class Distribution;
//class SparseGraph;
class ConvectionTransport;
class MaterialDatabase;


/**
 * @brief Mixed-hybrid model of linear Darcy flow, possibly unsteady.
 *
 * Abstract class for various implementations of Darcy flow. In future there should be
 * one further level of abstraction for general time dependent problem.
 *
 * maybe TODO:
 * split compute_one_step to :
 * 1) prepare_next_timestep
 * 2) actualize_solution - this is for iterative nonlinear solvers
 *
 * make interface of DarcyFlowMH a general interface of time depenedent model. ....
 */

class TransportOperatorSplitting : public EquationBase {
public:
	TransportOperatorSplitting(MaterialDatabase *material_database, Mesh *init_mesh);
/*	void ReadFlowFieldVector(Vec *vec);*/
	virtual void compute_one_step();
	//virtual void compute_until();

//	~TransportOperatorSplitting();
	 virtual void get_parallel_solution_vector(Vec &vc);
	 virtual void get_solution_vector(double* &vector, unsigned int &size);
	 void compute_until_save_time();
	 TimeGovernor *time;
protected:

private:

    ConvectionTransport *convection;
    Linear_reaction *decayRad;
    Semchem_interface *Semchem_reactions;
   // Mesh *mesh;
   // MaterialDatabase *mat_base;
   // TimeGovernor *time;
 //   Chemistry *chemistry;
};

#endif // TRANSPORT_OPERATOR_SPLITTING_HH_
