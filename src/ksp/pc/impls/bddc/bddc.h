#if !defined(__pcbddc_h)
#define __pcbddc_h

#include <../src/ksp/pc/impls/is/pcis.h>
#include "bddcstructs.h"

//typedef enum {SCATTERS_BDDC,GATHERS_BDDC} CoarseCommunicationsType;

/* Private context (data structure) for the BDDC preconditioner.  */
typedef struct {
  /* First MUST come the folowing line, for the stuff that is common to FETI and Neumann-Neumann. */
  PC_IS         pcis;
  /* Coarse stuffs needed by BDDC application in KSP */
  Vec           coarse_vec;
  Vec           coarse_rhs;
  KSP           coarse_ksp;
  Mat           coarse_phi_B;
  Mat           coarse_phi_D;
  Mat           coarse_psi_B;
  Mat           coarse_psi_D;
  PetscInt      local_primal_size;
  VecScatter    coarse_loc_to_glob;
  /* Local stuffs needed by BDDC application in KSP */
  Vec           vec1_P;
  Vec           vec1_C;
  Mat           local_auxmat1;
  Mat           local_auxmat2;
  Vec           vec1_R;
  Vec           vec2_R;
  IS            is_R_local;
  VecScatter    R_to_B;
  VecScatter    R_to_D;
  KSP           ksp_R;
  KSP           ksp_D;
  Vec           vec4_D;
  /* Quantities defining constraining details (local) of the preconditioner */
  /* These quantities define the preconditioner itself */
  PetscInt      n_constraints;
  PetscInt      n_vertices;
  Mat           ConstraintMatrix;
  PetscBool     use_change_of_basis;
  PetscBool     use_change_on_faces;
  Mat           ChangeOfBasisMatrix;
  Vec           original_rhs;
  Vec           temp_solution;
  Mat           local_mat;
  PetscBool     use_exact_dirichlet_trick;
  /* Some defaults on selecting vertices and constraints*/
  PetscBool     use_vertices;
  PetscBool     use_faces;
  PetscBool     use_edges;
  /* Some customization is possible */
  PCBDDCGraph                mat_graph;
  MatNullSpace               NullSpace;
  IS                         user_primal_vertices;
  PetscBool                  use_nnsp_true;
  PetscInt                   n_ISForDofs;
  IS                         *ISForDofs;
  IS                         NeumannBoundaries;
  IS                         DirichletBoundaries;
  PetscBool                  switch_static;
  PetscInt                   coarsening_ratio;
  PetscInt                   current_level;
  PetscInt                   max_levels;
  /* scaling */
  Vec                        work_scaling;
  PetscBool                  use_deluxe_scaling;
  PCBDDCDeluxeScaling        deluxe_ctx;
  /* For verbose output of some bddc data structures */
  PetscInt                   dbg_flag;
  PetscViewer                dbg_viewer;
} PC_BDDC;


#endif /* __pcbddc_h */
