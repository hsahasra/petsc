/*<html><body><pre>*/
/*$Id: vector.c,v 1.204 2000/05/05 22:14:59 balay Exp bsmith $*/
/*
     Provides the interface functions for all vector operations.
   These are the vector functions the user calls.
*/
#include "src/vec/vecimpl.h"    /*I "petscvec.h" I*/

#undef __FUNC__  
#define __FUNC__ /*<a name="VecSetBlockSize"></a>*/"VecSetBlockSize"
/*@
   VecSetBlockSize - Sets the blocksize for future calls to VecSetValuesBlocked()
   and VecSetValuesBlockedLocal().

   Collective on Vec

   Input Parameter:
+  v - the vector
-  bs - the blocksize

   Notes:
   All vectors obtained by VecDuplicate() inherit the same blocksize.

   Level: advanced

.seealso: VecSetValuesBlocked(), VecSetLocalToGlobalMappingBlocked(), VecGetBlockSize()

.keywords: block size, vectors
@*/
int VecSetBlockSize(Vec v,int bs)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(v,VEC_COOKIE); 
  if (bs <= 0) bs = 1;
  if (bs == v->bs) PetscFunctionReturn(0);
  if (v->bs != -1) SETERRQ2(PETSC_ERR_ARG_WRONGSTATE,1,"Cannot reset blocksize. Current size %d new %d",v->bs,bs);
  if (v->N % bs) SETERRQ2(PETSC_ERR_ARG_OUTOFRANGE,1,"Vector length not divisible by blocksize %d %d",v->N,bs);
  if (v->n % bs) SETERRQ2(PETSC_ERR_ARG_OUTOFRANGE,1,"Local vector length not divisible by blocksize %d %d",v->n,bs);
  
  v->bs        = bs;
  v->bstash.bs = bs; /* use the same blocksize for the vec's block-stash */
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecGetBlockSize"
/*@
   VecGetBlockSize - Gets the blocksize for the vector, i.e. what is used for VecSetValuesBlocked()
   and VecSetValuesBlockedLocal().

   Collective on Vec

   Input Parameter:
.  v - the vector

   Output Parameter:
.  bs - the blocksize

   Notes:
   All vectors obtained by VecDuplicate() inherit the same blocksize.

   Level: advanced

.seealso: VecSetValuesBlocked(), VecSetLocalToGlobalMappingBlocked(), VecSetBlockSize()

.keywords: block size, vectors
@*/
int VecGetBlockSize(Vec v,int *bs)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(v,VEC_COOKIE); 
  *bs = v->bs;
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecValid"
/*@
   VecValid - Checks whether a vector object is valid.

   Not Collective

   Input Parameter:
.  v - the object to check

   Output Parameter:
   flg - flag indicating vector status, either
   PETSC_TRUE if vector is valid, or PETSC_FALSE otherwise.

   Level: developer

.keywords: vector, valid
@*/
int VecValid(Vec v,PetscTruth *flg)
{
  PetscFunctionBegin;
  PetscValidIntPointer(flg);
  if (!v)                           *flg = PETSC_FALSE;
  else if (v->cookie != VEC_COOKIE) *flg = PETSC_FALSE;
  else                              *flg = PETSC_TRUE;
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecDot"
/*@
   VecDot - Computes the vector dot product.

   Collective on Vec

   Input Parameters:
.  x, y - the vectors

   Output Parameter:
.  alpha - the dot product

   Performance Issues:
+    per-processor memory bandwidth
.    interprocessor latency
-    work load inbalance that causes certain processes to arrive much earlier than
     others

   Notes for Users of Complex Numbers:
   For complex vectors, VecDot() computes 
$     val = (x,y) = y^H x,
   where y^H denotes the conjugate transpose of y.

   Use VecTDot() for the indefinite form
$     val = (x,y) = y^T x,
   where y^T denotes the transpose of y.

   Level: intermediate

.keywords: vector, dot product, inner product

.seealso: VecMDot(), VecTDot(), VecNorm(), VecDotBegin(), VecDotEnd()
@*/
int VecDot(Vec x,Vec y,Scalar *val)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE); 
  PetscValidHeaderSpecific(y,VEC_COOKIE);
  PetscValidScalarPointer(val);
  PetscValidType(x);
  PetscValidType(y);
  PetscCheckSameType(x,y);
  PetscCheckSameComm(x,y);
  if (x->N != y->N) SETERRQ(PETSC_ERR_ARG_INCOMP,0,"Incompatible vector global lengths");
  if (x->n != y->n) SETERRQ(PETSC_ERR_ARG_INCOMP,0,"Incompatible vector local lengths");

  PLogEventBarrierBegin(VEC_DotBarrier,x,y,0,0,x->comm);
  ierr = (*x->ops->dot)(x,y,val);CHKERRQ(ierr);
  PLogEventBarrierEnd(VEC_DotBarrier,x,y,0,0,x->comm);
  /*
     The next block is for incremental debugging
  */
  if (PetscCompare) {
    int flag;
    ierr = MPI_Comm_compare(PETSC_COMM_WORLD,x->comm,&flag);CHKERRQ(ierr);
    if (flag != MPI_UNEQUAL) {
      ierr = PetscCompareScalar(*val);CHKERRQ(ierr);
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecNorm"
/*@
   VecNorm  - Computes the vector norm.

   Collective on Vec

   Input Parameters:
+  x - the vector
-  type - one of NORM_1, NORM_2, NORM_INFINITY.  Also available
          NORM_1_AND_2, which computes both norms and stores them
          in a two element array.

   Output Parameter:
.  val - the norm 

   Notes:
$     NORM_1 denotes sum_i |x_i|
$     NORM_2 denotes sqrt(sum_i (x_i)^2)
$     NORM_INFINITY denotes max_i |x_i|

   Level: intermediate

   Performance Issues:
+    per-processor memory bandwidth
.    interprocessor latency
-    work load inbalance that causes certain processes to arrive much earlier than
     others

   Compile Option:
   PETSC_HAVE_SLOW_NRM2 will cause a C (loop unrolled) version of the norm to be used, rather
 than the BLAS. This should probably only be used when one is using the FORTRAN BLAS routines 
 (as opposed to vendor provided) because the FORTRAN BLAS NRM2() routine is very slow. 

.keywords: vector, norm

.seealso: VecDot(), VecTDot(), VecNorm(), VecDotBegin(), VecDotEnd(), 
          VecNormBegin(), VecNormEnd()

@*/
int VecNorm(Vec x,NormType type,PetscReal *val)  
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidType(x);
  PLogEventBarrierBegin(VEC_NormBarrier,x,0,0,0,x->comm);
  ierr = (*x->ops->norm)(x,type,val);CHKERRQ(ierr);
  PLogEventBarrierEnd(VEC_NormBarrier,x,0,0,0,x->comm);
  /*
     The next block is for incremental debugging
  */
  if (PetscCompare) {
    int flag;
    ierr = MPI_Comm_compare(PETSC_COMM_WORLD,x->comm,&flag);CHKERRQ(ierr);
    if (flag != MPI_UNEQUAL) {
      ierr = PetscCompareDouble(*val);CHKERRQ(ierr);
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecMax"
/*@C
   VecMax - Determines the maximum vector component and its location.

   Collective on Vec

   Input Parameter:
.  x - the vector

   Output Parameters:
+  val - the maximum component
-  p - the location of val

   Notes:
   Returns the value PETSC_MIN and p = -1 if the vector is of length 0.

   Level: intermediate

.keywords: vector, maximum

.seealso: VecNorm(), VecMin()
@*/
int VecMax(Vec x,int *p,PetscReal *val)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidScalarPointer(val);
  PetscValidType(x);
  PLogEventBegin(VEC_Max,x,0,0,0);
  ierr = (*x->ops->max)(x,p,val);CHKERRQ(ierr);
  PLogEventEnd(VEC_Max,x,0,0,0);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecMin"
/*@
   VecMin - Determines the minimum vector component and its location.

   Collective on Vec

   Input Parameters:
.  x - the vector

   Output Parameter:
+  val - the minimum component
-  p - the location of val

   Level: intermediate

   Notes:
   Returns the value PETSC_MAX and p = -1 if the vector is of length 0.

.keywords: vector, minimum

.seealso: VecMax()
@*/
int VecMin(Vec x,int *p,PetscReal *val)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidScalarPointer(val);
  PetscValidType(x);
  PLogEventBegin(VEC_Min,x,0,0,0);
  ierr = (*x->ops->min)(x,p,val);CHKERRQ(ierr);
  PLogEventEnd(VEC_Min,x,0,0,0);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecTDot"
/*@
   VecTDot - Computes an indefinite vector dot product. That is, this
   routine does NOT use the complex conjugate.

   Collective on Vec

   Input Parameters:
.  x, y - the vectors

   Output Parameter:
.  val - the dot product

   Notes for Users of Complex Numbers:
   For complex vectors, VecTDot() computes the indefinite form
$     val = (x,y) = y^T x,
   where y^T denotes the transpose of y.

   Use VecDot() for the inner product
$     val = (x,y) = y^H x,
   where y^H denotes the conjugate transpose of y.

   Level: intermediate

.keywords: vector, dot product, inner product

.seealso: VecDot(), VecMTDot()
@*/
int VecTDot(Vec x,Vec y,Scalar *val) 
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidHeaderSpecific(y,VEC_COOKIE);
  PetscValidScalarPointer(val);
  PetscValidType(x);
  PetscValidType(y);
  PetscCheckSameType(x,y);
  PetscCheckSameComm(x,y);
  if (x->N != y->N) SETERRQ(PETSC_ERR_ARG_INCOMP,0,"Incompatible vector global lengths");
  if (x->n != y->n) SETERRQ(PETSC_ERR_ARG_INCOMP,0,"Incompatible vector local lengths");

  PLogEventBegin(VEC_TDot,x,y,0,0);
  ierr = (*x->ops->tdot)(x,y,val);CHKERRQ(ierr);
  PLogEventEnd(VEC_TDot,x,y,0,0);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecScale"
/*@
   VecScale - Scales a vector. 

   Collective on Vec

   Input Parameters:
+  x - the vector
-  alpha - the scalar

   Output Parameter:
.  x - the scaled vector

   Note:
   For a vector with n components, VecScale() computes 
$      x[i] = alpha * x[i], for i=1,...,n.

   Level: intermediate

.keywords: vector, scale
@*/
int VecScale(const Scalar *alpha,Vec x)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidScalarPointer(alpha);
  PetscValidType(x);
  PLogEventBegin(VEC_Scale,x,0,0,0);
  ierr = (*x->ops->scale)(alpha,x);CHKERRQ(ierr);
  PLogEventEnd(VEC_Scale,x,0,0,0);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecCopy"
/*@
   VecCopy - Copies a vector. 

   Collective on Vec

   Input Parameter:
.  x - the vector

   Output Parameter:
.  y - the copy

   Notes:
   For default parallel PETSc vectors, both x and y must be distributed in
   the same manner; local copies are done.

   Level: beginner

.keywords: vector, copy

.seealso: VecDuplicate()
@*/
int VecCopy(Vec x,Vec y)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE); 
  PetscValidHeaderSpecific(y,VEC_COOKIE);
  PetscValidType(x);
  PetscValidType(y);
  PetscCheckSameComm(x,y);
  if (x->N != y->N) SETERRQ(PETSC_ERR_ARG_INCOMP,0,"Incompatible vector global lengths");
  if (x->n != y->n) SETERRQ(PETSC_ERR_ARG_INCOMP,0,"Incompatible vector local lengths");

  PLogEventBegin(VEC_Copy,x,y,0,0);
  ierr = (*x->ops->copy)(x,y);CHKERRQ(ierr);
  PLogEventEnd(VEC_Copy,x,y,0,0);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecSet"
/*@
   VecSet - Sets all components of a vector to a single scalar value. 

   Collective on Vec

   Input Parameters:
+  alpha - the scalar
-  x  - the vector

   Output Parameter:
.  x  - the vector

   Note:
   For a vector of dimension n, VecSet() computes
$     x[i] = alpha, for i=1,...,n,
   so that all vector entries then equal the identical
   scalar value, alpha.  Use the more general routine
   VecSetValues() to set different vector entries.

   Level: beginner

.seealso VecSetValues(), VecSetValuesBlocked(), VecSetRandom()

.keywords: vector, set
@*/
int VecSet(const Scalar *alpha,Vec x) 
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidScalarPointer(alpha);
  PetscValidType(x);

  PLogEventBegin(VEC_Set,x,0,0,0);
  ierr = (*x->ops->set)(alpha,x);CHKERRQ(ierr);
  PLogEventEnd(VEC_Set,x,0,0,0);
  PetscFunctionReturn(0);
} 

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecSetRandom"
/*@C
   VecSetRandom - Sets all components of a vector to random numbers.

   Collective on Vec

   Input Parameters:
+  rctx - the random number context, formed by PetscRandomCreate()
-  x  - the vector

   Output Parameter:
.  x  - the vector

   Example of Usage:
.vb
     PetscRandomCreate(PETSC_COMM_WORLD,RANDOM_DEFAULT,&rctx);
     VecSetRandom(rctx,x);
     PetscRandomDestroy(rctx);
.ve

   Level: intermediate

.keywords: vector, set, random

.seealso: VecSet(), VecSetValues(), PetscRandomCreate(), PetscRandomDestroy()
@*/
int VecSetRandom(PetscRandom rctx,Vec x) 
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidHeaderSpecific(rctx,PETSCRANDOM_COOKIE);
  PetscValidType(x);

  PLogEventBegin(VEC_SetRandom,x,rctx,0,0);
  ierr = (*x->ops->setrandom)(rctx,x);CHKERRQ(ierr);
  PLogEventEnd(VEC_SetRandom,x,rctx,0,0);
  PetscFunctionReturn(0);
} 

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecAXPY"
/*@
   VecAXPY - Computes y = alpha x + y. 

   Collective on Vec

   Input Parameters:
+  alpha - the scalar
-  x, y  - the vectors

   Output Parameter:
.  y - output vector

   Level: intermediate

.keywords: vector, saxpy

.seealso: VecAYPX(), VecMAXPY(), VecWAXPY()
@*/
int VecAXPY(const Scalar *alpha,Vec x,Vec y)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidHeaderSpecific(y,VEC_COOKIE);
  PetscValidScalarPointer(alpha);
  PetscValidType(x);
  PetscValidType(y);
  PetscCheckSameType(x,y);
  PetscCheckSameComm(x,y);
  if (x->N != y->N) SETERRQ(PETSC_ERR_ARG_INCOMP,0,"Incompatible vector global lengths");
  if (x->n != y->n) SETERRQ(PETSC_ERR_ARG_INCOMP,0,"Incompatible vector local lengths");

  PLogEventBegin(VEC_AXPY,x,y,0,0);
  ierr = (*x->ops->axpy)(alpha,x,y);CHKERRQ(ierr);
  PLogEventEnd(VEC_AXPY,x,y,0,0);
  PetscFunctionReturn(0);
} 

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecAXPBY"
/*@
   VecAXPBY - Computes y = alpha x + beta y. 

   Collective on Vec

   Input Parameters:
+  alpha,beta - the scalars
-  x, y  - the vectors

   Output Parameter:
.  y - output vector

   Level: intermediate

.keywords: vector, saxpy

.seealso: VecAYPX(), VecMAXPY(), VecWAXPY(), VecAXPY()
@*/
int VecAXPBY(const Scalar *alpha,const Scalar *beta,Vec x,Vec y)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidHeaderSpecific(y,VEC_COOKIE);
  PetscValidScalarPointer(alpha);
  PetscValidScalarPointer(beta);
  PetscValidType(x);
  PetscValidType(y);
  PetscCheckSameType(x,y);
  PetscCheckSameComm(x,y);
  if (x->N != y->N) SETERRQ(PETSC_ERR_ARG_INCOMP,0,"Incompatible vector global lengths");
  if (x->n != y->n) SETERRQ(PETSC_ERR_ARG_INCOMP,0,"Incompatible vector local lengths");

  PLogEventBegin(VEC_AXPY,x,y,0,0);
  ierr = (*x->ops->axpby)(alpha,beta,x,y);CHKERRQ(ierr);
  PLogEventEnd(VEC_AXPY,x,y,0,0);
  PetscFunctionReturn(0);
} 

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecAYPX"
/*@
   VecAYPX - Computes y = x + alpha y.

   Collective on Vec

   Input Parameters:
+  alpha - the scalar
-  x, y  - the vectors

   Output Parameter:
.  y - output vector

   Level: intermediate

.keywords: vector, saypx

.seealso: VecAXPY(), VecWAXPY()
@*/
int VecAYPX(const Scalar *alpha,Vec x,Vec y)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE); 
  PetscValidHeaderSpecific(y,VEC_COOKIE);
  PetscValidScalarPointer(alpha);
  PetscValidType(x);
  PetscValidType(y);
  PetscCheckSameType(x,y);
  PetscCheckSameComm(x,y);
  if (x->N != y->N) SETERRQ(PETSC_ERR_ARG_INCOMP,0,"Incompatible vector global lengths");
  if (x->n != y->n) SETERRQ(PETSC_ERR_ARG_INCOMP,0,"Incompatible vector local lengths");

  PLogEventBegin(VEC_AYPX,x,y,0,0);
  ierr =  (*x->ops->aypx)(alpha,x,y);CHKERRQ(ierr);
  PLogEventEnd(VEC_AYPX,x,y,0,0);
  PetscFunctionReturn(0);
} 

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecSwap"
/*@
   VecSwap - Swaps the vectors x and y.

   Collective on Vec

   Input Parameters:
.  x, y  - the vectors

   Level: advanced

.keywords: vector, swap
@*/
int VecSwap(Vec x,Vec y)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE);  
  PetscValidHeaderSpecific(y,VEC_COOKIE);
  PetscValidType(x);
  PetscValidType(y);
  PetscCheckSameType(x,y);
  PetscCheckSameComm(x,y);
  if (x->N != y->N) SETERRQ(PETSC_ERR_ARG_INCOMP,0,"Incompatible vector global lengths");
  if (x->n != y->n) SETERRQ(PETSC_ERR_ARG_INCOMP,0,"Incompatible vector local lengths");

  PLogEventBegin(VEC_Swap,x,y,0,0);
  ierr = (*x->ops->swap)(x,y);CHKERRQ(ierr);
  PLogEventEnd(VEC_Swap,x,y,0,0);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecWAXPY"
/*@
   VecWAXPY - Computes w = alpha x + y.

   Collective on Vec

   Input Parameters:
+  alpha - the scalar
-  x, y  - the vectors

   Output Parameter:
.  w - the result

   Level: intermediate

.keywords: vector, waxpy

.seealso: VecAXPY(), VecAYPX()
@*/
int VecWAXPY(const Scalar *alpha,Vec x,Vec y,Vec w)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE); 
  PetscValidHeaderSpecific(y,VEC_COOKIE);
  PetscValidHeaderSpecific(w,VEC_COOKIE);
  PetscValidScalarPointer(alpha);
  PetscValidType(x);
  PetscValidType(y);
  PetscValidType(w);
  PetscCheckSameType(x,y); PetscCheckSameType(y,w);
  PetscCheckSameComm(x,y); PetscCheckSameComm(y,w);
  if (x->N != y->N || x->N != w->N) SETERRQ(PETSC_ERR_ARG_INCOMP,0,"Incompatible vector global lengths");
  if (x->n != y->n || x->n != w->n) SETERRQ(PETSC_ERR_ARG_INCOMP,0,"Incompatible vector local lengths");

  PLogEventBegin(VEC_WAXPY,x,y,w,0);
  ierr =  (*x->ops->waxpy)(alpha,x,y,w);CHKERRQ(ierr);
  PLogEventEnd(VEC_WAXPY,x,y,w,0);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecPointwiseMult"
/*@
   VecPointwiseMult - Computes the componentwise multiplication w = x*y.

   Collective on Vec

   Input Parameters:
.  x, y  - the vectors

   Output Parameter:
.  w - the result

   Level: advanced

.keywords: vector, multiply, componentwise, pointwise

.seealso: VecPointwiseDivide()
@*/
int VecPointwiseMult(Vec x,Vec y,Vec w)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE); 
  PetscValidHeaderSpecific(y,VEC_COOKIE);
  PetscValidHeaderSpecific(w,VEC_COOKIE);
  PetscValidType(x);
  PetscValidType(y);
  PetscValidType(w);
  PetscCheckSameType(x,y); PetscCheckSameType(y,w);
  PetscCheckSameComm(x,y); PetscCheckSameComm(y,w);
  if (x->N != y->N || x->N != w->N) SETERRQ(PETSC_ERR_ARG_INCOMP,0,"Incompatible vector global lengths");
  if (x->n != y->n || x->n != w->n) SETERRQ(PETSC_ERR_ARG_INCOMP,0,"Incompatible vector local lengths");

  PLogEventBegin(VEC_PMult,x,y,w,0);
  ierr = (*x->ops->pointwisemult)(x,y,w);CHKERRQ(ierr);
  PLogEventEnd(VEC_PMult,x,y,w,0);
  PetscFunctionReturn(0);
} 

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecPointwiseDivide"
/*@
   VecPointwiseDivide - Computes the componentwise division w = x/y.

   Collective on Vec

   Input Parameters:
.  x, y  - the vectors

   Output Parameter:
.  w - the result

   Level: advanced

.keywords: vector, divide, componentwise, pointwise

.seealso: VecPointwiseMult()
@*/
int VecPointwiseDivide(Vec x,Vec y,Vec w)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE); 
  PetscValidHeaderSpecific(y,VEC_COOKIE);
  PetscValidHeaderSpecific(w,VEC_COOKIE);
  PetscValidType(x);
  PetscValidType(y);
  PetscValidType(w);
  PetscCheckSameType(x,y); PetscCheckSameType(y,w);
  PetscCheckSameComm(x,y); PetscCheckSameComm(y,w);
  if (x->N != y->N || x->N != w->N) SETERRQ(PETSC_ERR_ARG_INCOMP,0,"Incompatible vector global lengths");
  if (x->n != y->n || x->n != w->n) SETERRQ(PETSC_ERR_ARG_INCOMP,0,"Incompatible vector local lengths");

  ierr = (*x->ops->pointwisedivide)(x,y,w);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecDuplicate"
/*@C
   VecDuplicate - Creates a new vector of the same type as an existing vector.

   Collective on Vec

   Input Parameters:
.  v - a vector to mimic

   Output Parameter:
.  newv - location to put new vector

   Notes:
   VecDuplicate() does not copy the vector, but rather allocates storage
   for the new vector.  Use VecCopy() to copy a vector.

   Use VecDestroy() to free the space. Use VecDuplicateVecs() to get several
   vectors. 

   Level: beginner

.keywords: vector, duplicate, create

.seealso: VecDestroy(), VecDuplicateVecs(), VecCreate(), VecCopy()
@*/
int VecDuplicate(Vec x,Vec *newv) 
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidPointer(newv);
  PetscValidType(x);
  ierr = (*x->ops->duplicate)(x,newv);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecDestroy"
/*@C
   VecDestroy - Destroys a vector.

   Collective on Vec

   Input Parameters:
.  v  - the vector

   Level: beginner

.keywords: vector, destroy

.seealso: VecDuplicate()
@*/
int VecDestroy(Vec v)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(v,VEC_COOKIE);
  if (--v->refct > 0) PetscFunctionReturn(0);
  /* destroy the internal part */
  if (v->ops->destroy) {
    ierr = (*v->ops->destroy)(v);CHKERRQ(ierr);
  }
  /* destroy the external/common part */
  if (v->mapping) {
    ierr = ISLocalToGlobalMappingDestroy(v->mapping);CHKERRQ(ierr);
  }
  if (v->bmapping) {
    ierr = ISLocalToGlobalMappingDestroy(v->bmapping);CHKERRQ(ierr);
  }
  if (v->map) {
    ierr = MapDestroy(v->map);CHKERRQ(ierr);
  }
  PLogObjectDestroy(v);
  PetscHeaderDestroy(v); 
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecDuplicateVecs"
/*@C
   VecDuplicateVecs - Creates several vectors of the same type as an existing vector.

   Collective on Vec

   Input Parameters:
+  m - the number of vectors to obtain
-  v - a vector to mimic

   Output Parameter:
.  V - location to put pointer to array of vectors

   Notes:
   Use VecDestroyVecs() to free the space. Use VecDuplicate() to form a single
   vector.

   Fortran Note:
   The Fortran interface is slightly different from that given below, it 
   requires one to pass in V a Vec (integer) array of size at least m.
   See the Fortran chapter of the users manual and petsc/src/vec/examples for details.

   Level: intermediate

.keywords: vector, duplicate

.seealso:  VecDestroyVecs(), VecDuplicate(), VecCreate(), VecDuplicateVecsF90()
@*/
int VecDuplicateVecs(Vec v,int m,Vec *V[])  
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(v,VEC_COOKIE);
  PetscValidPointer(V);
  PetscValidType(v);
  ierr = (*v->ops->duplicatevecs)(v, m,V);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecDestroyVecs"
/*@C
   VecDestroyVecs - Frees a block of vectors obtained with VecDuplicateVecs().

   Collective on Vec

   Input Parameters:
+  vv - pointer to array of vector pointers
-  m - the number of vectors previously obtained

   Fortran Note:
   The Fortran interface is slightly different from that given below.
   See the Fortran chapter of the users manual and 
   petsc/src/vec/examples for details.

   Level: intermediate

.keywords: vector, destroy

.seealso: VecDuplicateVecs(), VecDestroyVecsF90()
@*/
int VecDestroyVecs(const Vec vv[],int m)
{
  int ierr;

  PetscFunctionBegin;
  if (!vv) SETERRQ(PETSC_ERR_ARG_BADPTR,0,"Null vectors");
  PetscValidHeaderSpecific(*vv,VEC_COOKIE);
  PetscValidType(*vv);
  ierr = (*(*vv)->ops->destroyvecs)(vv,m);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecSetValues"
/*@
   VecSetValues - Inserts or adds values into certain locations of a vector. 

   Input Parameters:
   Not Collective

+  x - vector to insert in
.  ni - number of elements to add
.  ix - indices where to add
.  y - array of values
-  iora - either INSERT_VALUES or ADD_VALUES, where
   ADD_VALUES adds values to any existing entries, and
   INSERT_VALUES replaces existing entries with new values

   Notes: 
   VecSetValues() sets x[ix[i]] = y[i], for i=0,...,ni-1.

   Calls to VecSetValues() with the INSERT_VALUES and ADD_VALUES 
   options cannot be mixed without intervening calls to the assembly
   routines.

   These values may be cached, so VecAssemblyBegin() and VecAssemblyEnd() 
   MUST be called after all calls to VecSetValues() have been completed.

   VecSetValues() uses 0-based indices in Fortran as well as in C.

   Negative indices may be passed in ix, these rows are 
   simply ignored. This allows easily inserting element load matrices
   with homogeneous Dirchlet boundary conditions that you don't want represented
   in the vector.

   Level: beginner

.keywords: vector, set, values

.seealso:  VecAssemblyBegin(), VecAssemblyEnd(), VecSetValuesLocal(),
           VecSetValue(), VecSetValuesBlocked()
@*/
int VecSetValues(Vec x,int ni,const int ix[],const Scalar y[],InsertMode iora) 
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidIntPointer(ix);
  PetscValidScalarPointer(y);
  PetscValidType(x);
  PLogEventBegin(VEC_SetValues,x,0,0,0);
  ierr = (*x->ops->setvalues)(x,ni,ix,y,iora);CHKERRQ(ierr);
  PLogEventEnd(VEC_SetValues,x,0,0,0);  
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecSetValuesBlocked"
/*@
   VecSetValuesBlocked - Inserts or adds blocks of values into certain locations of a vector. 

   Not Collective

   Input Parameters:
+  x - vector to insert in
.  ni - number of blocks to add
.  ix - indices where to add in block count, rather than element count
.  y - array of values
-  iora - either INSERT_VALUES or ADD_VALUES, where
   ADD_VALUES adds values to any existing entries, and
   INSERT_VALUES replaces existing entries with new values

   Notes: 
   VecSetValuesBlocked() sets x[ix[bs*i]+j] = y[bs*i+j], 
   for j=0,...,bs, for i=0,...,ni-1. where bs was set with VecSetBlockSize().

   Calls to VecSetValuesBlocked() with the INSERT_VALUES and ADD_VALUES 
   options cannot be mixed without intervening calls to the assembly
   routines.

   These values may be cached, so VecAssemblyBegin() and VecAssemblyEnd() 
   MUST be called after all calls to VecSetValuesBlocked() have been completed.

   VecSetValuesBlocked() uses 0-based indices in Fortran as well as in C.

   Negative indices may be passed in ix, these rows are 
   simply ignored. This allows easily inserting element load matrices
   with homogeneous Dirchlet boundary conditions that you don't want represented
   in the vector.

   Level: intermediate

.keywords: vector, set, values, blocked

.seealso:  VecAssemblyBegin(), VecAssemblyEnd(), VecSetValuesBlockedLocal(),
           VecSetValues()
@*/
int VecSetValuesBlocked(Vec x,int ni,const int ix[],const Scalar y[],InsertMode iora) 
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidIntPointer(ix);
  PetscValidScalarPointer(y);
  PetscValidType(x);
  PLogEventBegin(VEC_SetValues,x,0,0,0);
  ierr = (*x->ops->setvaluesblocked)(x,ni,ix,y,iora);CHKERRQ(ierr);
  PLogEventEnd(VEC_SetValues,x,0,0,0);  
  PetscFunctionReturn(0);
}

/*MC
   VecSetValue - Set a single entry into a vector.

   Synopsis:
   void VecSetValue(Vec v,int row,Scalar value, InsertMode mode);

   Not Collective

   Input Parameters:
+  v - the vector
.  row - the row location of the entry
.  value - the value to insert
-  mode - either INSERT_VALUES or ADD_VALUES

   Notes:
   For efficiency one should use VecSetValues() and set several or 
   many values simultaneously if possible.

   Note that VecSetValue() does NOT return an error code (since this
   is checked internally).

   These values may be cached, so VecAssemblyBegin() and VecAssemblyEnd() 
   MUST be called after all calls to VecSetValues() have been completed.

   VecSetValues() uses 0-based indices in Fortran as well as in C.

   Level: beginner

.seealso: VecSetValues(), VecAssemblyBegin(), VecAssemblyEnd(), VecSetValuesBlockedLocal()
M*/

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecSetLocalToGlobalMapping"
/*@
   VecSetLocalToGlobalMapping - Sets a local numbering to global numbering used
   by the routine VecSetValuesLocal() to allow users to insert vector entries
   using a local (per-processor) numbering.

   Collective on Vec

   Input Parameters:
+  x - vector
-  mapping - mapping created with ISLocalToGlobalMappingCreate() or ISLocalToGlobalMappingCreateIS()

   Notes: 
   All vectors obtained with VecDuplicate() from this vector inherit the same mapping.

   Level: intermediate

.keywords: vector, set, values, local ordering

seealso:  VecAssemblyBegin(), VecAssemblyEnd(), VecSetValues(), VecSetValuesLocal(),
           VecSetLocalToGlobalMappingBlocked(), VecSetValuesBlockedLocal()
@*/
int VecSetLocalToGlobalMapping(Vec x,ISLocalToGlobalMapping mapping)
{
  int ierr;
  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidHeaderSpecific(mapping,IS_LTOGM_COOKIE);

  if (x->mapping) {
    SETERRQ(PETSC_ERR_ARG_WRONGSTATE,0,"Mapping already set for vector");
  }

  x->mapping = mapping;
  ierr = PetscObjectReference((PetscObject)mapping);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecSetLocalToGlobalMappingBlocked"
/*@
   VecSetLocalToGlobalMappingBlocked - Sets a local numbering to global numbering used
   by the routine VecSetValuesBlockedLocal() to allow users to insert vector entries
   using a local (per-processor) numbering.

   Collective on Vec

   Input Parameters:
+  x - vector
-  mapping - mapping created with ISLocalToGlobalMappingCreate() or ISLocalToGlobalMappingCreateIS()

   Notes: 
   All vectors obtained with VecDuplicate() from this vector inherit the same mapping.

   Level: intermediate

.keywords: vector, set, values, local ordering

.seealso:  VecAssemblyBegin(), VecAssemblyEnd(), VecSetValues(), VecSetValuesLocal(),
           VecSetLocalToGlobalMapping(), VecSetValuesBlockedLocal()
@*/
int VecSetLocalToGlobalMappingBlocked(Vec x,ISLocalToGlobalMapping mapping)
{
  int ierr;
  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidHeaderSpecific(mapping,IS_LTOGM_COOKIE);

  if (x->bmapping) {
    SETERRQ(PETSC_ERR_ARG_WRONGSTATE,0,"Mapping already set for vector");
  }
  x->bmapping = mapping;
  ierr = PetscObjectReference((PetscObject)mapping);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecSetValuesLocal"
/*@
   VecSetValuesLocal - Inserts or adds values into certain locations of a vector,
   using a local ordering of the nodes. 

   Not Collective

   Input Parameters:
+  x - vector to insert in
.  ni - number of elements to add
.  ix - indices where to add
.  y - array of values
-  iora - either INSERT_VALUES or ADD_VALUES, where
   ADD_VALUES adds values to any existing entries, and
   INSERT_VALUES replaces existing entries with new values

   Level: intermediate

   Notes: 
   VecSetValuesLocal() sets x[ix[i]] = y[i], for i=0,...,ni-1.

   Calls to VecSetValues() with the INSERT_VALUES and ADD_VALUES 
   options cannot be mixed without intervening calls to the assembly
   routines.

   These values may be cached, so VecAssemblyBegin() and VecAssemblyEnd() 
   MUST be called after all calls to VecSetValuesLocal() have been completed.

   VecSetValuesLocal() uses 0-based indices in Fortran as well as in C.

.keywords: vector, set, values, local ordering

.seealso:  VecAssemblyBegin(), VecAssemblyEnd(), VecSetValues(), VecSetLocalToGlobalMapping(),
           VecSetValuesBlockedLocal()
@*/
int VecSetValuesLocal(Vec x,int ni,const int ix[],const Scalar y[],InsertMode iora) 
{
  int ierr,lixp[128],*lix = lixp;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidIntPointer(ix);
  PetscValidScalarPointer(y);
  PetscValidType(x);
  if (!x->mapping) {
    SETERRQ(PETSC_ERR_ARG_WRONGSTATE,0,"Local to global never set with VecSetLocalToGlobalMapping()");
  }
  if (ni > 128) {
    lix = (int*)PetscMalloc(ni*sizeof(int));CHKPTRQ(lix);
  }

  PLogEventBegin(VEC_SetValues,x,0,0,0);
  ierr = ISLocalToGlobalMappingApply(x->mapping,ni,ix,lix);CHKERRQ(ierr);
  ierr = (*x->ops->setvalues)(x,ni,lix,y,iora);CHKERRQ(ierr);
  PLogEventEnd(VEC_SetValues,x,0,0,0);  
  if (ni > 128) {
    ierr = PetscFree(lix);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecSetValuesBlockedLocal"
/*@
   VecSetValuesBlockedLocal - Inserts or adds values into certain locations of a vector,
   using a local ordering of the nodes. 

   Not Collective

   Input Parameters:
+  x - vector to insert in
.  ni - number of blocks to add
.  ix - indices where to add in block count, not element count
.  y - array of values
-  iora - either INSERT_VALUES or ADD_VALUES, where
   ADD_VALUES adds values to any existing entries, and
   INSERT_VALUES replaces existing entries with new values

   Level: intermediate

   Notes: 
   VecSetValuesBlockedLocal() sets x[bs*ix[i]+j] = y[bs*i+j], 
   for j=0,..bs-1, for i=0,...,ni-1, where bs has been set with VecSetBlockSize().

   Calls to VecSetValuesBlockedLocal() with the INSERT_VALUES and ADD_VALUES 
   options cannot be mixed without intervening calls to the assembly
   routines.

   These values may be cached, so VecAssemblyBegin() and VecAssemblyEnd() 
   MUST be called after all calls to VecSetValuesBlockedLocal() have been completed.

   VecSetValuesBlockedLocal() uses 0-based indices in Fortran as well as in C.


.keywords: vector, set, values, blocked, local ordering

.seealso:  VecAssemblyBegin(), VecAssemblyEnd(), VecSetValues(), VecSetValuesBlocked(), 
           VecSetLocalToGlobalMappingBlocked()
@*/
int VecSetValuesBlockedLocal(Vec x,int ni,const int ix[],const Scalar y[],InsertMode iora) 
{
  int ierr,lixp[128],*lix = lixp;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidIntPointer(ix);
  PetscValidScalarPointer(y);
  PetscValidType(x);
  if (!x->bmapping) {
    SETERRQ(PETSC_ERR_ARG_WRONGSTATE,0,"Local to global never set with VecSetLocalToGlobalMappingBlocked()");
  }
  if (ni > 128) {
    lix = (int*)PetscMalloc(ni*sizeof(int));CHKPTRQ(lix);
  }

  PLogEventBegin(VEC_SetValues,x,0,0,0);
  ierr = ISLocalToGlobalMappingApply(x->bmapping,ni,ix,lix);CHKERRQ(ierr);
  ierr = (*x->ops->setvaluesblocked)(x,ni,lix,y,iora);CHKERRQ(ierr);
  PLogEventEnd(VEC_SetValues,x,0,0,0);  
  if (ni > 128) {
    ierr = PetscFree(lix);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecAssemblyBegin"
/*@
   VecAssemblyBegin - Begins assembling the vector.  This routine should
   be called after completing all calls to VecSetValues().

   Collective on Vec

   Input Parameter:
.  vec - the vector

   Level: beginner

.keywords: vector, begin, assembly, assemble

.seealso: VecAssemblyEnd(), VecSetValues()
@*/
int VecAssemblyBegin(Vec vec)
{
  int        ierr;
  PetscTruth flg;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(vec,VEC_COOKIE);
  PetscValidType(vec);

  ierr = OptionsHasName(vec->prefix,"-vec_view_stash",&flg);CHKERRQ(ierr);
  if (flg) {
    ierr = VecStashView(vec,VIEWER_STDOUT_(vec->comm));CHKERRQ(ierr);
  }

  PLogEventBegin(VEC_AssemblyBegin,vec,0,0,0);
  if (vec->ops->assemblybegin) {
    ierr = (*vec->ops->assemblybegin)(vec);CHKERRQ(ierr);
  }
  PLogEventEnd(VEC_AssemblyBegin,vec,0,0,0);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecAssemblyEnd"
/*@
   VecAssemblyEnd - Completes assembling the vector.  This routine should
   be called after VecAssemblyBegin().

   Collective on Vec

   Input Parameter:
.  vec - the vector

   Options Database Keys:
.  -vec_view - Prints vector in ASCII format
.  -vec_view_matlab - Prints vector in Matlab format
.  -vec_view_draw - Activates vector viewing using drawing tools
.  -display <name> - Sets display name (default is host)
.  -draw_pause <sec> - Sets number of seconds to pause after display
.  -vec_view_socket - Activates vector viewing using a socket
-  -vec_view_ams - Activates vector viewing using the ALICE Memory Snooper (AMS)
 
   Level: beginner

.keywords: vector, end, assembly, assemble

.seealso: VecAssemblyBegin(), VecSetValues()
@*/
int VecAssemblyEnd(Vec vec)
{
  int        ierr;
  PetscTruth flg;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(vec,VEC_COOKIE);
  PLogEventBegin(VEC_AssemblyEnd,vec,0,0,0);
  PetscValidType(vec);
  if (vec->ops->assemblyend) {
    ierr = (*vec->ops->assemblyend)(vec);CHKERRQ(ierr);
  }
  PLogEventEnd(VEC_AssemblyEnd,vec,0,0,0);
  ierr = OptionsHasName(vec->prefix,"-vec_view",&flg);CHKERRQ(ierr);
  if (flg) {
    ierr = VecView(vec,VIEWER_STDOUT_(vec->comm));CHKERRQ(ierr);
  }
  ierr = OptionsHasName(PETSC_NULL,"-vec_view_matlab",&flg);CHKERRQ(ierr);
  if (flg) {
    ierr = ViewerPushFormat(VIEWER_STDOUT_(vec->comm),VIEWER_FORMAT_ASCII_MATLAB,"V");CHKERRQ(ierr);
    ierr = VecView(vec,VIEWER_STDOUT_(vec->comm));CHKERRQ(ierr);
    ierr = ViewerPopFormat(VIEWER_STDOUT_(vec->comm));CHKERRQ(ierr);
  }
  ierr = OptionsHasName(PETSC_NULL,"-vec_view_draw",&flg);CHKERRQ(ierr);
  if (flg) {
    ierr = VecView(vec,VIEWER_DRAW_(vec->comm));CHKERRQ(ierr);
    ierr = ViewerFlush(VIEWER_DRAW_(vec->comm));CHKERRQ(ierr);
  }
  ierr = OptionsHasName(PETSC_NULL,"-vec_view_draw_lg",&flg);CHKERRQ(ierr);
  if (flg) {
    ierr = ViewerSetFormat(VIEWER_DRAW_(vec->comm),VIEWER_FORMAT_DRAW_LG,0);CHKERRQ(ierr);
    ierr = VecView(vec,VIEWER_DRAW_(vec->comm));CHKERRQ(ierr);
    ierr = ViewerFlush(VIEWER_DRAW_(vec->comm));CHKERRQ(ierr);
  }
  ierr = OptionsHasName(PETSC_NULL,"-vec_view_socket",&flg);CHKERRQ(ierr);
  if (flg) {
    ierr = VecView(vec,VIEWER_SOCKET_(vec->comm));CHKERRQ(ierr);
    ierr = ViewerFlush(VIEWER_SOCKET_(vec->comm));CHKERRQ(ierr);
  }
#if defined(PETSC_HAVE_AMS)
  ierr = OptionsHasName(PETSC_NULL,"-vec_view_ams",&flg);CHKERRQ(ierr);
  if (flg) {
    ierr = VecView(vec,VIEWER_AMS_(vec->comm));CHKERRQ(ierr);
  }
#endif
  PetscFunctionReturn(0);
}


#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecMTDot"
/*@C
   VecMTDot - Computes indefinite vector multiple dot products. 
   That is, it does NOT use the complex conjugate.

   Collective on Vec

   Input Parameters:
+  nv - number of vectors
.  x - one vector
-  y - array of vectors.  Note that vectors are pointers

   Output Parameter:
.  val - array of the dot products

   Notes for Users of Complex Numbers:
   For complex vectors, VecMTDot() computes the indefinite form
$      val = (x,y) = y^T x,
   where y^T denotes the transpose of y.

   Use VecMDot() for the inner product
$      val = (x,y) = y^H x,
   where y^H denotes the conjugate transpose of y.

   Level: intermediate

.keywords: vector, dot product, inner product, non-Hermitian, multiple

.seealso: VecMDot(), VecTDot()
@*/
int VecMTDot(int nv,Vec x,const Vec y[],Scalar *val)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidHeaderSpecific(*y,VEC_COOKIE);
  PetscValidScalarPointer(val);
  PetscValidType(x);
  PetscValidType(*y);
  PetscCheckSameType(x,*y);
  PetscCheckSameComm(x,*y);
  if (x->N != (*y)->N) SETERRQ(PETSC_ERR_ARG_INCOMP,0,"Incompatible vector global lengths");
  if (x->n != (*y)->n) SETERRQ(PETSC_ERR_ARG_INCOMP,0,"Incompatible vector local lengths");

  PLogEventBegin(VEC_MTDot,x,*y,0,0);
  ierr = (*x->ops->mtdot)(nv,x,y,val);CHKERRQ(ierr);
  PLogEventEnd(VEC_MTDot,x,*y,0,0);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecMDot"
/*@C
   VecMDot - Computes vector multiple dot products. 

   Collective on Vec

   Input Parameters:
+  nv - number of vectors
.  x - one vector
-  y - array of vectors. 

   Output Parameter:
.  val - array of the dot products

   Notes for Users of Complex Numbers:
   For complex vectors, VecMDot() computes 
$     val = (x,y) = y^H x,
   where y^H denotes the conjugate transpose of y.

   Use VecMTDot() for the indefinite form
$     val = (x,y) = y^T x,
   where y^T denotes the transpose of y.

   Level: intermediate

.keywords: vector, dot product, inner product, multiple

.seealso: VecMTDot(), VecDot()
@*/
int VecMDot(int nv,Vec x,const Vec y[],Scalar *val)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE); 
  PetscValidHeaderSpecific(*y,VEC_COOKIE);
  PetscValidScalarPointer(val);
  PetscValidType(x);
  PetscValidType(*y);
  PetscCheckSameType(x,*y);
  PetscCheckSameComm(x,*y);
  if (x->N != (*y)->N) SETERRQ(PETSC_ERR_ARG_INCOMP,0,"Incompatible vector global lengths");
  if (x->n != (*y)->n) SETERRQ(PETSC_ERR_ARG_INCOMP,0,"Incompatible vector local lengths");

  PLogEventBarrierBegin(VEC_MDotBarrier,x,*y,0,0,x->comm);
  ierr = (*x->ops->mdot)(nv,x,y,val);CHKERRQ(ierr);
  PLogEventBarrierEnd(VEC_MDotBarrier,x,*y,0,0,x->comm);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecMAXPY"
/*@C
   VecMAXPY - Computes y = y + sum alpha[j] x[j]

   Collective on Vec

   Input Parameters:
+  nv - number of scalars and x-vectors
.  alpha - array of scalars
.  y - one vector
-  x - array of vectors

   Output Parameter:
.  y  - array of vectors

   Level: intermediate

.keywords: vector, saxpy, maxpy, multiple

.seealso: VecAXPY(), VecWAXPY(), VecAYPX()
@*/
int  VecMAXPY(int nv,const Scalar *alpha,Vec y,Vec *x)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(y,VEC_COOKIE);
  PetscValidHeaderSpecific(*x,VEC_COOKIE);
  PetscValidScalarPointer(alpha);
  PetscValidType(y);
  PetscValidType(*x);
  PetscCheckSameType(y,*x);
  PetscCheckSameComm(y,*x);
  if (y->N != (*x)->N) SETERRQ(PETSC_ERR_ARG_INCOMP,0,"Incompatible vector global lengths");
  if (y->n != (*x)->n) SETERRQ(PETSC_ERR_ARG_INCOMP,0,"Incompatible vector local lengths");

  PLogEventBegin(VEC_MAXPY,*x,y,0,0);
  ierr = (*y->ops->maxpy)(nv,alpha,y,x);CHKERRQ(ierr);
  PLogEventEnd(VEC_MAXPY,*x,y,0,0);
  PetscFunctionReturn(0);
} 

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecGetArray"
/*@C
   VecGetArray - Returns a pointer to a contiguous array that contains this 
   processor's portion of the vector data. For the standard PETSc
   vectors, VecGetArray() returns a pointer to the local data array and
   does not use any copies. If the underlying vector data is not stored
   in a contiquous array this routine will copy the data to a contiquous
   array and return a pointer to that. You MUST call VecRestoreArray() 
   when you no longer need access to the array.

   Not Collective

   Input Parameter:
.  x - the vector

   Output Parameter:
.  a - location to put pointer to the array

   Fortran Note:
   This routine is used differently from Fortran 77
$    Vec         x
$    Scalar      x_array(1)
$    PetscOffset i_x
$    int         ierr
$       call VecGetArray(x,x_array,i_x,ierr)
$
$   Access first local entry in vector with
$      value = x_array(i_x + 1)
$
$      ...... other code
$       call VecRestoreArray(x,x_array,i_x,ierr)
   For Fortran 90 see VecGetArrayF90()

   See the Fortran chapter of the users manual and 
   petsc/src/snes/examples/tutorials/ex5f.F for details.

   Level: beginner

.keywords: vector, get, array

.seealso: VecRestoreArray(), VecGetArrays(), VecGetArrayF90(), VecPlaceArray(), VecGetArray2d()
@*/
int VecGetArray(Vec x,Scalar *a[])
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidPointer(a);
  PetscValidType(x);
  ierr = (*x->ops->getarray)(x,a);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecGetArrays" 
/*@C
   VecGetArrays - Returns a pointer to the arrays in a set of vectors
   that were created by a call to VecDuplicateVecs().  You MUST call
   VecRestoreArrays() when you no longer need access to the array.

   Not Collective

   Input Parameter:
+  x - the vectors
-  n - the number of vectors

   Output Parameter:
.  a - location to put pointer to the array

   Fortran Note:
   This routine is not supported in Fortran.

   Level: intermediate

.keywords: vector, get, arrays

.seealso: VecGetArray(), VecRestoreArrays()
@*/
int VecGetArrays(const Vec x[],int n,Scalar **a[])
{
  int    i,ierr;
  Scalar **q;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(*x,VEC_COOKIE);
  PetscValidPointer(a);
  if (n <= 0) SETERRQ1(PETSC_ERR_ARG_OUTOFRANGE,0,"Must get at least one array n = %d",n);
  q = (Scalar**)PetscMalloc(n*sizeof(Scalar*));CHKPTRQ(q);
  for (i=0; i<n; ++i) {
    ierr = VecGetArray(x[i],&q[i]);CHKERRQ(ierr);
  }
  *a = q;
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecRestoreArrays"
/*@C
   VecRestoreArrays - Restores a group of vectors after VecGetArrays()
   has been called.

   Not Collective

   Input Parameters:
+  x - the vector
.  n - the number of vectors
-  a - location of pointer to arrays obtained from VecGetArrays()

   Notes:
   For regular PETSc vectors this routine does not involve any copies. For
   any special vectors that do not store local vector data in a contiguous
   array, this routine will copy the data back into the underlying 
   vector data structure from the arrays obtained with VecGetArrays().

   Fortran Note:
   This routine is not supported in Fortran.

   Level: intermediate

.keywords: vector, restore, arrays

.seealso: VecGetArrays(), VecRestoreArray()
@*/
int VecRestoreArrays(const Vec x[],int n,Scalar **a[])
{
  int    i,ierr;
  Scalar **q = *a;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(*x,VEC_COOKIE);
  PetscValidPointer(a);
  q = *a;
  for(i=0;i<n;++i) {
    ierr = VecRestoreArray(x[i],&q[i]);CHKERRQ(ierr);
  }
  ierr = PetscFree(q);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecRestoreArray"
/*@C
   VecRestoreArray - Restores a vector after VecGetArray() has been called.

   Not Collective

   Input Parameters:
+  x - the vector
-  a - location of pointer to array obtained from VecGetArray()

   Level: beginner

   Notes:
   For regular PETSc vectors this routine does not involve any copies. For
   any special vectors that do not store local vector data in a contiguous
   array, this routine will copy the data back into the underlying 
   vector data structure from the array obtained with VecGetArray().

   This routine actually zeros out the a pointer. This is to prevent accidental
   us of the array after it has been restored. If you pass null for a it will 
   not zero the array pointer a.

   Fortran Note:
   This routine is used differently from Fortran 77
$    Vec         x
$    Scalar      x_array(1)
$    PetscOffset i_x
$    int         ierr
$       call VecGetArray(x,x_array,i_x,ierr)
$
$   Access first local entry in vector with
$      value = x_array(i_x + 1)
$
$      ...... other code
$       call VecRestoreArray(x,x_array,i_x,ierr)

   See the Fortran chapter of the users manual and 
   petsc/src/snes/examples/tutorials/ex5f.F for details.
   For Fortran 90 see VecRestoreArrayF90()

.keywords: vector, restore, array

.seealso: VecGetArray(), VecRestoreArrays(), VecRestoreArrayF90(), VecPlaceArray(), VecRestoreArray2d()
@*/
int VecRestoreArray(Vec x,Scalar *a[])
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  if (a) PetscValidPointer(a);
  PetscValidType(x);
  if (x->ops->restorearray) {
    ierr = (*x->ops->restorearray)(x,a);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}


#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecView"
/*@C
   VecView - Views a vector object. 

   Collective on Vec

   Input Parameters:
+  v - the vector
-  viewer - an optional visualization context

   Notes:
   The available visualization contexts include
+     VIEWER_STDOUT_SELF - standard output (default)
-     VIEWER_STDOUT_WORLD - synchronized standard
         output where only the first processor opens
         the file.  All other processors send their 
         data to the first processor to print. 

   You can change the format the vector is printed using the 
   option ViewerSetFormat().

   The user can open alternative visualization contexts with
+    ViewerASCIIOpen() - Outputs vector to a specified file
.    ViewerBinaryOpen() - Outputs vector in binary to a
         specified file; corresponding input uses VecLoad()
.    ViewerDrawOpen() - Outputs vector to an X window display
-    ViewerSocketOpen() - Outputs vector to Socket viewer

   The user can call ViewerSetFormat() to specify the output
   format of ASCII printed objects (when using VIEWER_STDOUT_SELF,
   VIEWER_STDOUT_WORLD and ViewerASCIIOpen).  Available formats include
+    VIEWER_FORMAT_ASCII_DEFAULT - default, prints vector contents
.    VIEWER_FORMAT_ASCII_MATLAB - prints vector contents in Matlab format
.    VIEWER_FORMAT_ASCII_INDEX - prints vector contents, including indices of vector elements
.    VIEWER_FORMAT_ASCII_COMMON - prints vector contents, using a 
         format common among all vector types
.    VIEWER_FORMAT_ASCII_INFO - prints basic information about the matrix
         size and structure (not the matrix entries)
-    VIEWER_FORMAT_ASCII_INFO_LONG - prints more detailed information about
         the matrix structure

   Level: beginner

.keywords: Vec, view, visualize, output, print, write, draw

.seealso: ViewerASCIIOpen(), ViewerDrawOpen(), DrawLGCreate(),
          ViewerSocketOpen(), ViewerBinaryOpen(), VecLoad(), ViewerCreate(),
          PetscDoubleView(), PetscScalarView(), PetscIntView()
@*/
int VecView(Vec vec,Viewer viewer)
{
  int ierr,format;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(vec,VEC_COOKIE);
  PetscValidType(vec);
  if (!viewer) viewer = VIEWER_STDOUT_(vec->comm);
  PetscValidHeaderSpecific(viewer,VIEWER_COOKIE);
  PetscCheckSameComm(vec,viewer);
  if (vec->stash.n || vec->bstash.n) SETERRQ(1,1,"Must call VecAssemblyBegin/End() before viewing this vector");

  /*
     Check if default viewer has been overridden, but user request it anyways
  */
  ierr = ViewerGetFormat(viewer,&format);CHKERRQ(ierr);
  if (vec->ops->viewnative && format == VIEWER_FORMAT_NATIVE) {
    char *fname;
    ierr   = ViewerGetOutputname(viewer,&fname);CHKERRQ(ierr);
    ierr   = ViewerPopFormat(viewer);CHKERRQ(ierr);
    ierr = (*vec->ops->viewnative)(vec,viewer);CHKERRQ(ierr);
    ierr   = ViewerPushFormat(viewer,VIEWER_FORMAT_NATIVE,fname);CHKERRQ(ierr);
  } else {
    ierr = (*vec->ops->view)(vec,viewer);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecGetSize"
/*@
   VecGetSize - Returns the global number of elements of the vector.

   Not Collective

   Input Parameter:
.  x - the vector

   Output Parameters:
.  size - the global length of the vector

   Level: beginner

.keywords: vector, get, size, global, dimension

.seealso: VecGetLocalSize()
@*/
int VecGetSize(Vec x,int *size)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidIntPointer(size);
  PetscValidType(x);
  ierr = (*x->ops->getsize)(x,size);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecGetLocalSize"
/*@
   VecGetLocalSize - Returns the number of elements of the vector stored 
   in local memory. This routine may be implementation dependent, so use 
   with care.

   Not Collective

   Input Parameter:
.  x - the vector

   Output Parameter:
.  size - the length of the local piece of the vector

   Level: beginner

.keywords: vector, get, dimension, size, local

.seealso: VecGetSize()
@*/
int VecGetLocalSize(Vec x,int *size)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidIntPointer(size);
  PetscValidType(x);
  ierr = (*x->ops->getlocalsize)(x,size);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecGetOwnershipRange"
/*@
   VecGetOwnershipRange - Returns the range of indices owned by 
   this processor, assuming that the vectors are laid out with the
   first n1 elements on the first processor, next n2 elements on the
   second, etc.  For certain parallel layouts this range may not be 
   well defined. 

   Not Collective

   Input Parameter:
.  x - the vector

   Output Parameters:
+  low - the first local element, pass in PETSC_NULL if not interested
-  high - one more than the last local element, pass in PETSC_NULL if not interested

   Note:
   The high argument is one more than the last element stored locally.

   Level: beginner

.keywords: vector, get, range, ownership
@*/
int VecGetOwnershipRange(Vec x,int *low,int *high)
{
  int ierr;
  Map map;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidType(x);
  if (low) PetscValidIntPointer(low);
  if (high) PetscValidIntPointer(high);
  ierr = (*x->ops->getmap)(x,&map);CHKERRQ(ierr);
  ierr = MapGetLocalRange(map,low,high);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecGetMap"
/*@C
   VecGetMap - Returns the map associated with the vector

   Not Collective

   Input Parameter:
.  x - the vector

   Output Parameters:
.  map - the map

   Level: developer

.keywords: vector, get, map
@*/
int VecGetMap(Vec x,Map *map)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidType(x);
  ierr = (*x->ops->getmap)(x,map);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecSetOption"
/*@
   VecSetOption - Sets an option for controling a vector's behavior.

   Collective on Vec

   Input Parameter:
+  x - the vector
-  op - the option

   Notes: 
   Currently the only option supported is
   VEC_IGNORE_OFF_PROC_ENTRIES, which causes VecSetValues() to ignore 
   entries destined to be stored on a seperate processor.

   Level: intermediate

.keywords: vector, options
@*/
int VecSetOption(Vec x,VecOption op)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidType(x);
  if (x->ops->setoption) {
    ierr = (*x->ops->setoption)(x,op);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecDuplicateVecs_Default"
/* Default routines for obtaining and releasing; */
/* may be used by any implementation */
int VecDuplicateVecs_Default(Vec w,int m,Vec *V[])
{
  int  i,ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(w,VEC_COOKIE);
  PetscValidPointer(V);
  if (m <= 0) SETERRQ1(PETSC_ERR_ARG_OUTOFRANGE,0,"m must be > 0: m = %d",m);
  *V = (Vec*)PetscMalloc(m * sizeof(Vec *));CHKPTRQ(*V);
  for (i=0; i<m; i++) {ierr = VecDuplicate(w,*V+i);CHKERRQ(ierr);}
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecDestroyVecs_Default"
int VecDestroyVecs_Default(const Vec v[], int m)
{
  int i,ierr;

  PetscFunctionBegin;
  PetscValidPointer(v);
  if (m <= 0) SETERRQ1(PETSC_ERR_ARG_OUTOFRANGE,0,"m must be > 0: m = %d",m);
  for (i=0; i<m; i++) {ierr = VecDestroy(v[i]);CHKERRQ(ierr);}
  ierr = PetscFree((Vec*)v);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecPlaceArray"
/*@
   VecPlaceArray - Allows one to replace the array in a vector with an
   array provided by the user. This is useful to avoid copying an array
   into a vector.  FOR EXPERTS ONLY!

   Not Collective

   Input Parameters:
+  vec - the vector
-  array - the array

   Notes:
   You can back up the original array by calling VecGetArray() followed 
   by VecRestoreArray() and stashing the value somewhere.  Then when
   finished using the vector, call VecPlaceArray() with that stashed value;
   otherwise, you may lose access to the original array.

   Level: developer

.seealso: VecGetArray(), VecRestoreArray(), VecReplaceArray()

.keywords: vec, place, array
@*/
int VecPlaceArray(Vec vec,const Scalar array[])
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(vec,VEC_COOKIE);
  PetscValidType(vec);
  if (vec->ops->placearray) {
    ierr = (*vec->ops->placearray)(vec,array);CHKERRQ(ierr);
  } else {
    SETERRQ(1,1,"Cannot place array in this type of vector");
  }
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecReplaceArray"
/*@C
   VecReplaceArray - Allows one to replace the array in a vector with an
   array provided by the user. This is useful to avoid copying an array
   into a vector.  FOR EXPERTS ONLY!

   Not Collective

   Input Parameters:
+  vec - the vector
-  array - the array

   Notes:
   This permanently replaces the array and frees the memory associated
   with the old array.

   Not supported from Fortran

   Level: developer

.seealso: VecGetArray(), VecRestoreArray(), VecPlaceArray()

.keywords: vec, place, array
@*/
int VecReplaceArray(Vec vec,const Scalar array[])
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(vec,VEC_COOKIE);
  PetscValidType(vec);
  if (vec->ops->replacearray) {
    ierr = (*vec->ops->replacearray)(vec,array);CHKERRQ(ierr);
  } else {
    SETERRQ(1,1,"Cannot replace array in this type of vector");
  }
  PetscFunctionReturn(0);
}

/*MC
    VecDuplicateVecsF90 - Creates several vectors of the same type as an existing vector
    and makes them accessible via a Fortran90 pointer.

    Synopsis:
    VecGetArrayF90(Vec x,int n,{Scalar, pointer :: y(:)},integer ierr)

    Collective on Vec

    Input Parameters:
+   x - a vector to mimic
-   n - the number of vectors to obtain

    Output Parameters:
+   y - Fortran90 pointer to the array of vectors
-   ierr - error code

    Example of Usage: 
.vb
    Vec x
    Vec, pointer :: y(:)
    ....
    call VecDuplicateVecsF90(x,2,y,ierr)
    call VecSet(alpha,y(2),ierr)
    call VecSet(alpha,y(2),ierr)
    ....
    call VecDestroyVecsF90(y,2,ierr)
.ve

    Notes:
    Not yet supported for all F90 compilers

    Use VecDestroyVecsF90() to free the space.

    Level: beginner

.seealso:  VecDestroyVecsF90(), VecDuplicateVecs()

.keywords:  vector, duplicate, f90
M*/

/*MC
    VecRestoreArrayF90 - Restores a vector to a usable state after a call to
    VecGetArrayF90().

    Synopsis:
    VecRestoreArrayF90(Vec x,{Scalar, pointer :: xx_v(:)},integer ierr)

    Not collective

    Input Parameters:
+   x - vector
-   xx_v - the Fortran90 pointer to the array

    Output Parameter:
.   ierr - error code

    Example of Usage: 
.vb
    Scalar, pointer :: xx_v(:)
    ....
    call VecGetArrayF90(x,xx_v,ierr)
    a = xx_v(3)
    call VecRestoreArrayF90(x,xx_v,ierr)
.ve
   
    Notes:
    Not yet supported for all F90 compilers

    Level: beginner

.seealso:  VecGetArrayF90(), VecGetArray(), VecRestoreArray()

.keywords:  vector, array, f90
M*/

/*MC
    VecDestroyVecsF90 - Frees a block of vectors obtained with VecDuplicateVecsF90().

    Synopsis:
    VecDestroyVecsF90({Scalar, pointer :: x(:)},integer n,integer ierr)

    Input Parameters:
+   x - pointer to array of vector pointers
-   n - the number of vectors previously obtained

    Output Parameter:
.   ierr - error code

    Notes:
    Not yet supported for all F90 compilers

    Level: beginner

.seealso:  VecDestroyVecs(), VecDuplicateVecsF90()

.keywords:  vector, destroy, f90
M*/

/*MC
    VecGetArrayF90 - Accesses a vector array from Fortran90. For default PETSc
    vectors, VecGetArrayF90() returns a pointer to the local data array. Otherwise,
    this routine is implementation dependent. You MUST call VecRestoreArrayF90() 
    when you no longer need access to the array.

    Synopsis:
    VecGetArrayF90(Vec x,{Scalar, pointer :: xx_v(:)},integer ierr)

    Not Collective 

    Input Parameter:
.   x - vector

    Output Parameters:
+   xx_v - the Fortran90 pointer to the array
-   ierr - error code

    Example of Usage: 
.vb
    Scalar, pointer :: xx_v(:)
    ....
    call VecGetArrayF90(x,xx_v,ierr)
    a = xx_v(3)
    call VecRestoreArrayF90(x,xx_v,ierr)
.ve

    Notes:
    Not yet supported for all F90 compilers

    Level: beginner

.seealso:  VecRestoreArrayF90(), VecGetArray(), VecRestoreArray()

.keywords:  vector, array, f90
M*/

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecLoadIntoVector"
/*@C 
  VecLoadIntoVector - Loads a vector that has been stored in binary format
  with VecView().

  Collective on Viewer 

  Input Parameters:
+ viewer - binary file viewer, obtained from ViewerBinaryOpen()
- vec - vector to contain files values (must be of correct length)

  Level: intermediate

  Notes:
  The input file must contain the full global vector, as
  written by the routine VecView().

  Use VecLoad() to create the vector as the values are read in

  Notes for advanced users:
  Most users should not need to know the details of the binary storage
  format, since VecLoad() and VecView() completely hide these details.
  But for anyone who's interested, the standard binary matrix storage
  format is
.vb
     int    VEC_COOKIE
     int    number of rows
     Scalar *values of all nonzeros
.ve

   Note for Cray users, the int's stored in the binary file are 32 bit
integers; not 64 as they are represented in the memory, so if you
write your own routines to read/write these binary files from the Cray
you need to adjust the integer sizes that you read in, see
PetscReadBinary() and PetscWriteBinary() to see how this may be
done.

   In addition, PETSc automatically does the byte swapping for
machines that store the bytes reversed, e.g.  DEC alpha, freebsd,
linux, nt and the paragon; thus if you write your own binary
read/write routines you have to swap the bytes; see PetscReadBinary()
and PetscWriteBinary() to see how this may be done.

.keywords: vector, load, binary, input

.seealso: ViewerBinaryOpen(), VecView(), MatLoad(), VecLoad() 
@*/  
int VecLoadIntoVector(Viewer viewer,Vec vec)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer,VIEWER_COOKIE);
  PetscValidHeaderSpecific(vec,VEC_COOKIE);
  PetscValidType(vec);
  if (!vec->ops->loadintovector) {
    SETERRQ(1,1,"Vector does not support load");
  }
  ierr = (*vec->ops->loadintovector)(viewer,vec);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecReciprocal"
/*@
   VecReciprocal - Replaces each component of a vector by its reciprocal.

   Collective on Vec

   Input Parameter:
.  v - the vector 

   Output Parameter:
.  v - the vector reciprocal

   Level: intermediate

.keywords: vector, reciprocal
@*/
int VecReciprocal(Vec vec)
{
  int    ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(vec,VEC_COOKIE);
  PetscValidType(vec);
  if (!vec->ops->reciprocal) {
    SETERRQ(1,1,"Vector does not support reciprocal operation");
  }
  ierr = (*vec->ops->reciprocal)(vec);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecSetOperation"
int VecSetOperation(Vec vec,VecOperation op, void *f)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(vec,VEC_COOKIE);
  /* save the native version of the viewer */
  if (op == VECOP_VIEW && !vec->ops->viewnative) {
    vec->ops->viewnative = vec->ops->view;
  }
  (((void**)vec->ops)[(int)op]) = f;
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecSetStashInitialSize"
/*@
   VecSetStashInitialSize - sets the sizes of the vec-stash, that is
   used during the assembly process to store values that belong to 
   other processors.

   Collective on Vec

   Input Parameters:
+  vec   - the vector
.  size  - the initial size of the stash.
-  bsize - the initial size of the block-stash(if used).

   Options Database Keys:
+   -vecstash_initial_size <size> or <size0,size1,...sizep-1>
-   -vecstash_block_initial_size <bsize> or <bsize0,bsize1,...bsizep-1>

   Level: intermediate

   Notes: 
     The block-stash is used for values set with VecSetValuesBlocked() while
     the stash is used for values set with VecSetValues()

     Run with the option -log_info and look for output of the form
     VecAssemblyBegin_MPIXXX:Stash has MM entries, uses nn mallocs.
     to determine the appropriate value, MM, to use for size and 
     VecAssemblyBegin_MPIXXX:Block-Stash has BMM entries, uses nn mallocs.
     to determine the value, BMM to use for bsize

.keywords: vector, stash, assembly

.seealso: VecSetBlockSize(), VecSetValues(), VecSetValuesBlocked(), VecStashView()

@*/
int VecSetStashInitialSize(Vec vec,int size,int bsize)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(vec,VEC_COOKIE);
  ierr = VecStashSetInitialSize_Private(&vec->stash,size);CHKERRQ(ierr);
  ierr = VecStashSetInitialSize_Private(&vec->bstash,bsize);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name="VecStashView"></a>*/"VecStashView"
/*@
   VecStashView - Prints the entries in the vector stash and block stash.

   Collective on Vec

   Input Parameters:
+  vec   - the vector
-  viewer - the viewer

.keywords: vector, stash, assembly

.seealso: VecSetBlockSize(), VecSetValues(), VecSetValuesBlocked()

@*/
int VecStashView(Vec v,Viewer viewer)
{
  int        ierr,rank,i,j;
  PetscTruth match;
  VecStash   *s;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(v,VEC_COOKIE);
  PetscValidHeaderSpecific(viewer,VIEWER_COOKIE);
  PetscCheckSameComm(v,viewer);

  ierr = PetscTypeCompare((PetscObject)viewer,ASCII_VIEWER,&match);CHKERRQ(ierr);
  if (!match) SETERRQ1(1,1,"Stash viewer only works with ASCII viewer not %s\n",((PetscObject)v)->type_name);
  ierr = ViewerASCIIUseTabs(viewer,PETSC_FALSE);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(v->comm,&rank);CHKERRQ(ierr);
  s = &v->bstash;

  /* print block stash */
  ierr = ViewerASCIISynchronizedPrintf(viewer,"[%d]Vector Block stash size %d block size %d\n",rank,s->n,s->bs);CHKERRQ(ierr);
  for (i=0; i<s->n; i++) {
    ierr = ViewerASCIISynchronizedPrintf(viewer,"[%d] Element %d ",rank,s->idx[i]);CHKERRQ(ierr);
    for (j=0; j<s->bs; j++) {
      ierr = ViewerASCIISynchronizedPrintf(viewer,"%g ",s->array[i*s->bs+j]);CHKERRQ(ierr);
    }
    ierr = ViewerASCIISynchronizedPrintf(viewer,"\n");CHKERRQ(ierr);
  }
  ierr = ViewerFlush(viewer);CHKERRQ(ierr);

  s = &v->stash;

  /* print basic stash */
  ierr = ViewerASCIISynchronizedPrintf(viewer,"[%d]Vector stash size %d\n",rank,s->n);CHKERRQ(ierr);
  for (i=0; i<s->n; i++) {
    ierr = ViewerASCIISynchronizedPrintf(viewer,"[%d] Element %d %g\n",rank,s->idx[i],s->array[i]);CHKERRQ(ierr);
  }
  ierr = ViewerFlush(viewer);CHKERRQ(ierr);

  ierr = ViewerASCIIUseTabs(viewer,PETSC_TRUE);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}  

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecGetArray2d"
/*@C
   VecGetArray2d - Returns a pointer to a 2d contiguous array that contains this 
   processor's portion of the vector data.  You MUST call VecRestoreArray2d() 
   when you no longer need access to the array.

   Not Collective

   Input Parameter:
+  x - the vector
.  m - first dimension of two dimensional array
.  n - second dimension of two dimensional array
.  mstart - first index you will use in first coordinate direction (often 0)
-  nstart - first index in the second coordinate direction (often 0)

   Output Parameter:
.  a - location to put pointer to the array

   Level: beginner

  Notes:
   For a vector obtained from DACreateLocalVector() mstart and nstart are likely
   obtained from the corner indices obtained from DAGetGhostCorners() while for
   DACreateGlobalVector() they are the corner indices from DAGetCorners(). In both cases
   the arguments from DAGet[Ghost}Corners() are reversed in the call to VecGetArray2d().
   
   For standard PETSc vectors this is an inexpensive call; it does not copy the vector values.

.keywords: vector, get, array

.seealso: VecGetArray(), VecRestoreArray(), VecGetArrays(), VecGetArrayF90(), VecPlaceArray(),
          VecRestoreArray2d(), DAVecGetarray(), DAVecRestoreArray(), VecGetArray3d(), VecRestoreArray3d(),
          VecGetarray1d(), VecRestoreArray1d(), VecGetArray4d(), VecRestoreArray4d()
@*/
int VecGetArray2d(Vec x,int m,int n,int mstart,int nstart,Scalar **a[])
{
  int    i,ierr,N;
  Scalar *aa;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidPointer(a);
  PetscValidType(x);
  ierr = VecGetLocalSize(x,&N);CHKERRQ(ierr);
  if (m*n != N) SETERRQ3(1,1,"Local array size %d does not match 2d array dimensions %d by %d",N,m,n);
  ierr = VecGetArray(x,&aa);CHKERRQ(ierr);

  *a = (Scalar **)PetscMalloc(m*sizeof(Scalar*));CHKPTRQ(*a);
  for (i=0; i<m; i++) (*a)[i] = aa + i*n -nstart;
  *a -= mstart;
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecRestoreArray2d"
/*@C
   VecRestoreArray2d - Restores a vector after VecGetArray2d() has been called.

   Not Collective

   Input Parameters:
+  x - the vector
.  m - first dimension of two dimensional array
.  n - second dimension of the two dimensional array
.  mstart - first index you will use in first coordinate direction (often 0)
.  nstart - first index in the second coordinate direction (often 0)
-  a - location of pointer to array obtained from VecGetArray2d()

   Level: beginner

   Notes:
   For regular PETSc vectors this routine does not involve any copies. For
   any special vectors that do not store local vector data in a contiguous
   array, this routine will copy the data back into the underlying 
   vector data structure from the array obtained with VecGetArray().

   This routine actually zeros out the a pointer. 

.keywords: vector, restore, array

.seealso: VecGetArray(), VecRestoreArray(), VecRestoreArrays(), VecRestoreArrayF90(), VecPlaceArray(),
          VecGetArray2d(), VecGetArray3d(), VecRestoreArray3d(), DAVecGetArray(), DAVecRestoreArray()
          VecGetarray1d(), VecRestoreArray1d(), VecGetArray4d(), VecRestoreArray4d()
@*/
int VecRestoreArray2d(Vec x,int m,int n,int mstart,int nstart,Scalar **a[])
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidType(x);
  ierr = PetscFree(*a + mstart);CHKERRQ(ierr);
  ierr = VecRestoreArray(x,PETSC_NULL);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecGetArray1d"
/*@C
   VecGetArray1d - Returns a pointer to a 1d contiguous array that contains this 
   processor's portion of the vector data.  You MUST call VecRestoreArray1d() 
   when you no longer need access to the array.

   Not Collective

   Input Parameter:
+  x - the vector
.  m - first dimension of two dimensional array
-  mstart - first index you will use in first coordinate direction (often 0)

   Output Parameter:
.  a - location to put pointer to the array

   Level: beginner

  Notes:
   For a vector obtained from DACreateLocalVector() mstart are likely
   obtained from the corner indices obtained from DAGetGhostCorners() while for
   DACreateGlobalVector() they are the corner indices from DAGetCorners(). 
   
   For standard PETSc vectors this is an inexpensive call; it does not copy the vector values.

.keywords: vector, get, array

.seealso: VecGetArray(), VecRestoreArray(), VecGetArrays(), VecGetArrayF90(), VecPlaceArray(),
          VecRestoreArray2d(), DAVecGetarray(), DAVecRestoreArray(), VecGetArray3d(), VecRestoreArray3d(),
          VecGetarray2d(), VecRestoreArray1d(), VecGetArray4d(), VecRestoreArray4d()
@*/
int VecGetArray1d(Vec x,int m,int mstart,Scalar *a[])
{
  int ierr,N;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidPointer(a);
  PetscValidType(x);
  ierr = VecGetLocalSize(x,&N);CHKERRQ(ierr);
  if (m != N) SETERRQ2(1,1,"Local array size %d does not match 1d array dimensions %d",N,m);
  ierr = VecGetArray(x,a);CHKERRQ(ierr);
  *a  -= mstart;
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"VecRestoreArray1d"
/*@C
   VecRestoreArray1d - Restores a vector after VecGetArray1d() has been called.

   Not Collective

   Input Parameters:
+  x - the vector
.  m - first dimension of two dimensional array
.  mstart - first index you will use in first coordinate direction (often 0)
-  a - location of pointer to array obtained from VecGetArray21()

   Level: beginner

   Notes:
   For regular PETSc vectors this routine does not involve any copies. For
   any special vectors that do not store local vector data in a contiguous
   array, this routine will copy the data back into the underlying 
   vector data structure from the array obtained with VecGetArray1d().

   This routine actually zeros out the a pointer. 

.keywords: vector, restore, array

.seealso: VecGetArray(), VecRestoreArray(), VecRestoreArrays(), VecRestoreArrayF90(), VecPlaceArray(),
          VecGetArray2d(), VecGetArray3d(), VecRestoreArray3d(), DAVecGetArray(), DAVecRestoreArray()
          VecGetarray1d(), VecRestoreArray2d(), VecGetArray4d(), VecRestoreArray4d()
@*/
int VecRestoreArray1d(Vec x,int m,int mstart,Scalar *a[])
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidType(x);
  ierr = VecRestoreArray(x,PETSC_NULL);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
