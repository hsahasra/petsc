static char help[] = "Solves -Laplacian u - exp(u) = 0,  0 < x < 1 using GPU\n\n";
/*
   Same as ex47.c except it also uses the GPU to evaluate the function
*/

#include "petscda.h"
#include "petscsnes.h"
extern PetscErrorCode ComputeFunction(SNES,Vec,Vec,void*), ComputeJacobian(SNES,Vec,Mat*,Mat*,MatStructure*,void*);

int main(int argc,char **argv) 
{
  SNES           snes; 
  Vec            x,f;  
  Mat            J;
  DA             da;
  PetscErrorCode ierr;

  PetscInitialize(&argc,&argv,(char *)0,help);

  ierr = DACreate1d(PETSC_COMM_WORLD,DA_NONPERIODIC,8,1,1,PETSC_NULL,&da);CHKERRQ(ierr);
  ierr = DACreateGlobalVector(da,&x); VecDuplicate(x,&f);CHKERRQ(ierr);
  ierr = DAGetMatrix(da,MATAIJ,&J);CHKERRQ(ierr);

  ierr = SNESCreate(PETSC_COMM_WORLD,&snes);CHKERRQ(ierr);
  ierr = SNESSetFunction(snes,f,ComputeFunction,da);CHKERRQ(ierr);
  ierr = SNESSetJacobian(snes,J,J,ComputeJacobian,da);CHKERRQ(ierr);
  ierr = SNESSetFromOptions(snes);CHKERRQ(ierr);
  ierr = SNESSolve(snes,PETSC_NULL,x);CHKERRQ(ierr);

  ierr = MatDestroy(J);CHKERRQ(ierr);
  ierr = VecDestroy(x);CHKERRQ(ierr);
  ierr = VecDestroy(f);CHKERRQ(ierr);
  ierr = SNESDestroy(snes);CHKERRQ(ierr);
  ierr = DADestroy(da);CHKERRQ(ierr);

  PetscFinalize();
  return 0;
}

PetscErrorCode ComputeFunction(SNES snes,Vec x,Vec f,void *ctx) 
{
  PetscInt       i,Mx,xs,xm;
  PetscScalar    *xx,*ff,hx;
  DA             da = (DA) ctx; 
  Vec            xlocal;
  PetscErrorCode ierr;

  ierr = DAGetInfo(da,PETSC_IGNORE,&Mx,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE);CHKERRQ(ierr);
  hx     = 1.0/(PetscReal)(Mx-1);
  ierr = DAGetLocalVector(da,&xlocal);DAGlobalToLocalBegin(da,x,INSERT_VALUES,xlocal);CHKERRQ(ierr);
  ierr = DAGlobalToLocalEnd(da,x,INSERT_VALUES,xlocal);CHKERRQ(ierr);
  ierr = DAVecGetArray(da,xlocal,&xx); DAVecGetArray(da,f,&ff);CHKERRQ(ierr);
  ierr = DAGetCorners(da,&xs,PETSC_NULL,PETSC_NULL,&xm,PETSC_NULL,PETSC_NULL);CHKERRQ(ierr);

  for (i=xs; i<xs+xm; i++) {
    if (i == 0 || i == Mx-1) ff[i] = xx[i]/hx; 
    else  ff[i] =  (2.0*xx[i] - xx[i-1] - xx[i+1])/hx - hx*PetscExpScalar(xx[i]); 
  }
  ierr = DAVecRestoreArray(da,xlocal,&xx);CHKERRQ(ierr);
  ierr = DARestoreLocalVector(da,&xlocal);CHKERRQ(ierr);
  ierr = DAVecRestoreArray(da,f,&ff);CHKERRQ(ierr);
  return 0;

}
PetscErrorCode ComputeJacobian(SNES snes,Vec x,Mat *J,Mat *B,MatStructure *flag,void *ctx)
{
  DA             da = (DA) ctx; 
  PetscInt       i,Mx,xm,xs; 
  PetscScalar    hx,*xx; 
  Vec            xlocal;
  PetscErrorCode ierr;

  ierr = DAGetInfo(da,PETSC_IGNORE,&Mx,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE);CHKERRQ(ierr);
  hx = 1.0/(PetscReal)(Mx-1);
  ierr = DAGetLocalVector(da,&xlocal);DAGlobalToLocalBegin(da,x,INSERT_VALUES,xlocal);CHKERRQ(ierr);
  ierr = DAGlobalToLocalEnd(da,x,INSERT_VALUES,xlocal);CHKERRQ(ierr);
  ierr = DAVecGetArray(da,xlocal,&xx);CHKERRQ(ierr);
  ierr = DAGetCorners(da,&xs,PETSC_NULL,PETSC_NULL,&xm,PETSC_NULL,PETSC_NULL);CHKERRQ(ierr);

  for (i=xs; i<xs+xm; i++) {
    if (i == 0 || i == Mx-1) { 
      ierr = MatSetValue(*J,i,i,1.0/hx,INSERT_VALUES);CHKERRQ(ierr);
    } else {
      ierr = MatSetValue(*J,i,i-1,-1.0/hx,INSERT_VALUES);CHKERRQ(ierr);
      ierr = MatSetValue(*J,i,i,2.0/hx - hx*PetscExpScalar(xx[i]),INSERT_VALUES);CHKERRQ(ierr);
      ierr = MatSetValue(*J,i,i+1,-1.0/hx,INSERT_VALUES);CHKERRQ(ierr);
    }
  }
  ierr = MatAssemblyBegin(*J,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(*J,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  *flag = SAME_NONZERO_PATTERN;
  ierr = DAVecRestoreArray(da,xlocal,&xx);CHKERRQ(ierr);
  ierr = DARestoreLocalVector(da,&xlocal);CHKERRQ(ierr);
  return 0;}
