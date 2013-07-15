
#define DEBUG

#define TEST_USE_PETSC

#include <gtest_mpi.hh>

#include <la/schur.hh>
#include <la/linsys.hh>

#include <petscmat.h>


TEST(la, inversion_matrix) {
	PetscInt size = 19;
	PetscInt rows [] = {5,2,3,3,4,2};
	//PetscInt part [] = {0,10,19};
	IS set;

	// vytvorit rozdeleni bloku na procesory ve tvaru "part" (tj. indexy prvnich radku na procesorech)
    MPI_Comm_rank(PETSC_COMM_WORLD, &rank);


    // volat s lokalni velkosti = pocet radku na lokalnim proc.
	LinSys * lin_sys = new LinSys_MPIAIJ(size);
	lin_sys->set_symmetric();
	lin_sys->start_allocation();

	// preallocate matrix - vytvorit funkci pro asemblaci, tu volat dvakrat jednou pro alokaci a jednou pro plneni
	unsigned int min = 0;
	for (unsigned int i = 0; i < 6; i++) { // cyklus jen pres lokalni bloky
		std::vector<PetscScalar> vals(rows[i] * rows[i], rows[i]);
		std::vector<int> rows_idx;
		rows_idx.reserve(rows[i]);
		for (unsigned int j=0; j<rows[i]; j++) {
			rows_idx.push_back(min + j);
		}
		int * insert_rows = &rows_idx[0];
		PetscScalar * insert_vals = &vals[0];
		lin_sys->mat_set_values(rows[i], insert_rows, rows[i], insert_rows, insert_vals);
		min += rows[i];
	}
	lin_sys->start_add_assembly();

	// fill matrix
	min = 0;
	srand(time(NULL));
	for (unsigned int i = 0; i < 6; i++) {
		std::vector<PetscScalar> vals;
		vals.reserve(rows[i] * rows[i]);
		for (unsigned int j=0; j<rows[i]*rows[i]; j++) {
			vals.push_back( rand()%19 + 1 );
		}
		std::vector<int> rows_idx;
		rows_idx.reserve(rows[i]);
		for (unsigned int j=0; j<rows[i]; j++) {
			rows_idx.push_back(min + j);
		}
		int * insert_rows = &rows_idx[0];
		PetscScalar * insert_vals = &vals[0];
		lin_sys->mat_set_values(rows[i], insert_rows, rows[i], insert_rows, insert_vals);
		min += rows[i];
	}
	lin_sys->finalize();
	//MatView(lin_sys->get_matrix(),PETSC_VIEWER_STDOUT_SELF);

	ISCreateStride(PETSC_COMM_SELF, 10, 0, 1, &set); // kazdy proc. lokalni cast indexsetu viz. schur.cc line 386
	//ISView(set, PETSC_VIEWER_STDOUT_SELF);

	SchurComplement schurComplement(lin_sys, set, 5);

}
