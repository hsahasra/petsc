 
static char help[] = "Tests MPI parallel matrix creation. Test MatGetRedundantMatrix() \n\n";

#include <petscmat.h>

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **args)
{
  Mat            C;
  MatInfo        info;
  PetscMPIInt    rank,size;
  PetscInt       i,j,m = 3,n = 2,low,high,iglobal;
  PetscInt       Ii,J,ldim;
  PetscErrorCode ierr;
  PetscBool      flg_info,flg_mat;
  PetscScalar    v,one = 1.0;
  Vec            x,y;

  PetscInitialize(&argc,&args,(char*)0,help);
  ierr = PetscOptionsGetInt(NULL,"-m",&m,NULL);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(PETSC_COMM_WORLD,&rank);CHKERRQ(ierr);
  ierr = MPI_Comm_size(PETSC_COMM_WORLD,&size);CHKERRQ(ierr);
  n    = 2*size;

  ierr = MatCreate(PETSC_COMM_WORLD,&C);CHKERRQ(ierr);
  ierr = MatSetSizes(C,PETSC_DECIDE,PETSC_DECIDE,m*n,m*n);CHKERRQ(ierr);
  ierr = MatSetFromOptions(C);CHKERRQ(ierr);
  ierr = MatSetUp(C);CHKERRQ(ierr);

  /* Create the matrix for the five point stencil, YET AGAIN */
  for (i=0; i<m; i++) {
    for (j=2*rank; j<2*rank+2; j++) {
      v = -1.0;  Ii = j + n*i;
      if (i>0)   {J = Ii - n; ierr = MatSetValues(C,1,&Ii,1,&J,&v,INSERT_VALUES);CHKERRQ(ierr);}
      if (i<m-1) {J = Ii + n; ierr = MatSetValues(C,1,&Ii,1,&J,&v,INSERT_VALUES);CHKERRQ(ierr);}
      if (j>0)   {J = Ii - 1; ierr = MatSetValues(C,1,&Ii,1,&J,&v,INSERT_VALUES);CHKERRQ(ierr);}
      if (j<n-1) {J = Ii + 1; ierr = MatSetValues(C,1,&Ii,1,&J,&v,INSERT_VALUES);CHKERRQ(ierr);}
      v = 4.0; ierr = MatSetValues(C,1,&Ii,1,&Ii,&v,INSERT_VALUES);CHKERRQ(ierr);
    }
  }

  /* Add extra elements (to illustrate variants of MatGetInfo) */
  Ii   = n; J = n-2; v = 100.0;
  ierr = MatSetValues(C,1,&Ii,1,&J,&v,INSERT_VALUES);CHKERRQ(ierr);
  Ii   = n-2; J = n; v = 100.0;
  ierr = MatSetValues(C,1,&Ii,1,&J,&v,INSERT_VALUES);CHKERRQ(ierr);

  ierr = MatAssemblyBegin(C,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(C,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);

  /* Form vectors */
  ierr = MatGetVecs(C,&x,&y);CHKERRQ(ierr);
  ierr = VecGetLocalSize(x,&ldim);
  ierr = VecGetOwnershipRange(x,&low,&high);CHKERRQ(ierr);
  for (i=0; i<ldim; i++) {
    iglobal = i + low;
    v       = one*((PetscReal)i) + 100.0*rank;
    ierr    = VecSetValues(x,1,&iglobal,&v,INSERT_VALUES);CHKERRQ(ierr);
  }
  ierr = VecAssemblyBegin(x);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(x);CHKERRQ(ierr);

  ierr = MatMult(C,x,y);CHKERRQ(ierr);
  /*
  ierr = VecView(x,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
  ierr = VecView(y,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
   */
  ierr = PetscOptionsHasName(NULL,"-view_info",&flg_info);CHKERRQ(ierr);
  if (flg_info)  {
    ierr = PetscViewerSetFormat(PETSC_VIEWER_STDOUT_WORLD,PETSC_VIEWER_ASCII_INFO);CHKERRQ(ierr);
    ierr = MatView(C,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
  
    ierr = MatGetInfo(C,MAT_GLOBAL_SUM,&info);CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_WORLD,"matrix information (global sums):\n\
     nonzeros = %D, allocated nonzeros = %D\n",(PetscInt)info.nz_used,(PetscInt)info.nz_allocated);CHKERRQ(ierr);
    ierr = MatGetInfo (C,MAT_GLOBAL_MAX,&info);CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_WORLD,"matrix information (global max):\n\
     nonzeros = %D, allocated nonzeros = %D\n",(PetscInt)info.nz_used,(PetscInt)info.nz_allocated);CHKERRQ(ierr);
  }
  
  ierr = PetscOptionsHasName(NULL,"-view_mat",&flg_mat);CHKERRQ(ierr);
  if (flg_mat) {
    ierr = MatView(C,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
  }

  /* Test MatGetRedundantMatrix() */
  if (size > 1) {
    MPI_Comm       subcomm;
    Mat            Credundant;
    PetscMPIInt    nsubcomms=size,subsize;

    ierr = PetscOptionsGetInt(NULL,"-nsubcomms",&nsubcomms,NULL);CHKERRQ(ierr);
    if (nsubcomms > size) SETERRQ2(PETSC_COMM_SELF,PETSC_ERR_ARG_WRONG,"nsubcomms %d cannot > ncomms %d",nsubcomms,size);

    ierr = MatGetRedundantMatrix(C,nsubcomms,MPI_COMM_NULL,MAT_INITIAL_MATRIX,&Credundant);CHKERRQ(ierr);
    ierr = MatGetRedundantMatrix(C,nsubcomms,MPI_COMM_NULL,MAT_REUSE_MATRIX,&Credundant);CHKERRQ(ierr);

    ierr = PetscObjectGetComm((PetscObject)Credundant,&subcomm);CHKERRQ(ierr);
    ierr = MPI_Comm_size(subcomm,&subsize);CHKERRQ(ierr);
    
    if (subsize==1 && flg_mat) {
      printf("\n[%d] Credundant:\n",rank);
     ierr = MatView(Credundant,PETSC_VIEWER_STDOUT_SELF);CHKERRQ(ierr);
    }
    ierr = MatDestroy(&Credundant);CHKERRQ(ierr);
  }

  ierr = VecDestroy(&x);CHKERRQ(ierr);
  ierr = VecDestroy(&y);CHKERRQ(ierr);
  ierr = MatDestroy(&C);CHKERRQ(ierr);
  ierr = PetscFinalize();
  return 0;
}
