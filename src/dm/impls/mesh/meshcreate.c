#define PETSCDM_DLL
#include <private/meshimpl.h>    /*I   "petscdmmesh.h"   I*/
#include <petscdmda.h>

#undef __FUNCT__
#define __FUNCT__ "DMSetFromOptions_Mesh"
PetscErrorCode  DMSetFromOptions_Mesh(DM dm)
{
  //DM_Mesh       *mesh = (DM_Mesh *) dm->data;
  char           typeName[256];
  PetscBool      flg;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  ierr = PetscOptionsBegin(((PetscObject) dm)->comm, ((PetscObject) dm)->prefix, "DMMesh Options", "DMMesh");CHKERRQ(ierr);
    /* Handle DMMesh refinement */
    /* Handle associated vectors */
    if (!VecRegisterAllCalled) {ierr = VecRegisterAll(PETSC_NULL);CHKERRQ(ierr);}
    ierr = PetscOptionsList("-dm_vec_type", "Vector type used for created vectors", "DMSetVecType", VecList, dm->vectype, typeName, 256, &flg);CHKERRQ(ierr);
    if (flg) {
      ierr = DMSetVecType(dm, typeName);CHKERRQ(ierr);
    }
    /* process any options handlers added with PetscObjectAddOptionsHandler() */
    ierr = PetscObjectProcessOptionsHandlers((PetscObject) dm);CHKERRQ(ierr);
  ierr = PetscOptionsEnd();CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* External function declarations here */
extern PetscErrorCode DMGlobalToLocalBegin_Mesh(DM dm, Vec g, InsertMode mode, Vec l);
extern PetscErrorCode DMGlobalToLocalEnd_Mesh(DM dm, Vec g, InsertMode mode, Vec l);
extern PetscErrorCode DMLocalToGlobalBegin_Mesh(DM dm, Vec l, InsertMode mode, Vec g);
extern PetscErrorCode DMLocalToGlobalEnd_Mesh(DM dm, Vec l, InsertMode mode, Vec g);
extern PetscErrorCode DMCreateGlobalVector_Mesh(DM dm, Vec *gvec);
extern PetscErrorCode DMCreateLocalVector_Mesh(DM dm, Vec *lvec);
extern PetscErrorCode DMGetInterpolation_Mesh(DM dmCoarse, DM dmFine, Mat *interpolation, Vec *scaling);
extern PetscErrorCode DMGetMatrix_Mesh(DM dm, const MatType mtype, Mat *J);
extern PetscErrorCode DMRefine_Mesh(DM dm, MPI_Comm comm, DM *dmRefined);
extern PetscErrorCode DMCoarsenHierarchy_Mesh(DM dm, int numLevels, DM *coarseHierarchy);
extern PetscErrorCode DMDestroy_Mesh(DM dm);
extern PetscErrorCode DMView_Mesh(DM dm, PetscViewer viewer);

EXTERN_C_BEGIN
#undef __FUNCT__
#define __FUNCT__ "DMConvert_DA_Mesh"
PetscErrorCode DMConvert_DA_Mesh(DM dm, const DMType newtype, DM *dmNew)
{
  typedef ALE::Mesh<PetscInt,PetscScalar> FlexMesh;
  DM             cda;
  Vec            coordinates;
  PetscScalar   *coords;
  PetscInt       dim, M;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = DMDAGetInfo(dm, &dim, &M, 0,0,0,0,0,0,0,0,0,0,0);CHKERRQ(ierr);
  if (dim > 1) SETERRQ(((PetscObject) dm)->comm, PETSC_ERR_SUP, "Currently, only 1D DMDAs can be converted to DMMeshes.");
  ierr = DMDAGetCoordinateDA(dm, &cda);CHKERRQ(ierr);
  ierr = DMDAGetCoordinates(dm, &coordinates);CHKERRQ(ierr);
  ierr = VecGetArray(coordinates, &coords);CHKERRQ(ierr);

  ierr = DMMeshCreate(((PetscObject) dm)->comm, dmNew);CHKERRQ(ierr);
  ALE::Obj<PETSC_MESH_TYPE>              mesh  = new PETSC_MESH_TYPE(((PetscObject) dm)->comm, dim, 0);
  ALE::Obj<PETSC_MESH_TYPE::sieve_type>  sieve = new PETSC_MESH_TYPE::sieve_type(((PetscObject) dm)->comm, 0);
  ALE::Obj<FlexMesh>                     m     = new FlexMesh(((PetscObject) dm)->comm, dim, 0);
  ALE::Obj<FlexMesh::sieve_type>         s     = new FlexMesh::sieve_type(((PetscObject) dm)->comm, 0);
  PETSC_MESH_TYPE::renumbering_type      renumbering;

  m->setSieve(s);
  {
    /* M edges if its periodic */
    const int             numEdges    = M-1;
    const int             numVertices = M;
    FlexMesh::point_type *vertices    = new FlexMesh::point_type[numVertices];
    const ALE::Obj<FlexMesh::label_type>& markers = m->createLabel("marker");

    if (m->commRank() == 0) {
      /* Create sieve and ordering */
      for(int v = numEdges; v < numEdges+numVertices; v++) {
        vertices[v-numEdges] = FlexMesh::point_type(v);
      }
      for(int e = 0; e < numEdges; ++e) {
        FlexMesh::point_type edge(e);
        int order = 0;

        s->addArrow(vertices[e],                 edge, order++);
        s->addArrow(vertices[(e+1)%numVertices], edge, order++);
      }
    }
    m->stratify();
    delete [] vertices;
    /* Only do this if its not periodic */
    m->setValue(markers, vertices[0], 1);
    m->setValue(markers, vertices[M-1], 2);
  }
  ALE::SieveBuilder<FlexMesh>::buildCoordinates(m, dim, coords);
  mesh->setSieve(sieve);
  ALE::ISieveConverter::convertMesh(*m, *mesh, renumbering, false);
  ierr = VecRestoreArray(coordinates, &coords);CHKERRQ(ierr);
  ierr = DMMeshSetMesh(*dmNew, mesh);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMCreate_Mesh"
PetscErrorCode DMCreate_Mesh(DM dm)
{
  DM_Mesh       *mesh;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  ierr = PetscNewLog(dm, DM_Mesh, &mesh);CHKERRQ(ierr);
  dm->data = mesh;

  new(&mesh->m) ALE::Obj<PETSC_MESH_TYPE>(PETSC_NULL);

  mesh->globalScatter = PETSC_NULL;
  mesh->lf            = PETSC_NULL;
  mesh->lj            = PETSC_NULL;

  ierr = PetscStrallocpy(VECSTANDARD, &dm->vectype);CHKERRQ(ierr);
  dm->ops->globaltolocalbegin = DMGlobalToLocalBegin_Mesh;
  dm->ops->globaltolocalend   = DMGlobalToLocalEnd_Mesh;
  dm->ops->localtoglobalbegin = DMLocalToGlobalBegin_Mesh;
  dm->ops->localtoglobalend   = DMLocalToGlobalEnd_Mesh;
  dm->ops->createglobalvector = DMCreateGlobalVector_Mesh;
  dm->ops->createlocalvector  = DMCreateLocalVector_Mesh;
  dm->ops->getinterpolation   = DMGetInterpolation_Mesh;
  dm->ops->getcoloring        = 0;
  dm->ops->getelements        = 0;
  dm->ops->getmatrix          = DMGetMatrix_Mesh;
  dm->ops->refine             = DMRefine_Mesh;
  dm->ops->coarsen            = 0;
  dm->ops->refinehierarchy    = 0;
  dm->ops->coarsenhierarchy   = DMCoarsenHierarchy_Mesh;
  dm->ops->getinjection       = 0;
  dm->ops->getaggregates      = 0;
  dm->ops->destroy            = DMDestroy_Mesh;
  dm->ops->view               = DMView_Mesh;
  dm->ops->setfromoptions     = DMSetFromOptions_Mesh;
  dm->ops->setup              = 0;

  ierr = PetscObjectComposeFunction((PetscObject) dm, "DMConvert_da_mesh_C", "DMConvert_DA_Mesh", (void (*)(void)) DMConvert_DA_Mesh);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__
#define __FUNCT__ "DMMeshCreate"
/*@
  DMMeshCreate - Creates a DMMesh object.

  Collective on MPI_Comm

  Input Parameter:
. comm - The communicator for the DMMesh object

  Output Parameter:
. mesh  - The DMMesh object

  Level: beginner

.keywords: DMMesh, create
@*/
PetscErrorCode  DMMeshCreate(MPI_Comm comm, DM *mesh)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidPointer(mesh,2);
  ierr = DMCreate(comm, mesh);CHKERRQ(ierr);
  ierr = DMSetType(*mesh, DMMESH);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}