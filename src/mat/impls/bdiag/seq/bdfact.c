#ifndef lint
static char vcid[] = "$Id: bdfact.c,v 1.27 1996/04/26 17:42:52 bsmith Exp bsmith $";
#endif

/* Block diagonal matrix format - factorization and triangular solves */

#include "bdiag.h"
#include "pinclude/plapack.h"

/* 
   BlockMatMult_Private - Computes C -= A*B, where
       A is nrow X nrow,
       B and C are nrow X ncol,
       All matrices are dense, stored by columns, where nrow is the 
           number of allocated rows for each matrix.
 */

#define BMatMult(nrow,ncol,A,B,C) BlockMatMult_Private(nrow,ncol,A,B,C)
static int BlockMatMult_Private(int nrow,int ncol,Scalar *A,Scalar *B,Scalar *C)
{
  Scalar B_i, *Apt;
  int    i, j, k, jnr;

  if (ncol == 1) {
    Apt = A;
    for (i=0; i<nrow; i++) {
      B_i = B[i];
      for (k=0; k<nrow; k++) C[k] -= Apt[k] * B_i;
      Apt += nrow;
    }
  } else {
    for (j=0; j<ncol; j++) {
      Apt = A;
      jnr = j*nrow;
      for (i=0; i<nrow; i++) {
        B_i = B[i+jnr];
        for (k=0; k<nrow; k++) C[k+jnr] -= Apt[k] * B_i;
        Apt += nrow;
      }
    }
  }
  PLogFlops(2*nrow*nrow*ncol);
  return 0;
}

int MatLUFactorSymbolic_SeqBDiag(Mat A,IS isrow,IS iscol,double f,Mat *B)
{
  Mat_SeqBDiag *a = (Mat_SeqBDiag *) A->data;

  if (a->m != a->n) SETERRQ(1,"MatLUFactorSymbolic_SeqBDiag:Matrix must be square");
  if (isrow || iscol)
    SETERRQ(1,"MatLUFactorSymbolic_SeqBDiag:permutations not supported");
  SETERRQ(1,"MatLUFactorSymbolic_SeqBDiag:Not written");
}

int MatILUFactorSymbolic_SeqBDiag(Mat A,IS isrow,IS iscol,double f,
                                  int levels,Mat *B)
{
  Mat_SeqBDiag *a = (Mat_SeqBDiag *) A->data;

  if (a->m != a->n) SETERRQ(1,"MatILUFactorSymbolic_SeqBDiag:Matrix must be square");
  if (isrow || iscol)
    SETERRQ(1,"MatILUFactorSymbolic_SeqBDiag:permutations not supported");
  if (levels != 0)
    SETERRQ(1,"MatLUFactorSymbolic_SeqBDiag:Only ILU(0) is supported");
  return MatConvert(A,MATSAME,B);
}

int  Linpack_DGEFA(Scalar *,int, int *);
int  Linpack_DGEDI(Scalar *,int, int *,Scalar *);

int MatLUFactorNumeric_SeqBDiag(Mat A,Mat *B)
{
  Mat          C = *B;
  Mat_SeqBDiag *a = (Mat_SeqBDiag *) C->data;
  int          info, k, d, d2, dgk, elim_row, elim_col, nb = a->nb, knb, knb2, nb2;
  int          dnum,nd = a->nd, mblock = a->mblock, nblock = a->nblock,ierr;
  int          *diag = a->diag, n = a->n, m = a->m, mainbd = a->mainbd, *dgptr;
  Scalar       **dv = a->diagv, *dd = dv[mainbd], mult, *v_work;
  Scalar       one = 1.0, zero = 0.0,*multiplier;

  /* Notes: 
      - We're using only B in this routine (A remains untouched).
      - The nb>1 case performs block LU, which is functionally the same as
        the nb=1 case, except that we use factorization, triangular solve,
        and matrix-matrix products within the dense subblocks.
      - Pivoting is not employed for the case nb=1; for the case nb>1
        pivoting is used only within the dense subblocks. 
   */
  if (nb == 1) {
    dgptr = (int *) PetscMalloc((m+n)*sizeof(int)); CHKPTRQ(dgptr);
    PetscMemzero(dgptr,(m+n)*sizeof(int));
    for ( k=0; k<nd; k++ ) dgptr[diag[k]+m] = k+1;
    for ( k=0; k<m; k++ ) { /* k = pivot_row */
      dd[k] = 1.0/dd[k];
      for ( d=mainbd-1; d>=0; d-- ) {
        elim_row = k + diag[d];
        if (elim_row < m) { /* sweep down */
          if (dv[d][k] != 0) {
            dv[d][k] *= dd[k];
            mult = dv[d][k];
            for ( d2=d+1; d2<nd; d2++ ) {
              elim_col = elim_row - diag[d2];
              if (elim_col >=0 && elim_col < n) {
                dgk = k - elim_col;
                if (dgk > 0) SETERRQ(1,
                   "MatLUFactorNumeric_SeqBDiag:bad elimination column");
                if ((dnum = dgptr[dgk+m])) {
                  if (diag[d2] > 0) dv[d2][elim_col] -= mult * dv[dnum-1][k];
                  else              dv[d2][elim_row] -= mult * dv[dnum-1][k];
                }
              }
            }
          }
        }
      }
    }
    PetscFree(dgptr);
  } 
  else {
    if (!a->pivot) {
      a->pivot = (int *) PetscMalloc(m*sizeof(int)); CHKPTRQ(a->pivot);
      PLogObjectMemory(C,m*sizeof(int));
    }
    v_work = (Scalar *) PetscMalloc((nb*nb+nb)*sizeof(Scalar)); CHKPTRQ(v_work);
    multiplier = v_work + nb;
    nb2 = nb*nb;
    dgptr = (int *) PetscMalloc((mblock+nblock)*sizeof(int)); CHKPTRQ(dgptr);
    PetscMemzero(dgptr,(mblock+nblock)*sizeof(int));
    for ( k=0; k<nd; k++ ) dgptr[diag[k]+mblock] = k+1;
    for ( k=0; k<mblock; k++ ) { /* k = block pivot_row */
      knb = k*nb; knb2 = knb*nb;
      /* invert the diagonal block */
printf("block"); DoubleView(nb*nb,&(dd[knb2]),0);
      
/* LAgetf2_(&nb,&nb,&(dd[knb2]),&nb,&(a->pivot[knb]),&info); CHKERRQ(info);*/
      ierr = Linpack_DGEFA(dd+knb2,nb,a->pivot+knb); CHKERRQ(ierr); 
printf("factored block");DoubleView(nb*nb,&(dd[knb2]),0);
      ierr = Linpack_DGEDI(dd+knb2,nb,a->pivot+knb,v_work); CHKERRQ(ierr);
printf("inverted block");DoubleView(nb*nb,&(dd[knb2]),0);
      for ( d=mainbd-1; d>=0; d-- ) {
        elim_row = k + diag[d];
        if (elim_row < mblock) { /* sweep down */
          /* dv[d][knb2]: test if entire block is zero? */
printf("lower block");DoubleView(nb*nb,&(dv[d][knb2]),0);
/* LAgetrs_("N",&nb,&nb,&dd[knb2],&nb,&(a->pivot[knb]),
                     &(dv[d][knb2]),&nb,&info); */

        BLgemm_("N","N",&nb,&nb,&nb,&one,&(dv[d][knb2]),&nb,&dd[knb2],&nb,&zero,
                multiplier,&nb);
        PetscMemcpy(&(dv[d][knb2]),multiplier,nb*nb*sizeof(Scalar));

printf("lower fixed block");DoubleView(nb*nb,&(dv[d][knb2]),0);
/*            if (info) SETERRQ(1,"MatLUFactorNumeric_SeqBDiag:Bad subblock triangular solve"); */
            for ( d2=d+1; d2<nd; d2++ ) {
              elim_col = elim_row - diag[d2];
              if (elim_col >=0 && elim_col < nblock) {
                dgk = k - elim_col;
                if (dgk > 0) SETERRQ(1,
                   "MatLUFactorNumeric_SeqBDiag:Bad elimination column");
                if ((dnum = dgptr[dgk+mblock])) {
                  if (diag[d2] > 0) BMatMult(nb,nb,&(dv[d][knb2]),
                            &(dv[dnum-1][knb2]),&(dv[d2][elim_col*nb2]));
                  else              BMatMult(nb,nb,&(dv[d][knb2]),
                            &(dv[dnum-1][knb2]),&(dv[d2][elim_row*nb2]));
                }
              }
            }
        }
      }
    }
    PetscFree(dgptr);
  }
  C->factor = FACTOR_LU;
  return 0;
}

int MatLUFactor_SeqBDiag(Mat A,IS isrow,IS iscol,double f)
{
  Mat_SeqBDiag *a = (Mat_SeqBDiag *) A->data;

  /* For now, no fill is allocated in symbolic factorization phase, so we
     directly use the input matrix for numeric factorization. */
  if (a->m != a->n) SETERRQ(1,"MatLUFactor_SeqBDiag:Matrix must be square");
  if (isrow || iscol) PLogInfo(A,
    "MatLUFactor_SeqBDiag: row and col permutations not supported\n");
  PLogInfo(A,"MatLUFactor_SeqBDiag:Only ILU(0) is supported\n");
  return MatLUFactorNumeric_SeqBDiag(A,&A);
}

int MatILUFactor_SeqBDiag(Mat A,IS isrow,IS iscol,double f,int level)
{
  Mat_SeqBDiag *a = (Mat_SeqBDiag *) A->data;

  /* For now, no fill is allocated in symbolic factorization phase, so we
     directly use the input matrix for numeric factorization. */
  if (a->m != a->n) SETERRQ(1,"MatILUFactor_SeqBDiag:Matrix must be square");
  if (isrow || iscol) PLogInfo(A,
    "MatILUFactor_SeqBDiag: row and col permutations not supported\n");
  if (level != 0)
    PLogInfo(A,"MatILUFactor_SeqBDiag:Only ILU(0) is supported\n");
  return MatLUFactorNumeric_SeqBDiag(A,&A);
}

int MatSolve_SeqBDiag(Mat A,Vec xx,Vec yy)
{
  Mat_SeqBDiag *a = (Mat_SeqBDiag *) A->data;
  int          one = 1, info, i, d, loc, ierr, mainbd = a->mainbd;
  int          mblock = a->mblock, nblock = a->nblock, inb, inb2;
  int          nb = a->nb, n = a->n, m = a->m, *diag = a->diag, col;
  Scalar       *x, *y, *dd = a->diagv[mainbd], sum, **dv = a->diagv;

  if (A->factor != FACTOR_LU) SETERRQ(1,"MatSolve_SeqBDiag:Not for unfactored matrix.");

  ierr = VecGetArray(xx,&x); CHKERRQ(ierr);
  ierr = VecGetArray(yy,&y); CHKERRQ(ierr);
  if (nb == 1) {
    /* forward solve the lower triangular part */
    for (i=0; i<m; i++) {
      sum = x[i];
      for (d=0; d<mainbd; d++) {
        loc = i - diag[d];
        if (loc >= 0) sum -= dv[d][loc] * y[loc];
      }
      y[i] = sum;
    }
    /* backward solve the upper triangular part */
    for ( i=m-1; i>=0; i-- ) {
      sum = y[i];
      for (d=mainbd+1; d<a->nd; d++) {
        col = i - diag[d];
        if (col < n) sum -= dv[d][i] * y[col];
      }
      y[i] = sum*dd[i];
    }
    PLogFlops(2*a->nz - a->n);
  } else {
    Scalar work[25],_DZero = 0.0,_DOne = 1.0;
    int    _One = 1;
    if (nb > 25) SETERRQ(1,"Blocks must be smaller then 25");
    PetscMemcpy(y,x,m*sizeof(Scalar));

    /* forward solve the lower triangular part */
    if (mainbd != 0) {
      for (i=0; i<mblock; i++) {
        inb = i*nb;
        for (d=0; d<mainbd; d++) {
          loc = i - diag[d];
          if (loc >= 0) BMatMult(nb,1,&(dv[d][loc*nb*nb]),
                                 &(y[loc*nb]),&(y[inb]));
        }
      }
    }
    /* backward solve the upper triangular part */
    for ( i=mblock-1; i>=0; i-- ) {
      inb = i*nb; inb2 = inb*nb;
      for (d=mainbd+1; d<a->nd; d++) {
        col = i - diag[d];
        if (col < nblock) BMatMult(nb,1,&(dv[d][inb2]),
                                   &(y[col*nb]),&(y[inb]));
      }
      /* LAgetrs_("N",&nb,&one,&(dd[inb2]),&nb,&(a->pivot[inb]),
               &(y[inb]),&nb,&info); */
        LAgemv_("N",&nb,&nb,&_DOne,&(dd[inb2]),&nb,&(y[inb]),&_One,&_DZero,
                work,&_One);
        PetscMemcpy(&(y[inb]),work,nb*sizeof(Scalar));

/*if (info) SETERRQ(1,"MatSolve_SeqBDiag:Bad subblock triangular solve");*/
    }
  }
  return 0;
}

int MatSolveTrans_SeqBDiag(Mat A,Vec xx,Vec yy)
{
  Mat_SeqBDiag *a = (Mat_SeqBDiag *) A->data;
  int          one = 1, info, i, ierr, nb = a->nb, m = a->m;
  Scalar       *x, *y, *submat;

  if (a->nd != 1 || a->diag[0] !=0) SETERRQ(1,
    "MatSolveTrans_SeqBDiag:Triangular solves only for main diag");
  ierr = VecGetArray(xx,&x); CHKERRQ(ierr);
  ierr = VecGetArray(yy,&y); CHKERRQ(ierr);
  PetscMemcpy(y,x,m*sizeof(Scalar));
  if (A->factor == FACTOR_LU) {
    submat = a->diagv[0];
    for (i=0; i<a->bdlen[0]; i++) {
      LAgetrs_("T",&nb,&one,&submat[i*nb*nb],&nb,&(a->pivot[i*nb]),y+i*nb,&nb,&info);
    }
  }
  else SETERRQ(1,"MatSolveTrans_SeqBDiag:Matrix must be factored to solve");
  if (info) SETERRQ(1,"MatSolveTrans_SeqBDiag:Bad subblock triangular solve");
  return 0;
}

int MatSolveAdd_SeqBDiag(Mat A,Vec xx,Vec zz,Vec yy)
{
  Mat_SeqBDiag *a = (Mat_SeqBDiag *) A->data;
  int          one = 1, info, ierr, i, nb = a->nb, m = a->m;
  Scalar       *x, *y, sone = 1.0, *submat;
  Vec          tmp = 0;

  if (a->nd != 1 || a->diag[0] !=0) SETERRQ(1,
    "MatSolveAdd_SeqBDiag:Triangular solves only for main diag");
  VecGetArray(xx,&x); VecGetArray(yy,&y);
  if (yy == zz) {
    ierr = VecDuplicate(yy,&tmp); CHKERRQ(ierr);
    PLogObjectParent(A,tmp);
    ierr = VecCopy(yy,tmp); CHKERRQ(ierr);
  } 
  PetscMemcpy(y,x,m*sizeof(Scalar));
  if (A->factor == FACTOR_LU) {
    submat = a->diagv[0];
    for (i=0; i<a->bdlen[0]; i++) {
      LAgetrs_("N",&nb,&one,&submat[i*nb*nb],&nb,&(a->pivot[i*nb]),y+i*nb,&nb,&info);
    }
  }
  if (info) SETERRQ(1,"MatSolveAdd_SeqBDiag:Bad subblock triangular solve");
  if (tmp) {VecAXPY(&sone,tmp,yy); VecDestroy(tmp);}
  else VecAXPY(&sone,zz,yy);
  return 0;
}
int MatSolveTransAdd_SeqBDiag(Mat A,Vec xx,Vec zz,Vec yy)
{
  Mat_SeqBDiag  *a = (Mat_SeqBDiag *) A->data;
  int           one = 1, info,ierr, i, nb = a->nb, m = a->m;
  Scalar        *x, *y, sone = 1.0, *submat;
  Vec           tmp;

  if (a->nd != 1 || a->diag[0] !=0) SETERRQ(1,
    "MatSolveTransAdd_SeqBDiag:Triangular solves only for main diag");
  VecGetArray(xx,&x); VecGetArray(yy,&y);
  if (yy == zz) {
    ierr = VecDuplicate(yy,&tmp); CHKERRQ(ierr);
    PLogObjectParent(A,tmp);
    ierr = VecCopy(yy,tmp); CHKERRQ(ierr);
  } 
  PetscMemcpy(y,x,m*sizeof(Scalar));
  if (A->factor == FACTOR_LU) {
    submat = a->diagv[0];
    for (i=0; i<a->bdlen[0]; i++) {
      LAgetrs_("T",&nb,&one,&submat[i*nb*nb],&nb,&(a->pivot[i*nb]),y+i*nb,&nb,&info);
    }
  }
  if (info) SETERRQ(1,"MatSolveTransAdd_SeqBDiag:Bad subblock triangular solve");
  if (tmp) {VecAXPY(&sone,tmp,yy); VecDestroy(tmp);}
  else VecAXPY(&sone,zz,yy);
  return 0;
}
