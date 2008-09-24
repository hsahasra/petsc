#define PETSCMAT_DLL

/*  -------------------------------------------------------------------- 

     This file implements a subclass of the SeqAIJ matrix class that uses
     the SuperLU 3.0 sparse solver. You can use this as a starting point for 
     implementing your own subclass of a PETSc matrix class.

     This demonstrates a way to make an implementation inheritence of a PETSc
     matrix type. This means constructing a new matrix type (SuperLU) by changing some
     of the methods of the previous type (SeqAIJ), adding additional data, and possibly
     additional method. (See any book on object oriented programming).
*/

/*
     Defines the data structure for the base matrix type (SeqAIJ)
*/
#include "src/mat/impls/aij/seq/aij.h"

/*
     SuperLU include files
*/
EXTERN_C_BEGIN
#if defined(PETSC_USE_COMPLEX)
#include "slu_zdefs.h"
#else
#include "slu_ddefs.h"
#endif  
#include "slu_util.h"
EXTERN_C_END

/*
     This is the data we are "ADDING" to the SeqAIJ matrix type to get the SuperLU matrix type
*/
typedef struct {
  SuperMatrix       A,L,U,B,X;
  superlu_options_t options;
  PetscInt          *perm_c; /* column permutation vector */
  PetscInt          *perm_r; /* row permutations from partial pivoting */
  PetscInt          *etree;
  PetscReal         *R, *C;
  char              equed[1];
  PetscInt          lwork;
  void              *work;
  PetscReal         rpg, rcond;
  mem_usage_t       mem_usage;
  MatStructure      flg;

  /* Flag to clean up (non-global) SuperLU objects during Destroy */
  PetscTruth CleanUpSuperLU;
} Mat_SuperLU;

extern PetscErrorCode MatFactorInfo_SuperLU(Mat,PetscViewer);
extern PetscErrorCode MatLUFactorNumeric_SuperLU(Mat,Mat,MatFactorInfo *);
extern PetscErrorCode MatDestroy_SuperLU(Mat);
extern PetscErrorCode MatView_SuperLU(Mat,PetscViewer);
extern PetscErrorCode MatAssemblyEnd_SuperLU(Mat,MatAssemblyType);
extern PetscErrorCode MatSolve_SuperLU(Mat,Vec,Vec);
extern PetscErrorCode MatSolveTranspose_SuperLU(Mat,Vec,Vec);
extern PetscErrorCode MatLUFactorSymbolic_SuperLU(Mat,Mat,IS,IS,MatFactorInfo*);
extern PetscErrorCode MatDuplicate_SuperLU(Mat, MatDuplicateOption, Mat *);

/*
    Utility function
*/
#undef __FUNCT__  
#define __FUNCT__ "MatFactorInfo_SuperLU"
PetscErrorCode MatFactorInfo_SuperLU(Mat A,PetscViewer viewer)
{
  Mat_SuperLU       *lu= (Mat_SuperLU*)A->spptr;
  PetscErrorCode    ierr;
  superlu_options_t options;

  PetscFunctionBegin;
  /* check if matrix is superlu_dist type */
  if (A->ops->solve != MatSolve_SuperLU) PetscFunctionReturn(0);

  options = lu->options;
  ierr = PetscViewerASCIIPrintf(viewer,"SuperLU run parameters:\n");CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer,"  Equil: %s\n",(options.Equil != NO) ? "YES": "NO");CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer,"  ColPerm: %D\n",options.ColPerm);CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer,"  IterRefine: %D\n",options.IterRefine);CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer,"  SymmetricMode: %s\n",(options.SymmetricMode != NO) ? "YES": "NO");CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer,"  DiagPivotThresh: %g\n",options.DiagPivotThresh);CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer,"  PivotGrowth: %s\n",(options.PivotGrowth != NO) ? "YES": "NO");CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer,"  ConditionNumber: %s\n",(options.ConditionNumber != NO) ? "YES": "NO");CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer,"  RowPerm: %D\n",options.RowPerm);CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer,"  ReplaceTinyPivot: %s\n",(options.ReplaceTinyPivot != NO) ? "YES": "NO");CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer,"  PrintStat: %s\n",(options.PrintStat != NO) ? "YES": "NO");CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer,"  lwork: %D\n",lu->lwork);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}

/*
    These are the methods provided to REPLACE the corresponding methods of the 
   SeqAIJ matrix class. Other methods of SeqAIJ are not replaced
*/
#undef __FUNCT__  
#define __FUNCT__ "MatLUFactorNumeric_SuperLU"
PetscErrorCode MatLUFactorNumeric_SuperLU(Mat F,Mat A,MatFactorInfo *info)
{
  Mat_SeqAIJ     *aa = (Mat_SeqAIJ*)(A)->data;
  Mat_SuperLU    *lu = (Mat_SuperLU*)(F)->spptr;
  PetscErrorCode ierr;
  PetscInt       sinfo;
  SuperLUStat_t  stat;
  PetscReal      ferr, berr; 
  NCformat       *Ustore;
  SCformat       *Lstore;
  
  PetscFunctionBegin;
  if (lu->flg == SAME_NONZERO_PATTERN){ /* successing numerical factorization */
    lu->options.Fact = SamePattern;
    /* Ref: ~SuperLU_3.0/EXAMPLE/dlinsolx2.c */
    Destroy_SuperMatrix_Store(&lu->A); 
    if ( lu->lwork >= 0 ) { 
      Destroy_SuperNode_Matrix(&lu->L);
      Destroy_CompCol_Matrix(&lu->U);
      lu->options.Fact = SamePattern;
    }
  }

  /* Create the SuperMatrix for lu->A=A^T:
       Since SuperLU likes column-oriented matrices,we pass it the transpose,
       and then solve A^T X = B in MatSolve(). */
#if defined(PETSC_USE_COMPLEX)
  zCreate_CompCol_Matrix(&lu->A,A->cmap->n,A->rmap->n,aa->nz,(doublecomplex*)aa->a,aa->j,aa->i,
                           SLU_NC,SLU_Z,SLU_GE);
#else
  dCreate_CompCol_Matrix(&lu->A,A->cmap->n,A->rmap->n,aa->nz,aa->a,aa->j,aa->i,
                           SLU_NC,SLU_D,SLU_GE);
#endif
  
  /* Initialize the statistics variables. */
  StatInit(&stat);

  /* Numerical factorization */
  lu->B.ncol = 0;  /* Indicate not to solve the system */
#if defined(PETSC_USE_COMPLEX)
   zgssvx(&lu->options, &lu->A, lu->perm_c, lu->perm_r, lu->etree, lu->equed, lu->R, lu->C,
           &lu->L, &lu->U, lu->work, lu->lwork, &lu->B, &lu->X, &lu->rpg, &lu->rcond, &ferr, &berr,
           &lu->mem_usage, &stat, &sinfo);
#else
  dgssvx(&lu->options, &lu->A, lu->perm_c, lu->perm_r, lu->etree, lu->equed, lu->R, lu->C,
           &lu->L, &lu->U, lu->work, lu->lwork, &lu->B, &lu->X, &lu->rpg, &lu->rcond, &ferr, &berr,
           &lu->mem_usage, &stat, &sinfo);
#endif
  if ( !sinfo || sinfo == lu->A.ncol+1 ) {
    if ( lu->options.PivotGrowth ) 
      ierr = PetscPrintf(PETSC_COMM_SELF,"  Recip. pivot growth = %e\n", lu->rpg);
    if ( lu->options.ConditionNumber )
      ierr = PetscPrintf(PETSC_COMM_SELF,"  Recip. condition number = %e\n", lu->rcond);
  } else if ( sinfo > 0 ){
    if ( lu->lwork == -1 ) {
      ierr = PetscPrintf(PETSC_COMM_SELF,"  ** Estimated memory: %D bytes\n", sinfo - lu->A.ncol);
    } else {
      ierr = PetscPrintf(PETSC_COMM_SELF,"  Warning: gssvx() returns info %D\n",sinfo);
    }
  } else { /* sinfo < 0 */
    SETERRQ2(PETSC_ERR_LIB, "info = %D, the %D-th argument in gssvx() had an illegal value", sinfo,-sinfo); 
  }

  if ( lu->options.PrintStat ) {
    ierr = PetscPrintf(PETSC_COMM_SELF,"MatLUFactorNumeric_SuperLU():\n");
    StatPrint(&stat);
    Lstore = (SCformat *) lu->L.Store;
    Ustore = (NCformat *) lu->U.Store;
    ierr = PetscPrintf(PETSC_COMM_SELF,"  No of nonzeros in factor L = %D\n", Lstore->nnz);
    ierr = PetscPrintf(PETSC_COMM_SELF,"  No of nonzeros in factor U = %D\n", Ustore->nnz);
    ierr = PetscPrintf(PETSC_COMM_SELF,"  No of nonzeros in L+U = %D\n", Lstore->nnz + Ustore->nnz - lu->A.ncol);
    ierr = PetscPrintf(PETSC_COMM_SELF,"  L\\U MB %.3f\ttotal MB needed %.3f\texpansions %D\n",
	       lu->mem_usage.for_lu/1e6, lu->mem_usage.total_needed/1e6,
	       lu->mem_usage.expansions);
  }
  StatFree(&stat);

  lu->flg = SAME_NONZERO_PATTERN;
  (F)->ops->solve            = MatSolve_SuperLU;
  (F)->ops->solvetranspose   = MatSolveTranspose_SuperLU;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatDestroy_SuperLU"
PetscErrorCode MatDestroy_SuperLU(Mat A)
{
  PetscErrorCode ierr;
  Mat_SuperLU    *lu=(Mat_SuperLU*)A->spptr;

  PetscFunctionBegin;
  if (lu->CleanUpSuperLU) { /* Free the SuperLU datastructures */
    Destroy_SuperMatrix_Store(&lu->A); 
    Destroy_SuperMatrix_Store(&lu->B);
    Destroy_SuperMatrix_Store(&lu->X); 

    ierr = PetscFree(lu->etree);CHKERRQ(ierr);
    ierr = PetscFree(lu->perm_r);CHKERRQ(ierr);
    ierr = PetscFree(lu->perm_c);CHKERRQ(ierr);
    ierr = PetscFree(lu->R);CHKERRQ(ierr);
    ierr = PetscFree(lu->C);CHKERRQ(ierr);
    if ( lu->lwork >= 0 ) {
      Destroy_SuperNode_Matrix(&lu->L);
      Destroy_CompCol_Matrix(&lu->U);
    }
  }
  ierr = MatDestroy_SeqAIJ(A);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "MatView_SuperLU"
PetscErrorCode MatView_SuperLU(Mat A,PetscViewer viewer)
{
  PetscErrorCode    ierr;
  PetscTruth        iascii;
  PetscViewerFormat format;

  PetscFunctionBegin;
  ierr = MatView_SeqAIJ(A,viewer);CHKERRQ(ierr);

  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_ASCII,&iascii);CHKERRQ(ierr);
  if (iascii) {
    ierr = PetscViewerGetFormat(viewer,&format);CHKERRQ(ierr);
    if (format == PETSC_VIEWER_ASCII_INFO) {
      ierr = MatFactorInfo_SuperLU(A,viewer);CHKERRQ(ierr);
    }
  }
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SuperLU_Private"
PetscErrorCode MatSolve_SuperLU_Private(Mat A,Vec b,Vec x)
{
  Mat_SuperLU    *lu = (Mat_SuperLU*)A->spptr;
  PetscScalar    *barray,*xarray;
  PetscErrorCode ierr;
  PetscInt       info,i;
  SuperLUStat_t  stat;
  PetscReal      ferr,berr; 

  PetscFunctionBegin;
  if ( lu->lwork == -1 ) {
    PetscFunctionReturn(0);
  }
  lu->B.ncol = 1;   /* Set the number of right-hand side */
  ierr = VecGetArray(b,&barray);CHKERRQ(ierr);
  ierr = VecGetArray(x,&xarray);CHKERRQ(ierr);

#if defined(PETSC_USE_COMPLEX)
  ((DNformat*)lu->B.Store)->nzval = (doublecomplex*)barray;
  ((DNformat*)lu->X.Store)->nzval = (doublecomplex*)xarray;
#else
  ((DNformat*)lu->B.Store)->nzval = barray;
  ((DNformat*)lu->X.Store)->nzval = xarray;
#endif

  /* Initialize the statistics variables. */
  StatInit(&stat);

  lu->options.Fact  = FACTORED; /* Indicate the factored form of A is supplied. */
#if defined(PETSC_USE_COMPLEX)
  zgssvx(&lu->options, &lu->A, lu->perm_c, lu->perm_r, lu->etree, lu->equed, lu->R, lu->C,
           &lu->L, &lu->U, lu->work, lu->lwork, &lu->B, &lu->X, &lu->rpg, &lu->rcond, &ferr, &berr,
           &lu->mem_usage, &stat, &info);
#else
  dgssvx(&lu->options, &lu->A, lu->perm_c, lu->perm_r, lu->etree, lu->equed, lu->R, lu->C,
           &lu->L, &lu->U, lu->work, lu->lwork, &lu->B, &lu->X, &lu->rpg, &lu->rcond, &ferr, &berr,
           &lu->mem_usage, &stat, &info);
#endif   
  ierr = VecRestoreArray(b,&barray);CHKERRQ(ierr);
  ierr = VecRestoreArray(x,&xarray);CHKERRQ(ierr);

  if ( !info || info == lu->A.ncol+1 ) {
    if ( lu->options.IterRefine ) {
      ierr = PetscPrintf(PETSC_COMM_SELF,"Iterative Refinement:\n");
      ierr = PetscPrintf(PETSC_COMM_SELF,"  %8s%8s%16s%16s\n", "rhs", "Steps", "FERR", "BERR");
      for (i = 0; i < 1; ++i)
        ierr = PetscPrintf(PETSC_COMM_SELF,"  %8d%8d%16e%16e\n", i+1, stat.RefineSteps, ferr, berr);
    }
  } else if ( info > 0 ){
    if ( lu->lwork == -1 ) {
      ierr = PetscPrintf(PETSC_COMM_SELF,"  ** Estimated memory: %D bytes\n", info - lu->A.ncol);
    } else {
      ierr = PetscPrintf(PETSC_COMM_SELF,"  Warning: gssvx() returns info %D\n",info);
    }
  } else if (info < 0){
    SETERRQ2(PETSC_ERR_LIB, "info = %D, the %D-th argument in gssvx() had an illegal value", info,-info);
  }

  if ( lu->options.PrintStat ) {
    ierr = PetscPrintf(PETSC_COMM_SELF,"MatSolve__SuperLU():\n");
    StatPrint(&stat);
  }
  StatFree(&stat);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SuperLU"
PetscErrorCode MatSolve_SuperLU(Mat A,Vec b,Vec x)
{
  Mat_SuperLU    *lu = (Mat_SuperLU*)A->spptr;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  lu->options.Trans = TRANS;
  ierr = MatSolve_SuperLU_Private(A,b,x);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSolveTranspose_SuperLU"
PetscErrorCode MatSolveTranspose_SuperLU(Mat A,Vec b,Vec x)
{
  Mat_SuperLU    *lu = (Mat_SuperLU*)A->spptr;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  lu->options.Trans = NOTRANS;
  ierr = MatSolve_SuperLU_Private(A,b,x);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


/*
   Note the r permutation is ignored
*/
#undef __FUNCT__  
#define __FUNCT__ "MatLUFactorSymbolic_SuperLU"
PetscErrorCode MatLUFactorSymbolic_SuperLU(Mat F,Mat A,IS r,IS c,MatFactorInfo *info)
{
  Mat_SuperLU    *lu = (Mat_SuperLU*)((F)->spptr);
  PetscErrorCode ierr;
  PetscInt       m=A->rmap->n,n=A->cmap->n;

  PetscFunctionBegin;

  /* Allocate spaces (notice sizes are for the transpose) */
  ierr = PetscMalloc(m*sizeof(PetscInt),&lu->etree);CHKERRQ(ierr);
  ierr = PetscMalloc(n*sizeof(PetscInt),&lu->perm_r);CHKERRQ(ierr);
  ierr = PetscMalloc(m*sizeof(PetscInt),&lu->perm_c);CHKERRQ(ierr);
  ierr = PetscMalloc(n*sizeof(PetscInt),&lu->R);CHKERRQ(ierr);
  ierr = PetscMalloc(m*sizeof(PetscInt),&lu->C);CHKERRQ(ierr);
 
  /* create rhs and solution x without allocate space for .Store */
#if defined(PETSC_USE_COMPLEX)
  zCreate_Dense_Matrix(&lu->B, m, 1, PETSC_NULL, m, SLU_DN, SLU_Z, SLU_GE);
  zCreate_Dense_Matrix(&lu->X, m, 1, PETSC_NULL, m, SLU_DN, SLU_Z, SLU_GE);
#else
  dCreate_Dense_Matrix(&lu->B, m, 1, PETSC_NULL, m, SLU_DN, SLU_D, SLU_GE);
  dCreate_Dense_Matrix(&lu->X, m, 1, PETSC_NULL, m, SLU_DN, SLU_D, SLU_GE);
#endif

  lu->flg            = DIFFERENT_NONZERO_PATTERN;
  lu->CleanUpSuperLU = PETSC_TRUE;
  (F)->ops->lufactornumeric  = MatLUFactorNumeric_SuperLU;
  PetscFunctionReturn(0);
}


/*MC
  MAT_SOLVER_SUPERLU = "superlu" - A matrix type providing direct solvers (LU) for sequential matrices 
  via the external package SuperLU.

  Use config/configure.py --download-superlu to have PETSc installed with SuperLU

  Options Database Keys:
+ -mat_superlu_ordering <0,1,2,3> - 0: natural ordering, 
                                    1: MMD applied to A'*A, 
                                    2: MMD applied to A'+A, 
                                    3: COLAMD, approximate minimum degree column ordering
. -mat_superlu_iterrefine - have SuperLU do iterative refinement after the triangular solve
                          choices: NOREFINE, SINGLE, DOUBLE, EXTRA; default is NOREFINE
- -mat_superlu_printstat - print SuperLU statistics about the factorization

   Notes: Do not confuse this with MAT_SOLVER_SUPERLU_DIST which is for parallel sparse solves

   Level: beginner

.seealso: PCLU, MAT_SOLVER_SUPERLU_DIST, MAT_SOLVER_MUMPS, MAT_SOLVER_SPOOLES
M*/

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "MatGetFactor_seqaij_superlu"
PetscErrorCode MatGetFactor_seqaij_superlu(Mat A,MatFactorType ftype,Mat *F)
{
  Mat            B;
  Mat_SuperLU    *lu;
  PetscErrorCode ierr;
  PetscInt       indx;  
  PetscTruth     flg;
  const char     *colperm[]={"NATURAL","MMD_ATA","MMD_AT_PLUS_A","COLAMD"}; /* MY_PERMC - not supported by the petsc interface yet */
  const char     *iterrefine[]={"NOREFINE", "SINGLE", "DOUBLE", "EXTRA"};
  const char     *rowperm[]={"NOROWPERM", "LargeDiag"}; /* MY_PERMC - not supported by the petsc interface yet */

  PetscFunctionBegin;
  ierr = MatCreate(((PetscObject)A)->comm,&B);CHKERRQ(ierr);
  ierr = MatSetSizes(B,A->rmap->n,A->cmap->n,PETSC_DETERMINE,PETSC_DETERMINE);CHKERRQ(ierr);
  ierr = MatSetType(B,((PetscObject)A)->type_name);CHKERRQ(ierr);
  ierr = MatSeqAIJSetPreallocation(B,0,PETSC_NULL);CHKERRQ(ierr);

  B->ops->lufactorsymbolic = MatLUFactorSymbolic_SuperLU;
  B->ops->destroy          = MatDestroy_SuperLU;
  B->ops->view             = MatView_SuperLU;
  B->factor                = MAT_FACTOR_LU;
  B->assembled             = PETSC_TRUE;  /* required by -ksp_view */
  B->preallocated          = PETSC_TRUE;
  
  ierr = PetscNewLog(B,Mat_SuperLU,&lu);CHKERRQ(ierr);
  set_default_options(&lu->options);
  /* equilibration causes error in solve(), thus not supported here. See dgssvx.c for possible reason. */
  lu->options.Equil = NO;  
  lu->options.PrintStat = NO;
  lu->lwork = 0;   /* allocate space internally by system malloc */

  ierr = PetscOptionsBegin(((PetscObject)A)->comm,((PetscObject)A)->prefix,"SuperLU Options","Mat");CHKERRQ(ierr);
    ierr = PetscOptionsEList("-mat_superlu_colperm","ColPerm","None",colperm,4,colperm[3],&indx,&flg);CHKERRQ(ierr);
    if (flg) {lu->options.ColPerm = (colperm_t)indx;}
    ierr = PetscOptionsEList("-mat_superlu_iterrefine","IterRefine","None",iterrefine,4,iterrefine[0],&indx,&flg);CHKERRQ(ierr);
    if (flg) { lu->options.IterRefine = (IterRefine_t)indx;}
    ierr = PetscOptionsTruth("-mat_superlu_symmetricmode","SymmetricMode","None",PETSC_FALSE,&flg,0);CHKERRQ(ierr);
    if (flg) lu->options.SymmetricMode = YES; 
    ierr = PetscOptionsReal("-mat_superlu_diagpivotthresh","DiagPivotThresh","None",lu->options.DiagPivotThresh,&lu->options.DiagPivotThresh,PETSC_NULL);CHKERRQ(ierr);
    ierr = PetscOptionsTruth("-mat_superlu_pivotgrowth","PivotGrowth","None",PETSC_FALSE,&flg,0);CHKERRQ(ierr);
    if (flg) lu->options.PivotGrowth = YES;
    ierr = PetscOptionsTruth("-mat_superlu_conditionnumber","ConditionNumber","None",PETSC_FALSE,&flg,0);CHKERRQ(ierr);
    if (flg) lu->options.ConditionNumber = YES;
    ierr = PetscOptionsEList("-mat_superlu_rowperm","rowperm","None",rowperm,2,rowperm[0],&indx,&flg);CHKERRQ(ierr);
    if (flg) {lu->options.RowPerm = (rowperm_t)indx;}
    ierr = PetscOptionsTruth("-mat_superlu_replacetinypivot","ReplaceTinyPivot","None",PETSC_FALSE,&flg,0);CHKERRQ(ierr);
    if (flg) lu->options.ReplaceTinyPivot = YES; 
    ierr = PetscOptionsTruth("-mat_superlu_printstat","PrintStat","None",PETSC_FALSE,&flg,0);CHKERRQ(ierr);
    if (flg) lu->options.PrintStat = YES; 
    ierr = PetscOptionsInt("-mat_superlu_lwork","size of work array in bytes used by factorization","None",lu->lwork,&lu->lwork,PETSC_NULL);CHKERRQ(ierr); 
    if (lu->lwork > 0 ){
      ierr = PetscMalloc(lu->lwork,&lu->work);CHKERRQ(ierr); 
    } else if (lu->lwork != 0 && lu->lwork != -1){
      ierr = PetscPrintf(PETSC_COMM_SELF,"   Warning: lwork %D is not supported by SUPERLU. The default lwork=0 is used.\n",lu->lwork);
      lu->lwork = 0;
    }
  PetscOptionsEnd();

#ifdef SUPERLU2
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)B,"MatCreateNull","MatCreateNull_SuperLU",(void(*)(void))MatCreateNull_SuperLU);CHKERRQ(ierr);
#endif
  B->spptr = lu;
  *F = B;
  PetscFunctionReturn(0);
}
EXTERN_C_END
