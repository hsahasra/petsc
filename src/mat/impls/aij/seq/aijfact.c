

#include "aij.h"
#include "inline/spops.h"
/*
    Factorization code for AIJ format. 
*/

int MatiAIJLUFactorSymbolic(Mat mat,IS isrow,IS iscol,Mat *fact)
{
  Matiaij *aij = (Matiaij *) mat->data, *aijnew;
  IS      isicol;
  int     *r,*ic, ierr, i, j, n = aij->m, *ai = aij->i, *aj = aij->j;
  int     prow, *ainew,*ajnew, jmax,*fill, *ajtmp, nz , *ii;
  int     *idnew, idx, pivot_row,row,m,fm, nnz, nzi,len;
 
  if (n != aij->n) SETERR(1,"Mat must be square");
  if (!isrow) {SETERR(1,"Must have row permutation");}
  if (!iscol) {SETERR(1,"Must have column permutation");}

  if (ierr = ISInvertPermutation(iscol,&isicol)) SETERR(ierr,0);
  ISGetIndices(isrow,&r); ISGetIndices(isicol,&ic);

  /* get new row pointers */
  ainew = (int *) MALLOC( (n+1)*sizeof(int) ); CHKPTR(ainew);
  ainew[0] = 1;
  /* don't know how many column pointers are needed so estimate */
  jmax = 2*ai[n];
  ajnew = (int *) MALLOC( (jmax)*sizeof(int) ); CHKPTR(ajnew);
  /* fill is a linked list of nonzeros in active row */
  fill = (int *) MALLOC( (n+1)*sizeof(int)); CHKPTR(fill);
  /* idnew is location of diagonal in factor */
  idnew = (int *) MALLOC( (n+1)*sizeof(int)); CHKPTR(idnew);
  idnew[0] = 1;

  for ( i=0; i<n; i++ ) {
    /* first copy previous fill into linked list */
    nnz = nz    = ai[r[i]+1] - ai[r[i]];
    ajtmp = aj + ai[r[i]] - 1;
    fill[n] = n;
    while (nz--) {
      fm = n;
      idx = ic[*ajtmp++ - 1];
      do {
        m = fm;
        fm = fill[m];
      } while (fm < idx);
      fill[m] = idx;
      fill[idx] = fm;
    }
    row = fill[n];
    while ( row < i ) {
      ajtmp = ajnew + idnew[row] - 1;
      nz = ainew[row+1] - idnew[row];
      fm = row;
      while (nz--) {
        fm = n;
        idx = *ajtmp++ - 1;
        do {
          m = fm;
          fm = fill[m];
        } while (fm < idx);
        if (fm != idx) {
          fill[m] = idx;
          fill[idx] = fm;
          fm = idx;
          nnz++;
        }
      }
      row = fill[row];
    }
    /* copy new filled row into permanent storage */
    ainew[i+1] = ainew[i] + nnz;
    if (ainew[i+1] > jmax+1) {
      /* allocate a longer ajnew */
      jmax += nnz*(n-i);
      ajtmp = (int *) MALLOC( jmax*sizeof(int) );CHKPTR(ajtmp);
      MEMCPY(ajtmp,ajnew,(ainew[i]-1)*sizeof(int));
      FREE(ajnew);
      ajnew = ajtmp;
    }
    ajtmp = ajnew + ainew[i] - 1;
    fm = fill[n];
    nzi = 0;
    while (nnz--) {
      if (fm < i) nzi++;
      *ajtmp++ = fm + 1;
      fm = fill[fm];
    }
    idnew[i] = ainew[i] + nzi;
  }

  ISDestroy(isicol); FREE(fill);

  /* put together the new matrix */
  ierr = MatCreateSequentialAIJ(n, n, 0, fact); CHKERR(ierr);
  aijnew = (Matiaij *) (*fact)->data;
  FREE(aijnew->imax);
  aijnew->singlemalloc = 0;
  len = (ainew[n] - 1)*sizeof(double);
  aijnew->a         = (Scalar *) MALLOC( len ); CHKPTR(aijnew->a);
  aijnew->j         = ajnew;
  aijnew->i         = ainew;
  aijnew->diag      = idnew;
  (*fact)->row      = isrow;
  (*fact)->col      = iscol;
  (*fact)->factor   = FACTOR_LU;
  return 0; 
}

int MatiAIJLUFactorNumeric(Mat mat,Mat fact)
{
  Matiaij *aij = (Matiaij *) mat->data, *aijnew = (Matiaij *)fact->data;
  IS      iscol = fact->col, isrow = fact->row, isicol;
  int     *r,*ic, ierr, i, j, n = aij->m, *ai = aijnew->i, *aj = aijnew->j;
  int     prow, *ainew,*ajnew, jmax,*fill, *ajtmpold, *ajtmp, nz , *ii;
  int     *idnew, idx, pivot_row,row,*pj, m,fm, nnz, nzi,len;
  Scalar  *rtmp,*vnew,*v, *pv, *pc, multiplier; 

  if (ierr = ISInvertPermutation(iscol,&isicol)) SETERR(ierr,0);
  ierr = ISGetIndices(isrow,&r); CHKERR(ierr);
  ierr = ISGetIndices(isicol,&ic); CHKERR(ierr);
  rtmp = (Scalar *) MALLOC( (n+1)*sizeof(Scalar) ); CHKPTR(rtmp);

  for ( i=0; i<n; i++ ) {
    nz = ai[i+1] - ai[i];
    ajtmp = aj + ai[i] - 1;
    for  ( j=0; j<nz; j++ ) rtmp[ajtmp[j]-1] = 0.0;

    /* load in initial (unfactored row) */
    nz = aij->i[r[i]+1] - aij->i[r[i]];
    ajtmpold = aij->j + aij->i[r[i]] - 1;
    v  = aij->a + aij->i[r[i]] - 1;
    for ( j=0; j<nz; j++ ) rtmp[ic[ajtmpold[j]-1]] =  v[j];

    row = *ajtmp++ - 1;
    while (row < i) {
      pc = rtmp + row;
      if (*pc != 0.0) {
        nz = aijnew->diag[row] - ai[row];
        pv = aijnew->a + aijnew->diag[row] - 1;
        pj = aijnew->j + aijnew->diag[row];
        multiplier = *pc * *pv++;
        *pc = multiplier;
        nz = ai[row+1] - ai[row] - 1 - nz;
        while (nz-->0) rtmp[*pj++ - 1] -= multiplier* *pv++; 
      }      
      row = *ajtmp++ - 1;
    }
    /* finished row so stick it into aijnew->a */
    pv = aijnew->a + ai[i] - 1;
    pj = aijnew->j + ai[i] - 1;
    nz = ai[i+1] - ai[i];
    rtmp[i] = 1.0/rtmp[i];
    for ( j=0; j<nz; j++ ) {pv[j] = rtmp[pj[j]-1];}
  } 
  FREE(rtmp);
  ierr = ISDestroy(isicol); CHKERR(ierr);
  fact->factor = FACTOR_LU;
 
  return 0;
}
int MatiAIJSolve(Mat mat,Vec bb, Vec xx)
{
  Matiaij *aij = (Matiaij *) mat->data;
  IS      iscol = mat->col, isrow = mat->row;
  int     *r,*c, ierr, i, j, n = aij->m, *vi, *ai = aij->i, *aj = aij->j;
  int     nz;
  Scalar  *x,*b,*tmp, *aa = aij->a, sum, *v;

  if (ierr = VecGetArray(bb,&b)) SETERR(ierr,0);
  if (ierr = VecGetArray(xx,&x)) SETERR(ierr,0);
  tmp = (Scalar *) MALLOC(n*sizeof(Scalar)); CHKPTR(tmp);

  if (ierr = ISGetIndices(isrow,&r)) SETERR(ierr,0);
  if (ierr = ISGetIndices(iscol,&c)) SETERR(ierr,0); c = c + (n-1);

  /* forward solve the lower triangular */
  tmp[0] = b[*r++];
  for ( i=1; i<n; i++ ) {
    v   = aa + ai[i] - 1;
    vi  = aj + ai[i] - 1;
    nz  = aij->diag[i] - ai[i];
    sum = b[*r++];
    while (nz--) sum -= *v++ * tmp[*vi++ - 1];
    tmp[i] = sum;
  }

  /* backward solve the upper triangular */
  for ( i=n-1; i>=0; i-- ){
    v   = aa + aij->diag[i];
    vi  = aj + aij->diag[i];
    nz  = ai[i+1] - aij->diag[i] - 1;
    sum = tmp[i];
    while (nz--) sum -= *v++ * tmp[*vi++ - 1];
    x[*c--] = tmp[i] = sum*aa[aij->diag[i]-1];
  }

  FREE(tmp);
  return 0;
}
