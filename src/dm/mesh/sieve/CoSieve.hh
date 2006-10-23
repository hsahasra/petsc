#ifndef included_ALE_CoSieve_hh
#define included_ALE_CoSieve_hh

#ifndef  included_ALE_Sieve_hh
#include <Sieve.hh>
#endif

extern "C" PetscMPIInt Petsc_DelTag(MPI_Comm comm,PetscMPIInt keyval,void* attr_val,void* extra_state);

namespace ALE {
  class ParallelObject {
  protected:
    int         _debug;
    MPI_Comm    _comm;
    int         _commRank;
    int         _commSize;
    std::string _name;
    PetscObject _petscObj;
    void __init(MPI_Comm comm) {
      static PetscCookie objType = -1;
      //const char        *id_name = ALE::getClassName<T>();
      const char        *id_name = "ParallelObject";
      PetscErrorCode     ierr;

      if (objType < 0) {
        ierr = PetscLogClassRegister(&objType, id_name);CHKERROR(ierr, "Error in PetscLogClassRegister");
      }
      this->_comm = comm;
      ierr = MPI_Comm_rank(this->_comm, &this->_commRank); CHKERROR(ierr, "Error in MPI_Comm_rank");
      ierr = MPI_Comm_size(this->_comm, &this->_commSize); CHKERROR(ierr, "Error in MPI_Comm_size");
      ierr = PetscObjectCreateGeneric(this->_comm, objType, id_name, &this->_petscObj);CHKERROR(ierr, "Error in PetscObjectCreate");
      //ALE::restoreClassName<T>(id_name);
    };
  public:
    ParallelObject(MPI_Comm comm = PETSC_COMM_SELF, const int debug = 0) : _debug(debug), _petscObj(NULL) {__init(comm);}
    virtual ~ParallelObject() {
      if (this->_petscObj) {
        PetscErrorCode ierr = PetscObjectDestroy(this->_petscObj); CHKERROR(ierr, "Failed in PetscObjectDestroy");
        this->_petscObj = NULL;
      }
    };
  public:
    int                debug()    const {return this->_debug;};
    void               setDebug(const int debug) {this->_debug = debug;};
    MPI_Comm           comm()     const {return this->_comm;};
    int                commSize() const {return this->_commSize;};
    int                commRank() const {return this->_commRank;}
    PetscObject        petscObj() const {return this->_petscObj;};
    const std::string& getName() const {return this->_name;};
    void               setName(const std::string& name) {this->_name = name;};
  };

  namespace New {
    template<typename Sieve_>
    class SieveBuilder {
    public:
      typedef Sieve_                                       sieve_type;
      typedef std::vector<typename sieve_type::point_type> PointArray;
    public:
      static void buildHexFaces(Obj<sieve_type> sieve, int dim, std::map<int, int*>& curElement, std::map<int,PointArray>& bdVertices, std::map<int,PointArray>& faces, typename sieve_type::point_type& cell) {
        int debug = sieve->debug();

        if (debug > 1) {std::cout << "  Building hex faces for boundary of " << cell << " (size " << bdVertices[dim].size() << "), dim " << dim << std::endl;}
        if (dim > 3) {
          throw ALE::Exception("Cannot do hexes of dimension greater than three");
        } else if (dim > 2) {
          int nodes[24] = {0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 5, 4,
                           1, 2, 6, 5, 2, 3, 7, 6, 3, 0, 4, 7};

          for(int b = 0; b < 6; b++) {
            typename sieve_type::point_type face;

            for(int c = 0; c < 4; c++) {
              bdVertices[dim-1].push_back(bdVertices[dim][nodes[b*4+c]]);
            }
            if (debug > 1) {std::cout << "    boundary hex face " << b << std::endl;}
            buildHexFaces(sieve, dim-1, curElement, bdVertices, faces, face);
            if (debug > 1) {std::cout << "    added face " << face << std::endl;}
            faces[dim].push_back(face);
          }
        } else if (dim > 1) {
          int boundarySize = bdVertices[dim].size();

          for(int b = 0; b < boundarySize; b++) {
            typename sieve_type::point_type face;

            for(int c = 0; c < 2; c++) {
              bdVertices[dim-1].push_back(bdVertices[dim][(b+c)%boundarySize]);
            }
            if (debug > 1) {std::cout << "    boundary point " << bdVertices[dim][b] << std::endl;}
            buildHexFaces(sieve, dim-1, curElement, bdVertices, faces, face);
            if (debug > 1) {std::cout << "    added face " << face << std::endl;}
            faces[dim].push_back(face);
          }
        } else {
          if (debug > 1) {std::cout << "  Just set faces to boundary in 1d" << std::endl;}
          faces[dim].insert(faces[dim].end(), bdVertices[dim].begin(), bdVertices[dim].end());
        }
        if (debug > 1) {
          for(typename PointArray::iterator f_iter = faces[dim].begin(); f_iter != faces[dim].end(); ++f_iter) {
            std::cout << "  face point " << *f_iter << std::endl;
          }
        }
        // We always create the toplevel, so we could short circuit somehow
        // Should not have to loop here since the meet of just 2 boundary elements is an element
        typename PointArray::iterator          f_itor = faces[dim].begin();
        const typename sieve_type::point_type& start  = *f_itor;
        const typename sieve_type::point_type& next   = *(++f_itor);
        Obj<typename sieve_type::supportSet> preElement = sieve->nJoin(start, next, 1);

        if (preElement->size() > 0) {
          cell = *preElement->begin();
          if (debug > 1) {std::cout << "  Found old cell " << cell << std::endl;}
        } else {
          int color = 0;

          cell = typename sieve_type::point_type((*curElement[dim])++);
          for(typename PointArray::iterator f_itor = faces[dim].begin(); f_itor != faces[dim].end(); ++f_itor) {
            sieve->addArrow(*f_itor, cell, color++);
          }
          if (debug > 1) {std::cout << "  Added cell " << cell << " dim " << dim << std::endl;}
        }
      };
      static void buildFaces(Obj<sieve_type> sieve, int dim, std::map<int, int*>& curElement, std::map<int,PointArray>& bdVertices, std::map<int,PointArray>& faces, typename sieve_type::point_type& cell) {
        int debug = sieve->debug();

        if (debug > 1) {
          if (cell >= 0) {
            std::cout << "  Building faces for boundary of " << cell << " (size " << bdVertices[dim].size() << "), dim " << dim << std::endl;
          } else {
            std::cout << "  Building faces for boundary of undetermined cell (size " << bdVertices[dim].size() << "), dim " << dim << std::endl;
          }
        }
        faces[dim].clear();
        if (dim > 1) {
          // Use the cone construction
          for(typename PointArray::iterator b_itor = bdVertices[dim].begin(); b_itor != bdVertices[dim].end(); ++b_itor) {
            typename sieve_type::point_type face   = -1;

            bdVertices[dim-1].clear();
            for(typename PointArray::iterator i_itor = bdVertices[dim].begin(); i_itor != bdVertices[dim].end(); ++i_itor) {
              if (i_itor != b_itor) {
                bdVertices[dim-1].push_back(*i_itor);
              }
            }
            if (debug > 1) {std::cout << "    boundary point " << *b_itor << std::endl;}
            buildFaces(sieve, dim-1, curElement, bdVertices, faces, face);
            if (debug > 1) {std::cout << "    added face " << face << std::endl;}
            faces[dim].push_back(face);
          }
        } else {
          if (debug > 1) {std::cout << "  Just set faces to boundary in 1d" << std::endl;}
          faces[dim].insert(faces[dim].end(), bdVertices[dim].begin(), bdVertices[dim].end());
        }
        if (debug > 1) {
          for(typename PointArray::iterator f_iter = faces[dim].begin(); f_iter != faces[dim].end(); ++f_iter) {
            std::cout << "  face point " << *f_iter << std::endl;
          }
        }
        // We always create the toplevel, so we could short circuit somehow
        // Should not have to loop here since the meet of just 2 boundary elements is an element
        typename PointArray::iterator          f_itor = faces[dim].begin();
        const typename sieve_type::point_type& start  = *f_itor;
        const typename sieve_type::point_type& next   = *(++f_itor);
        Obj<typename sieve_type::supportSet> preElement = sieve->nJoin(start, next, 1);

        if (preElement->size() > 0) {
          cell = *preElement->begin();
          if (debug > 1) {std::cout << "  Found old cell " << cell << std::endl;}
        } else {
          int color = 0;

          cell = typename sieve_type::point_type((*curElement[dim])++);
          for(typename PointArray::iterator f_itor = faces[dim].begin(); f_itor != faces[dim].end(); ++f_itor) {
            sieve->addArrow(*f_itor, cell, color++);
          }
          if (debug > 1) {std::cout << "  Added cell " << cell << " dim " << dim << std::endl;}
        }
      };

      #undef __FUNCT__
      #define __FUNCT__ "buildTopology"
      // Build a topology from a connectivity description
      //   (0, 0)        ... (0, numCells-1):  dim-dimensional cells
      //   (0, numCells) ... (0, numVertices): vertices
      // The other cells are numbered as they are requested
      static void buildTopology(Obj<sieve_type> sieve, int dim, int numCells, int cells[], int numVertices, bool interpolate = true, int corners = -1, int firstVertex = -1) {
        int debug = sieve->debug();

        ALE_LOG_EVENT_BEGIN;
        if (sieve->commRank() != 0) {
          ALE_LOG_EVENT_END;
          return;
        }
        if (firstVertex < 0) firstVertex = numCells;
        // Create a map from dimension to the current element number for that dimension
        std::map<int,int*>       curElement;
        std::map<int,PointArray> bdVertices;
        std::map<int,PointArray> faces;
        int                      curCell    = 0;
        int                      curVertex  = firstVertex;
        int                      newElement = firstVertex+numVertices;

        if (corners < 0) corners = dim+1;
        curElement[0]   = &curVertex;
        curElement[dim] = &curCell;
        for(int d = 1; d < dim; d++) {
          curElement[d] = &newElement;
        }
        for(int c = 0; c < numCells; c++) {
          typename sieve_type::point_type cell(c);

          // Build the cell
          if (interpolate) {
            bdVertices[dim].clear();
            for(int b = 0; b < corners; b++) {
              typename sieve_type::point_type vertex(cells[c*corners+b]+firstVertex);

              if (debug > 1) {std::cout << "Adding boundary vertex " << vertex << std::endl;}
              bdVertices[dim].push_back(vertex);
            }
            if (debug) {std::cout << "cell " << cell << " num boundary vertices " << bdVertices[dim].size() << std::endl;}

            if (corners != dim+1) {
              buildHexFaces(sieve, dim, curElement, bdVertices, faces, cell);
            } else {
              buildFaces(sieve, dim, curElement, bdVertices, faces, cell);
            }
          } else {
            for(int b = 0; b < corners; b++) {
              sieve->addArrow(typename sieve_type::point_type(cells[c*corners+b]+firstVertex), cell, b);
            }
            if (debug) {
              if (debug > 1) {
                for(int b = 0; b < corners; b++) {
                  std::cout << "  Adding vertex " << typename sieve_type::point_type(cells[c*corners+b]+firstVertex) << std::endl;
                }
              }
              std::cout << "Adding cell " << cell << " dim " << dim << std::endl;
            }
          }
        }
        ALE_LOG_EVENT_END;
      };
      template<typename Section>
      static void buildCoordinates(const Obj<Section>& coords, const int embedDim, const double coordinates[]) {
        const typename Section::patch_type                          patch    = 0;
        const Obj<typename Section::topology_type::label_sequence>& vertices = coords->getTopology()->depthStratum(patch, 0);
        const int numCells = coords->getTopology()->heightStratum(patch, 0)->size();

        coords->setName("coordinates");
        coords->setFiberDimensionByDepth(patch, 0, embedDim);
        coords->allocate();
        for(typename Section::topology_type::label_sequence::iterator v_iter = vertices->begin(); v_iter != vertices->end(); ++v_iter) {
          coords->update(patch, *v_iter, &(coordinates[(*v_iter - numCells)*embedDim]));
        }
      };
    };

    // A Topology is a collection of Sieves
    //   Each Sieve has a label, which we call a \emph{patch}
    //   The collection itself we call a \emph{sheaf}
    //   The main operation we provide in Topology is the creation of a \emph{label}
    //     A label is a bidirectional mapping of Sieve points to integers, implemented with a Sifter
    template<typename Patch_, typename Sieve_>
    class Topology : public ALE::ParallelObject {
    public:
      typedef Patch_                                                patch_type;
      typedef Sieve_                                                sieve_type;
      typedef typename sieve_type::point_type                       point_type;
      typedef typename std::map<patch_type, Obj<sieve_type> >       sheaf_type;
      typedef typename ALE::Sifter<int, point_type, int>            patch_label_type;
      typedef typename std::map<patch_type, Obj<patch_label_type> > label_type;
      typedef typename std::map<patch_type, int>                    max_label_type;
      typedef typename std::map<const std::string, label_type>      labels_type;
      typedef typename patch_label_type::supportSequence            label_sequence;
      typedef typename std::set<point_type>                         point_set_type;
      typedef typename ALE::Sifter<int,point_type,point_type>       send_overlap_type;
      typedef typename ALE::Sifter<point_type,int,point_type>       recv_overlap_type;
    protected:
      sheaf_type     _sheaf;
      labels_type    _labels;
      int            _maxHeight;
      max_label_type _maxHeights;
      int            _maxDepth;
      max_label_type _maxDepths;
      bool           _calculatedOverlap;
      Obj<send_overlap_type> _sendOverlap;
      Obj<recv_overlap_type> _recvOverlap;
      Obj<send_overlap_type> _distSendOverlap;
      Obj<recv_overlap_type> _distRecvOverlap;
      // Work space
      Obj<point_set_type>    _modifiedPoints;
    public:
      Topology(MPI_Comm comm, const int debug = 0) : ParallelObject(comm, debug), _maxHeight(-1), _maxDepth(-1), _calculatedOverlap(false) {
        this->_sendOverlap    = new send_overlap_type(this->comm(), this->debug());
        this->_recvOverlap    = new recv_overlap_type(this->comm(), this->debug());
        this->_modifiedPoints = new point_set_type();
      };
    public: // Verifiers
      void checkPatch(const patch_type& patch) {
        if (this->_sheaf.find(patch) == this->_sheaf.end()) {
          ostringstream msg;
          msg << "Invalid topology patch: " << patch << std::endl;
          throw ALE::Exception(msg.str().c_str());
        }
      };
      void checkLabel(const std::string& name, const patch_type& patch) {
        this->checkPatch(patch);
        if ((this->_labels.find(name) == this->_labels.end()) || (this->_labels[name].find(patch) == this->_labels[name].end())) {
          ostringstream msg;
          msg << "Invalid label name: " << name << " for patch " << patch << std::endl;
          throw ALE::Exception(msg.str().c_str());
        }
      };
      bool hasPatch(const patch_type& patch) {
        if (this->_sheaf.find(patch) != this->_sheaf.end()) {
          return true;
        }
        return false;
      };
      bool hasLabel(const std::string& name, const patch_type& patch) {
        if ((this->_labels.find(name) != this->_labels.end()) && (this->_labels[name].find(patch) != this->_labels[name].end())) {
          return true;
        }
        return false;
      };
    public: // Accessors
      const Obj<sieve_type>& getPatch(const patch_type& patch) {
        this->checkPatch(patch);
        return this->_sheaf[patch];
      };
      void setPatch(const patch_type& patch, const Obj<sieve_type>& sieve) {
        this->_sheaf[patch] = sieve;
      };
      int getValue (const Obj<patch_label_type>& label, const point_type& point, const int defValue = 0) {
        const Obj<typename patch_label_type::coneSequence>& cone = label->cone(point);

        if (cone->size() == 0) return defValue;
        return *cone->begin();
      };
      template<typename InputPoints>
      int getMaxValue (const Obj<patch_label_type>& label, const Obj<InputPoints>& points, const int defValue = 0) {
        int maxValue = defValue;

        for(typename InputPoints::iterator p_iter = points->begin(); p_iter != points->end(); ++p_iter) {
          maxValue = std::max(maxValue, this->getValue(label, *p_iter, defValue));
        }
        return maxValue;
      };
      void setValue(const Obj<patch_label_type>& label, const point_type& point, const int value) {
        label->setCone(value, point);
      };
      const Obj<patch_label_type>& createLabel(const patch_type& patch, const std::string& name) {
        this->checkPatch(patch);
        this->_labels[name][patch] = new patch_label_type(this->comm(), this->debug());
        return this->_labels[name][patch];
      };
      const Obj<patch_label_type>& getLabel(const patch_type& patch, const std::string& name) {
        this->checkLabel(name, patch);
        return this->_labels[name][patch];
      };
      const Obj<label_sequence>& getLabelStratum(const patch_type& patch, const std::string& name, int label) {
        this->checkLabel(name, patch);
        return this->_labels[name][patch]->support(label);
      };
      const sheaf_type& getPatches() {
        return this->_sheaf;
      };
      const labels_type& getLabels() {
        return this->_labels;
      };
      void clear() {
        this->_sheaf.clear();
        this->_labels.clear();
        this->_maxHeight = -1;
        this->_maxHeights.clear();
        this->_maxDepth = -1;
        this->_maxDepths.clear();
      };
      const Obj<send_overlap_type>& getSendOverlap() const {return this->_sendOverlap;};
      void setSendOverlap(const Obj<send_overlap_type>& overlap) {this->_sendOverlap = overlap;};
      const Obj<recv_overlap_type>& getRecvOverlap() const {return this->_recvOverlap;};
      void setRecvOverlap(const Obj<recv_overlap_type>& overlap) {this->_recvOverlap = overlap;};
      const Obj<send_overlap_type>& getDistSendOverlap() const {return this->_distSendOverlap;};
      void setDistSendOverlap(const Obj<send_overlap_type>& overlap) {this->_distSendOverlap = overlap;};
      const Obj<recv_overlap_type>& getDistRecvOverlap() const {return this->_distRecvOverlap;};
      void setDistRecvOverlap(const Obj<recv_overlap_type>& overlap) {this->_distRecvOverlap = overlap;};
    public:
      template<class InputPoints>
      void computeHeight(const Obj<patch_label_type>& height, const Obj<sieve_type>& sieve, const Obj<InputPoints>& points, int& maxHeight) {
        this->_modifiedPoints->clear();

        for(typename InputPoints::iterator p_iter = points->begin(); p_iter != points->end(); ++p_iter) {
          // Compute the max height of the points in the support of p, and add 1
          int h0 = this->getValue(height, *p_iter, -1);
          int h1 = this->getMaxValue(height, sieve->support(*p_iter), -1) + 1;

          if(h1 != h0) {
            this->setValue(height, *p_iter, h1);
            if (h1 > maxHeight) maxHeight = h1;
            this->_modifiedPoints->insert(*p_iter);
          }
        }
        // FIX: We would like to avoid the copy here with cone()
        if(this->_modifiedPoints->size() > 0) {
          this->computeHeight(height, sieve, sieve->cone(this->_modifiedPoints), maxHeight);
        }
      };
      void computeHeights() {
        const std::string name("height");

        this->_maxHeight = -1;
        for(typename sheaf_type::iterator s_iter = this->_sheaf.begin(); s_iter != this->_sheaf.end(); ++s_iter) {
          const Obj<patch_label_type>& label = this->createLabel(s_iter->first, name);

          this->_maxHeights[s_iter->first] = -1;
          this->computeHeight(label, s_iter->second, s_iter->second->leaves(), this->_maxHeights[s_iter->first]);
          if (this->_maxHeights[s_iter->first] > this->_maxHeight) this->_maxHeight = this->_maxHeights[s_iter->first];
        }
      };
      int height() const {return this->_maxHeight;};
      int height(const patch_type& patch) {
        this->checkPatch(patch);
        return this->_maxHeights[patch];
      };
      int height(const patch_type& patch, const point_type& point) {
        return this->getValue(this->_labels["height"][patch], point, -1);
      };
      const Obj<label_sequence>& heightStratum(const patch_type& patch, int height) {
        return this->getLabelStratum(patch, "height", height);
      };
      template<class InputPoints>
      void computeDepth(const Obj<patch_label_type>& depth, const Obj<sieve_type>& sieve, const Obj<InputPoints>& points, int& maxDepth) {
        this->_modifiedPoints->clear();

        for(typename InputPoints::iterator p_iter = points->begin(); p_iter != points->end(); ++p_iter) {
          // Compute the max depth of the points in the cone of p, and add 1
          int d0 = this->getValue(depth, *p_iter, -1);
          int d1 = this->getMaxValue(depth, sieve->cone(*p_iter), -1) + 1;

          if(d1 != d0) {
            this->setValue(depth, *p_iter, d1);
            if (d1 > maxDepth) maxDepth = d1;
            this->_modifiedPoints->insert(*p_iter);
          }
        }
        // FIX: We would like to avoid the copy here with support()
        if(this->_modifiedPoints->size() > 0) {
          this->computeDepth(depth, sieve, sieve->support(this->_modifiedPoints), maxDepth);
        }
      };
      void computeDepths() {
        const std::string name("depth");

        this->_maxDepth = -1;
        for(typename sheaf_type::iterator s_iter = this->_sheaf.begin(); s_iter != this->_sheaf.end(); ++s_iter) {
          const Obj<patch_label_type>& label = this->createLabel(s_iter->first, name);

          this->_maxDepths[s_iter->first] = -1;
          this->computeDepth(label, s_iter->second, s_iter->second->roots(), this->_maxDepths[s_iter->first]);
          if (this->_maxDepths[s_iter->first] > this->_maxDepth) this->_maxDepth = this->_maxDepths[s_iter->first];
        }
      };
      int depth() const {return this->_maxDepth;};
      int depth(const patch_type& patch) {
        this->checkPatch(patch);
        return this->_maxDepths[patch];
      };
      int depth(const patch_type& patch, const point_type& point) {
        return this->getValue(this->_labels["depth"][patch], point, -1);
      };
      const Obj<label_sequence>& depthStratum(const patch_type& patch, int depth) {
        return this->getLabelStratum(patch, "depth", depth);
      };
      #undef __FUNCT__
      #define __FUNCT__ "Topology::stratify"
      void stratify() {
        ALE_LOG_EVENT_BEGIN;
        this->computeHeights();
        this->computeDepths();
        ALE_LOG_EVENT_END;
      };
    public: // Viewers
      void view(const std::string& name, MPI_Comm comm = MPI_COMM_NULL) {
        if (comm == MPI_COMM_NULL) {
          comm = this->comm();
        }
        if (name == "") {
          PetscPrintf(comm, "viewing a Topology\n");
        } else {
          PetscPrintf(comm, "viewing Topology '%s'\n", name.c_str());
        }
        PetscPrintf(comm, "  maximum height %d maximum depth %d\n", this->height(), this->depth());
        for(typename sheaf_type::const_iterator s_iter = this->_sheaf.begin(); s_iter != this->_sheaf.end(); ++s_iter) {
          ostringstream txt;

          txt << "Patch " << s_iter->first;
          s_iter->second->view(txt.str().c_str(), comm);
          PetscPrintf(comm, "  maximum height %d maximum depth %d\n", this->height(s_iter->first), this->depth(s_iter->first));
        }
        for(typename labels_type::const_iterator l_iter = this->_labels.begin(); l_iter != this->_labels.end(); ++l_iter) {
          PetscPrintf(comm, "  label %s constructed\n", l_iter->first.c_str());
        }
      };
    public:
      void constructOverlap(const patch_type& patch) {
        if (this->_calculatedOverlap) return;
        this->constructOverlap(this->getPatch(patch)->base(), this->_sendOverlap, this->_recvOverlap);
        this->constructOverlap(this->getPatch(patch)->cap(), this->_sendOverlap, this->_recvOverlap);
        if (this->debug()) {
          this->_sendOverlap->view("Send overlap");
          this->_recvOverlap->view("Receive overlap");
        }
        this->_calculatedOverlap = true;
      };
      template<typename Sequence>
      void constructOverlap(const Obj<Sequence>& points, const Obj<send_overlap_type>& sendOverlap, const Obj<recv_overlap_type>& recvOverlap) {
        point_type *sendBuf = new point_type[points->size()];
        int         size    = 0;
        for(typename Sequence::iterator l_iter = points->begin(); l_iter != points->end(); ++l_iter) {
          sendBuf[size++] = *l_iter;
        }
        int *sizes   = new int[this->commSize()];   // The number of points coming from each process
        int *offsets = new int[this->commSize()+1]; // Prefix sums for sizes
        int *oldOffs = new int[this->commSize()+1]; // Temporary storage
        point_type *remotePoints = NULL;            // The points from each process
        int        *remoteRanks  = NULL;            // The rank and number of overlap points of each process that overlaps another

        // Change to Allgather() for the correct binning algorithm
        MPI_Gather(&size, 1, MPI_INT, sizes, 1, MPI_INT, 0, this->comm());
        if (this->commRank() == 0) {
          offsets[0] = 0;
          for(int p = 1; p <= this->commSize(); p++) {
            offsets[p] = offsets[p-1] + sizes[p-1];
          }
          remotePoints = new point_type[offsets[this->commSize()]];
        }
        MPI_Gatherv(sendBuf, size, MPI_INT, remotePoints, sizes, offsets, MPI_INT, 0, this->comm());
        std::map<int, std::map<int, std::set<point_type> > > overlapInfo; // Maps (p,q) to their set of overlap points

        if (this->commRank() == 0) {
          for(int p = 0; p < this->commSize(); p++) {
            std::sort(&remotePoints[offsets[p]], &remotePoints[offsets[p+1]]);
          }
          for(int p = 0; p <= this->commSize(); p++) {
            oldOffs[p] = offsets[p];
          }
          for(int p = 0; p < this->commSize(); p++) {
            for(int q = p+1; q < this->commSize(); q++) {
              std::set_intersection(&remotePoints[oldOffs[p]], &remotePoints[oldOffs[p+1]],
                                    &remotePoints[oldOffs[q]], &remotePoints[oldOffs[q+1]],
                                    std::insert_iterator<std::set<point_type> >(overlapInfo[p][q], overlapInfo[p][q].begin()));
              overlapInfo[q][p] = overlapInfo[p][q];
            }
            sizes[p]     = overlapInfo[p].size()*2;
            offsets[p+1] = offsets[p] + sizes[p];
          }
          remoteRanks = new int[offsets[this->commSize()]];
          int       k = 0;
          for(int p = 0; p < this->commSize(); p++) {
            for(typename std::map<int, std::set<point_type> >::iterator r_iter = overlapInfo[p].begin(); r_iter != overlapInfo[p].end(); ++r_iter) {
              remoteRanks[k*2]   = r_iter->first;
              remoteRanks[k*2+1] = r_iter->second.size();
              k++;
            }
          }
        }
        int numOverlaps;                          // The number of processes overlapping this process
        MPI_Scatter(sizes, 1, MPI_INT, &numOverlaps, 1, MPI_INT, 0, this->comm());
        int *overlapRanks = new int[numOverlaps]; // The rank and overlap size for each overlapping process
        MPI_Scatterv(remoteRanks, sizes, offsets, MPI_INT, overlapRanks, numOverlaps, MPI_INT, 0, this->comm());
        point_type *sendPoints = NULL;            // The points to send to each process
        if (this->commRank() == 0) {
          for(int p = 0, k = 0; p < this->commSize(); p++) {
            sizes[p] = 0;
            for(int r = 0; r < (int) overlapInfo[p].size(); r++) {
              sizes[p] += remoteRanks[k*2+1];
              k++;
            }
            offsets[p+1] = offsets[p] + sizes[p];
          }
          sendPoints = new point_type[offsets[this->commSize()]];
          for(int p = 0, k = 0; p < this->commSize(); p++) {
            for(typename std::map<int, std::set<point_type> >::iterator r_iter = overlapInfo[p].begin(); r_iter != overlapInfo[p].end(); ++r_iter) {
              int rank = r_iter->first;
              for(typename std::set<point_type>::iterator p_iter = (overlapInfo[p][rank]).begin(); p_iter != (overlapInfo[p][rank]).end(); ++p_iter) {
                sendPoints[k++] = *p_iter;
              }
            }
          }
        }
        int numOverlapPoints = 0;
        for(int r = 0; r < numOverlaps/2; r++) {
          numOverlapPoints += overlapRanks[r*2+1];
        }
        point_type *overlapPoints = new point_type[numOverlapPoints];
        MPI_Scatterv(sendPoints, sizes, offsets, MPI_INT, overlapPoints, numOverlapPoints, MPI_INT, 0, this->comm());

        for(int r = 0, k = 0; r < numOverlaps/2; r++) {
          int rank = overlapRanks[r*2];

          for(int p = 0; p < overlapRanks[r*2+1]; p++) {
            point_type point = overlapPoints[k++];

            sendOverlap->addArrow(point, rank, point);
            recvOverlap->addArrow(rank, point, point);
          }
        }

        delete [] overlapPoints;
        delete [] overlapRanks;
        delete [] sizes;
        delete [] offsets;
        delete [] oldOffs;
        if (this->commRank() == 0) {
          delete [] remoteRanks;
          delete [] remotePoints;
          delete [] sendPoints;
        }
      };
    };

    // An AbstractSection is a mapping from Sieve points to sets of values
    //   This is our presentation of a section of a fibre bundle,
    //     in which the Topology is the base space, and
    //     the value sets are vectors in the fiber spaces
    //   The main interface to values is through restrict() and update()
    //     This retrieval uses Sieve closure()
    //     We should call these rawRestrict/rawUpdate
    //   The Section must also be able to set/report the dimension of each fiber
    //     for which we use another section we call an \emph{Atlas}
    //     For some storage schemes, we also want offsets to go with these dimensions
    //   We must have some way of limiting the points associated with values
    //     so each section must support a getPatch() call returning the points with associated fibers
    //     I was using the Chart for this
    //   The Section must be able to participate in \emph{completion}
    //     This means restrict to a provided overlap, and exchange in the restricted sections
    //     Completion does not use hierarchy, so we see the Topology as a DiscreteTopology

    // A ConstantSection is the simplest Section
    //   All fibers are dimension 1
    //   All values are equal to a constant
    //     We need no value storage and no communication for completion
    template<typename Topology_, typename Value_>
    class NewConstantSection : public ALE::ParallelObject {
    public:
      typedef Topology_                          topology_type;
      typedef typename topology_type::patch_type patch_type;
      typedef typename topology_type::sieve_type sieve_type;
      typedef typename topology_type::point_type point_type;
      typedef std::set<point_type>               chart_type;
      typedef std::map<patch_type, chart_type>   atlas_type;
      typedef Value_                             value_type;
    protected:
      Obj<topology_type> _topology;
      atlas_type         _atlas;
      chart_type         _emptyChart;
      value_type         _value;
      value_type         _defaultValue;
    public:
      NewConstantSection(MPI_Comm comm, const int debug = 0) : ParallelObject(comm, debug), _defaultValue(0) {
        this->_topology = new topology_type(comm, debug);
      };
      NewConstantSection(const Obj<topology_type>& topology) : ParallelObject(topology->comm(), topology->debug()), _topology(topology) {};
      NewConstantSection(const Obj<topology_type>& topology, const value_type& value) : ParallelObject(topology->comm(), topology->debug()), _topology(topology), _value(value), _defaultValue(value) {};
      NewConstantSection(const Obj<topology_type>& topology, const value_type& value, const value_type& defaultValue) : ParallelObject(topology->comm(), topology->debug()), _topology(topology), _value(value), _defaultValue(defaultValue) {};
    public: // Verifiers
      void checkPatch(const patch_type& patch) const {
        this->_topology->checkPatch(patch);
        if (this->_atlas.find(patch) == this->_atlas.end()) {
          ostringstream msg;
          msg << "Invalid atlas patch " << patch << std::endl;
          throw ALE::Exception(msg.str().c_str());
        }
      };
      void checkPoint(const patch_type& patch, const point_type& point) const {
        this->checkPatch(patch);
        if (this->_atlas.find(patch)->second.find(point) == this->_atlas.find(patch)->second.end()) {
          ostringstream msg;
          msg << "Invalid section point " << point << std::endl;
          throw ALE::Exception(msg.str().c_str());
        }
      };
      void checkDimension(const int& dim) {
        if (dim != 1) {
          ostringstream msg;
          msg << "Invalid fiber dimension " << dim << " must be 1" << std::endl;
          throw ALE::Exception(msg.str().c_str());
        }
      };
      bool hasPatch(const patch_type& patch) {
        if (this->_atlas.find(patch) == this->_atlas.end()) {
          return false;
        }
        return true;
      };
      bool hasPoint(const patch_type& patch, const point_type& point) const {
        this->checkPatch(patch);
        return this->_atlas.find(patch)->second.count(point) > 0;
      };
    public: // Accessors
      const Obj<topology_type>& getTopology() const {return this->_topology;};
      void setTopology(const Obj<topology_type>& topology) {this->_topology = topology;};
      const chart_type& getPatch(const patch_type& patch) {
        if (this->hasPatch(patch)) {
          return this->_atlas[patch];
        }
        return this->_emptyChart;
      };
      void updatePatch(const patch_type& patch, const point_type& point) {
        this->_atlas[patch].insert(point);
      };
      template<typename Points>
      void updatePatch(const patch_type& patch, const Obj<Points>& points) {
        this->_atlas[patch].insert(points->begin(), points->end());
      };
      value_type getDefaultValue() {return this->_defaultValue;};
      void setDefaultValue(const value_type value) {this->_defaultValue = value;};
    public: // Sizes
      void clear() {
        this->_atlas.clear(); 
      };
      int getFiberDimension(const patch_type& patch, const point_type& p) const {
        if (this->hasPoint(patch, p)) return 1;
        return 0;
      };
      void setFiberDimension(const patch_type& patch, const point_type& p, int dim) {
        this->checkDimension(dim);
        this->updatePatch(patch, p);
      };
      template<typename Sequence>
      void setFiberDimension(const patch_type& patch, const Obj<Sequence>& points, int dim) {
        for(typename topology_type::label_sequence::iterator p_iter = points->begin(); p_iter != points->end(); ++p_iter) {
          this->setFiberDimension(patch, *p_iter, dim);
        }
      };
      void addFiberDimension(const patch_type& patch, const point_type& p, int dim) {
        if (this->hasPatch(patch) && (this->_atlas[patch].find(p) != this->_atlas[patch].end())) {
          ostringstream msg;
          msg << "Invalid addition to fiber dimension " << dim << " cannot exceed 1" << std::endl;
          throw ALE::Exception(msg.str().c_str());
        } else {
          this->setFiberDimension(patch, p, dim);
        }
      };
      void setFiberDimensionByDepth(const patch_type& patch, int depth, int dim) {
        this->setFiberDimension(patch, this->_topology->getLabelStratum(patch, "depth", depth), dim);
      };
      void setFiberDimensionByHeight(const patch_type& patch, int height, int dim) {
        this->setFiberDimension(patch, this->_topology->getLabelStratum(patch, "height", height), dim);
      };
      int size(const patch_type& patch) {return this->_atlas[patch].size();};
      int size(const patch_type& patch, const point_type& p) {return this->getFiberDimension(patch, p);};
    public: // Restriction
      const value_type *restrict(const patch_type& patch, const point_type& p) const {
        //std::cout <<"["<<this->commRank()<<"]: Constant restrict ("<<patch<<","<<p<<") from " << std::endl;
        //for(typename chart_type::iterator c_iter = this->_atlas.find(patch)->second.begin(); c_iter != this->_atlas.find(patch)->second.end(); ++c_iter) {
        //  std::cout <<"["<<this->commRank()<<"]:   point " << *c_iter << std::endl;
        //}
        if (this->hasPoint(patch, p)) {
          return &this->_value;
        }
        return &this->_defaultValue;
      };
      const value_type *restrictPoint(const patch_type& patch, const point_type& p) const {return this->restrict(patch, p);};
      void update(const patch_type& patch, const point_type& p, const value_type v[]) {
        this->checkPatch(patch);
        this->_value = v[0];
      };
      void updatePoint(const patch_type& patch, const point_type& p, const value_type v[]) {return this->update(patch, p, v);};
      void updateAdd(const patch_type& patch, const point_type& p, const value_type v[]) {
        this->checkPatch(patch);
        this->_value += v[0];
      };
      void updateAddPoint(const patch_type& patch, const point_type& p, const value_type v[]) {return this->updateAdd(patch, p, v);};
    public:
      void copy(const Obj<NewConstantSection>& section) {
        const typename topology_type::sheaf_type& patches = this->_topology->getPatches();

        for(typename topology_type::sheaf_type::const_iterator p_iter = patches.begin(); p_iter != patches.end(); ++p_iter) {
          const patch_type& patch = p_iter->first;
          if (!section->hasPatch(patch)) continue;
          const chart_type& chart = section->getPatch(patch);

          for(typename chart_type::iterator c_iter = chart.begin(); c_iter != chart.end(); ++c_iter) {
            this->updatePatch(patch, *c_iter);
          }
          this->_value = section->restrict(patch, *chart.begin())[0];
        }
      };
      void view(const std::string& name, MPI_Comm comm = MPI_COMM_NULL) const {
        ostringstream txt;
        int rank;

        if (comm == MPI_COMM_NULL) {
          comm = this->comm();
          rank = this->commRank();
        } else {
          MPI_Comm_rank(comm, &rank);
        }
        if (name == "") {
          if(rank == 0) {
            txt << "viewing a ConstantSection" << std::endl;
          }
        } else {
          if(rank == 0) {
            txt << "viewing ConstantSection '" << name << "'" << std::endl;
          }
        }
        const typename topology_type::sheaf_type& sheaf = this->_topology->getPatches();

        for(typename topology_type::sheaf_type::const_iterator p_iter = sheaf.begin(); p_iter != sheaf.end(); ++p_iter) {
          txt <<"["<<this->commRank()<<"]: Patch " << p_iter->first << std::endl;
          txt <<"["<<this->commRank()<<"]:   Value " << this->_value << std::endl;
        }
        PetscSynchronizedPrintf(comm, txt.str().c_str());
        PetscSynchronizedFlush(comm);
      };
    };

    // A UniformSection often acts as an Atlas
    //   All fibers are the same dimension
    //     Note we can use a ConstantSection for this Atlas
    //   Each point may have a different vector
    //     Thus we need storage for values, and hence must implement completion
    template<typename Topology_, typename Value_, int fiberDim = 1>
    class UniformSection : public ALE::ParallelObject {
    public:
      typedef Topology_                              topology_type;
      typedef typename topology_type::patch_type     patch_type;
      typedef typename topology_type::sieve_type     sieve_type;
      typedef typename topology_type::point_type     point_type;
      typedef NewConstantSection<topology_type, int> atlas_type;
      typedef typename atlas_type::chart_type        chart_type;
      typedef Value_                                 value_type;
      typedef struct {value_type v[fiberDim];}       fiber_type;
      typedef std::map<point_type, fiber_type>       array_type;
      typedef std::map<patch_type, array_type>       values_type;
    protected:
      Obj<atlas_type> _atlas;
      values_type     _arrays;
    public:
      UniformSection(MPI_Comm comm, const int debug = 0) : ParallelObject(comm, debug) {
        this->_atlas = new atlas_type(comm, debug);
      };
      UniformSection(const Obj<topology_type>& topology) : ParallelObject(topology->comm(), topology->debug()) {
        this->_atlas = new atlas_type(topology, fiberDim, 0);
      };
      UniformSection(const Obj<atlas_type>& atlas) : ParallelObject(atlas->comm(), atlas->debug()), _atlas(atlas) {};
    protected:
      value_type *getRawArray(const int size) {
        static value_type *array   = NULL;
        static int         maxSize = 0;

        if (size > maxSize) {
          maxSize = size;
          if (array) delete [] array;
          array = new value_type[maxSize];
        };
        return array;
      };
    public: // Verifiers
      void checkPatch(const patch_type& patch) {
        this->_atlas->checkPatch(patch);
        if (this->_arrays.find(patch) == this->_arrays.end()) {
          ostringstream msg;
          msg << "Invalid section patch: " << patch << std::endl;
          throw ALE::Exception(msg.str().c_str());
        }
      };
      bool hasPatch(const patch_type& patch) {
        return this->_atlas->hasPatch(patch);
      };
      bool hasPoint(const patch_type& patch, const point_type& point) {
        return this->_atlas->hasPoint(patch, point);
      };
      void checkDimension(const int& dim) {
        if (dim != fiberDim) {
          ostringstream msg;
          msg << "Invalid fiber dimension " << dim << " must be " << fiberDim << std::endl;
          throw ALE::Exception(msg.str().c_str());
        }
      };
    public: // Accessors
      const Obj<atlas_type>& getAtlas() {return this->_atlas;};
      void setAtlas(const Obj<atlas_type>& atlas) {this->_atlas = atlas;};
      const Obj<topology_type>& getTopology() {return this->_atlas->getTopology();};
      void setTopology(const Obj<topology_type>& topology) {this->_atlas->setTopology(topology);};
      const chart_type& getPatch(const patch_type& patch) {
        return this->_atlas->getPatch(patch);
      };
      void updatePatch(const patch_type& patch, const point_type& point) {
        this->setFiberDimension(patch, point, 1);
      };
      template<typename Points>
      void updatePatch(const patch_type& patch, const Obj<Points>& points) {
        for(typename Points::iterator p_iter = points->begin(); p_iter != points->end(); ++p_iter) {
          this->setFiberDimension(patch, *p_iter, 1);
        }
      };
      void copy(const Obj<UniformSection<Topology_, Value_, fiberDim> >& section) {
        this->getAtlas()->copy(section->getAtlas());
        const typename topology_type::sheaf_type& sheaf = section->getTopology()->getPatches();

        for(typename topology_type::sheaf_type::const_iterator s_iter = sheaf.begin(); s_iter != sheaf.end(); ++s_iter) {
          const patch_type& patch = s_iter->first;
          if (!section->hasPatch(patch)) continue;
          const chart_type& chart = section->getPatch(patch);

          for(typename chart_type::const_iterator c_iter = chart.begin(); c_iter != chart.end(); ++c_iter) {
            this->update(s_iter->first, *c_iter, section->restrict(s_iter->first, *c_iter));
          }
        }
      };
    public: // Sizes
      void clear() {
        this->_atlas->clear(); 
        this->_arrays.clear();
      };
      int getFiberDimension(const patch_type& patch, const point_type& p) const {
        // Could check for non-existence here
        return this->_atlas->restrictPoint(patch, p)[0];
      };
      void setFiberDimension(const patch_type& patch, const point_type& p, int dim) {
        this->checkDimension(dim);
        this->_atlas->updatePatch(patch, p);
        this->_atlas->updatePoint(patch, p, &dim);
      };
      template<typename Sequence>
      void setFiberDimension(const patch_type& patch, const Obj<Sequence>& points, int dim) {
        for(typename topology_type::label_sequence::iterator p_iter = points->begin(); p_iter != points->end(); ++p_iter) {
          this->setFiberDimension(patch, *p_iter, dim);
        }
      };
      void addFiberDimension(const patch_type& patch, const point_type& p, int dim) {
        if (this->hasPatch(patch) && (this->_atlas[patch].find(p) != this->_atlas[patch].end())) {
          ostringstream msg;
          msg << "Invalid addition to fiber dimension " << dim << " cannot exceed " << fiberDim << std::endl;
          throw ALE::Exception(msg.str().c_str());
        } else {
          this->setFiberDimension(patch, p, dim);
        }
      };
      void setFiberDimensionByDepth(const patch_type& patch, int depth, int dim) {
        this->setFiberDimension(patch, this->getTopology()->getLabelStratum(patch, "depth", depth), dim);
      };
      void setFiberDimensionByHeight(const patch_type& patch, int height, int dim) {
        this->setFiberDimension(patch, this->getTopology()->getLabelStratum(patch, "height", height), dim);
      };
      int size(const patch_type& patch) {
        const typename atlas_type::chart_type& points = this->_atlas->getPatch(patch);
        int size = 0;

        for(typename atlas_type::chart_type::iterator p_iter = points.begin(); p_iter != points.end(); ++p_iter) {
          size += this->getFiberDimension(patch, *p_iter);
        }
        return size;
      };
      int size(const patch_type& patch, const point_type& p) {
        const typename atlas_type::chart_type&  points  = this->_atlas->getPatch(patch);
        const Obj<typename sieve_type::coneSet> closure = this->getTopology()->getPatch(patch)->closure(p);
        typename sieve_type::coneSet::iterator  end     = closure->end();
        int size = 0;

        for(typename sieve_type::coneSet::iterator c_iter = closure->begin(); c_iter != end; ++c_iter) {
          if (points.count(*c_iter)) {
            size += this->getFiberDimension(patch, *c_iter);
          }
        }
        return size;
      };
      void orderPatches() {};
    public: // Restriction
      const array_type& restrict(const patch_type& patch) {
        this->checkPatch(patch);
        return this->_arrays[patch];
      };
      // Return the values for the closure of this point
      //   use a smart pointer?
      const value_type *restrict(const patch_type& patch, const point_type& p) {
        this->checkPatch(patch);
        const chart_type& chart = this->getPatch(patch);
        array_type& array  = this->_arrays[patch];
        const int   size   = this->size(patch, p);
        value_type *values = this->getRawArray(size);
        int         j      = -1;

        // We could actually ask for the height of the individual point
        if (this->getTopology()->height(patch) < 2) {
          // Only avoids the copy of closure()
          const int& dim = this->_atlas->restrict(patch, p)[0];

          if (chart.count(p)) {
            for(int i = 0; i < dim; ++i) {
              values[++j] = array[p].v[i];
            }
          }
          // Should be closure()
          const Obj<typename sieve_type::coneSequence>& cone = this->getTopology()->getPatch(patch)->cone(p);
          typename sieve_type::coneSequence::iterator   end  = cone->end();

          for(typename sieve_type::coneSequence::iterator p_iter = cone->begin(); p_iter != end; ++p_iter) {
            if (chart.count(*p_iter)) {
              const int& dim = this->_atlas->restrict(patch, *p_iter)[0];

              for(int i = 0; i < dim; ++i) {
                values[++j] = array[*p_iter].v[i];
              }
            }
          }
        } else {
          throw ALE::Exception("Not yet implemented for interpolated sieves");
        }
        if (j != size-1) {
          ostringstream txt;

          txt << "Invalid restrict to point " << p << std::endl;
          txt << "  j " << j << " should be " << (size-1) << std::endl;
          std::cout << txt.str();
          throw ALE::Exception(txt.str().c_str());
        }
        return values;
      };
      void update(const patch_type& patch, const point_type& p, const value_type v[]) {
        this->_atlas->checkPatch(patch);
        const chart_type& chart = this->getPatch(patch);
        array_type& array = this->_arrays[patch];
        int         j     = -1;

        if (this->getTopology()->height(patch) < 2) {
          // Only avoids the copy of closure()
          const int& dim = this->_atlas->restrict(patch, p)[0];

          if (chart.count(p)) {
            for(int i = 0; i < dim; ++i) {
              array[p].v[i] = v[++j];
            }
          }
          // Should be closure()
          const Obj<typename sieve_type::coneSequence>& cone = this->getTopology()->getPatch(patch)->cone(p);

          for(typename sieve_type::coneSequence::iterator p_iter = cone->begin(); p_iter != cone->end(); ++p_iter) {
            if (chart.count(*p_iter)) {
              const int& dim = this->_atlas->restrict(patch, *p_iter)[0];

              for(int i = 0; i < dim; ++i) {
                array[*p_iter].v[i] = v[++j];
              }
            }
          }
        } else {
          throw ALE::Exception("Not yet implemented for interpolated sieves");
        }
      };
      void updateAdd(const patch_type& patch, const point_type& p, const value_type v[]) {
        this->_atlas->checkPatch(patch);
        const chart_type& chart = this->getPatch(patch);
        array_type& array = this->_arrays[patch];
        int         j     = -1;

        if (this->getTopology()->height(patch) < 2) {
          // Only avoids the copy of closure()
          const int& dim = this->_atlas->restrict(patch, p)[0];

          if (chart.count(p)) {
            for(int i = 0; i < dim; ++i) {
              array[p].v[i] += v[++j];
            }
          }
          // Should be closure()
          const Obj<typename sieve_type::coneSequence>& cone = this->getTopology()->getPatch(patch)->cone(p);

          for(typename sieve_type::coneSequence::iterator p_iter = cone->begin(); p_iter != cone->end(); ++p_iter) {
            if (chart.count(*p_iter)) {
              const int& dim = this->_atlas->restrict(patch, *p_iter)[0];

              for(int i = 0; i < dim; ++i) {
                array[*p_iter].v[i] += v[++j];
              }
            }
          }
        } else {
          throw ALE::Exception("Not yet implemented for interpolated sieves");
        }
      };
      // Return only the values associated to this point, not its closure
      const value_type *restrictPoint(const patch_type& patch, const point_type& p) {
        this->checkPatch(patch);
        return this->_arrays[patch][p].v;
      };
      // Update only the values associated to this point, not its closure
      void updatePoint(const patch_type& patch, const point_type& p, const value_type v[]) {
        this->_atlas->checkPatch(patch);
        for(int i = 0; i < fiberDim; ++i) {
          this->_arrays[patch][p].v[i] = v[i];
        }
      };
      // Update only the values associated to this point, not its closure
      void updateAddPoint(const patch_type& patch, const point_type& p, const value_type v[]) {
        this->_atlas->checkPatch(patch);
        for(int i = 0; i < fiberDim; ++i) {
          this->_arrays[patch][p].v[i] += v[i];
        }
      };
    public:
      void view(const std::string& name, MPI_Comm comm = MPI_COMM_NULL) {
        ostringstream txt;
        int rank;

        if (comm == MPI_COMM_NULL) {
          comm = this->comm();
          rank = this->commRank();
        } else {
          MPI_Comm_rank(comm, &rank);
        }
        if (name == "") {
          if(rank == 0) {
            txt << "viewing a UniformSection" << std::endl;
          }
        } else {
          if(rank == 0) {
            txt << "viewing UniformSection '" << name << "'" << std::endl;
          }
        }
        for(typename values_type::const_iterator a_iter = this->_arrays.begin(); a_iter != this->_arrays.end(); ++a_iter) {
          const patch_type& patch = a_iter->first;
          array_type&       array = this->_arrays[patch];

          txt << "[" << this->commRank() << "]: Patch " << patch << std::endl;
          const typename atlas_type::chart_type& chart = this->_atlas->getPatch(patch);

          for(typename atlas_type::chart_type::const_iterator p_iter = chart.begin(); p_iter != chart.end(); ++p_iter) {
            const point_type&                     point = *p_iter;
            const typename atlas_type::value_type dim   = this->_atlas->restrict(patch, point)[0];

            if (dim != 0) {
              txt << "[" << this->commRank() << "]:   " << point << " dim " << dim << "  ";
              for(int i = 0; i < dim; i++) {
                txt << " " << array[point].v[i];
              }
              txt << std::endl;
            }
          }
        }
        PetscSynchronizedPrintf(comm, txt.str().c_str());
        PetscSynchronizedFlush(comm);
      };
    };

    // A Section is our most general construct (more general ones could be envisioned)
    //   The Atlas is a UniformSection of dimension 1 and value type Point
    //     to hold each fiber dimension and offsets into a contiguous patch array
    template<typename Topology_, typename Value_>
    class Section : public ALE::ParallelObject {
    public:
      typedef Topology_                                 topology_type;
      typedef typename topology_type::patch_type        patch_type;
      typedef typename topology_type::sieve_type        sieve_type;
      typedef typename topology_type::point_type        point_type;
      typedef ALE::Point                                index_type;
      typedef UniformSection<topology_type, index_type> atlas_type;
      typedef typename atlas_type::chart_type           chart_type;
      typedef Value_                                    value_type;
      typedef value_type *                              array_type;
      typedef std::map<patch_type, array_type>          values_type;
      typedef std::vector<index_type>                   IndexArray;
    protected:
      Obj<atlas_type> _atlas;
      Obj<atlas_type> _atlasNew;
      values_type     _arrays;
      Obj<IndexArray> _indexArray;
    public:
      Section(MPI_Comm comm, const int debug = 0) : ParallelObject(comm, debug) {
        this->_atlas      = new atlas_type(comm, debug);
        this->_atlasNew   = NULL;
        this->_indexArray = new IndexArray();
      };
      Section(const Obj<topology_type>& topology) : ParallelObject(topology->comm(), topology->debug()), _atlasNew(NULL) {
        this->_atlas      = new atlas_type(topology);
        this->_indexArray = new IndexArray();
      };
      Section(const Obj<atlas_type>& atlas) : ParallelObject(atlas->comm(), atlas->debug()), _atlas(atlas), _atlasNew(NULL) {
        this->_indexArray = new IndexArray();
      };
      virtual ~Section() {
        for(typename values_type::iterator a_iter = this->_arrays.begin(); a_iter != this->_arrays.end(); ++a_iter) {
          delete [] a_iter->second;
          a_iter->second = NULL;
        }
      };
    protected:
      value_type *getRawArray(const int size) {
        static value_type *array   = NULL;
        static int         maxSize = 0;

        if (size > maxSize) {
          maxSize = size;
          if (array) delete [] array;
          array = new value_type[maxSize];
        };
        return array;
      };
    public: // Verifiers
      void checkPatch(const patch_type& patch) {
        this->_atlas->checkPatch(patch);
        if (this->_arrays.find(patch) == this->_arrays.end()) {
          ostringstream msg;
          msg << "Invalid section patch: " << patch << std::endl;
          throw ALE::Exception(msg.str().c_str());
        }
      };
      bool hasPatch(const patch_type& patch) {
        return this->_atlas->hasPatch(patch);
      };
    public: // Accessors
      const Obj<atlas_type>& getAtlas() {return this->_atlas;};
      void setAtlas(const Obj<atlas_type>& atlas) {this->_atlas = atlas;};
      const Obj<topology_type>& getTopology() {return this->_atlas->getTopology();};
      void setTopology(const Obj<topology_type>& topology) {this->_atlas->setTopology(topology);};
      const chart_type& getPatch(const patch_type& patch) {
        return this->_atlas->getPatch(patch);
      };
      bool hasPoint(const patch_type& patch, const point_type& point) {
        return this->_atlas->hasPoint(patch, point);
      };
    public: // Sizes
      void clear() {
        this->_atlas->clear(); 
        this->_arrays.clear();
      };
      int getFiberDimension(const patch_type& patch, const point_type& p) const {
        // Could check for non-existence here
        return this->_atlas->restrictPoint(patch, p)->prefix;
      };
      int getFiberDimension(const Obj<atlas_type>& atlas, const patch_type& patch, const point_type& p) const {
        // Could check for non-existence here
        return atlas->restrictPoint(patch, p)->prefix;
      };
      void setFiberDimension(const patch_type& patch, const point_type& p, int dim) {
        const index_type idx(dim, -1);
        this->_atlas->updatePatch(patch, p);
        this->_atlas->updatePoint(patch, p, &idx);
      };
      template<typename Sequence>
      void setFiberDimension(const patch_type& patch, const Obj<Sequence>& points, int dim) {
        for(typename topology_type::label_sequence::iterator p_iter = points->begin(); p_iter != points->end(); ++p_iter) {
          this->setFiberDimension(patch, *p_iter, dim);
        }
      };
      void addFiberDimension(const patch_type& patch, const point_type& p, int dim) {
        const index_type values(dim, -1);
        this->_atlas->updatePatch(patch, p);
        this->_atlas->updateAddPoint(patch, p, &values);
      };
      void setFiberDimensionByDepth(const patch_type& patch, int depth, int dim) {
        this->setFiberDimension(patch, this->getTopology()->getLabelStratum(patch, "depth", depth), dim);
      };
      void setFiberDimensionByHeight(const patch_type& patch, int height, int dim) {
        this->setFiberDimension(patch, this->getTopology()->getLabelStratum(patch, "height", height), dim);
      };
      int size(const patch_type& patch) {
        const typename atlas_type::chart_type& points = this->_atlas->getPatch(patch);
        int size = 0;

        for(typename atlas_type::chart_type::iterator p_iter = points.begin(); p_iter != points.end(); ++p_iter) {
          size += this->getFiberDimension(patch, *p_iter);
        }
        return size;
      };
      int size(const patch_type& patch, const point_type& p) {
        const typename atlas_type::chart_type&  points  = this->_atlas->getPatch(patch);
        const Obj<typename sieve_type::coneSet> closure = this->getTopology()->getPatch(patch)->closure(p);
        typename sieve_type::coneSet::iterator  end     = closure->end();
        int size = 0;

        for(typename sieve_type::coneSet::iterator c_iter = closure->begin(); c_iter != end; ++c_iter) {
          if (points.count(*c_iter)) {
            size += this->getFiberDimension(patch, *c_iter);
          }
        }
        return size;
      };
      int size(const Obj<atlas_type>& atlas, const patch_type& patch) {
        const typename atlas_type::chart_type& points = atlas->getPatch(patch);
        int size = 0;

        for(typename atlas_type::chart_type::iterator p_iter = points.begin(); p_iter != points.end(); ++p_iter) {
          size += this->getFiberDimension(atlas, patch, *p_iter);
        }
        return size;
      };
    public: // Index retrieval
      const index_type& getIndex(const patch_type& patch, const point_type& p) {
        this->checkPatch(patch);
        return this->_atlas->restrictPoint(patch, p)[0];
      };
      template<typename Numbering>
      const index_type getIndex(const patch_type& patch, const point_type& p, const Obj<Numbering>& numbering) {
        this->checkPatch(patch);
        return index_type(this->getFiberDimension(patch, p), numbering->getIndex(p));
      };
      const Obj<IndexArray>& getIndices(const patch_type& patch, const point_type& p, const int level = -1) {
        this->_indexArray->clear();

        if (level == 0) {
          this->_indexArray->push_back(this->getIndex(patch, p));
        } else if ((level == 1) || (this->getTopology()->height(patch) == 1)) {
          const Obj<typename sieve_type::coneSequence>& cone = this->getTopology()->getPatch(patch)->cone(p);
          typename sieve_type::coneSequence::iterator   end  = cone->end();

          this->_indexArray->push_back(this->getIndex(patch, p));
          for(typename sieve_type::coneSequence::iterator p_iter = cone->begin(); p_iter != end; ++p_iter) {
            this->_indexArray->push_back(this->getIndex(patch, *p_iter));
          }
        } else if (level == -1) {
          const Obj<typename sieve_type::coneSet> closure = this->getTopology()->getPatch(patch)->closure(p);
          typename sieve_type::coneSet::iterator  end     = closure->end();

          for(typename sieve_type::coneSet::iterator p_iter = closure->begin(); p_iter != end; ++p_iter) {
            this->_indexArray->push_back(this->getIndex(patch, *p_iter));
          }
        } else {
          const Obj<typename sieve_type::coneArray> cone = this->getTopology()->getPatch(patch)->nCone(p, level);
          typename sieve_type::coneArray::iterator  end  = cone->end();

          for(typename sieve_type::coneArray::iterator p_iter = cone->begin(); p_iter != end; ++p_iter) {
            this->_indexArray->push_back(this->getIndex(patch, *p_iter));
          }
        }
        return this->_indexArray;
      };
      template<typename Numbering>
      const Obj<IndexArray>& getIndices(const patch_type& patch, const point_type& p, const Obj<Numbering>& numbering, const int level = -1) {
        this->_indexArray->clear();

        if (level == 0) {
          this->_indexArray->push_back(this->getIndex(patch, p, numbering));
        } else if ((level == 1) || (this->getTopology()->height(patch) == 1)) {
          const Obj<typename sieve_type::coneSequence>& cone = this->getTopology()->getPatch(patch)->cone(p);
          typename sieve_type::coneSequence::iterator   end  = cone->end();

          this->_indexArray->push_back(this->getIndex(patch, p, numbering));
          for(typename sieve_type::coneSequence::iterator p_iter = cone->begin(); p_iter != end; ++p_iter) {
            this->_indexArray->push_back(this->getIndex(patch, *p_iter, numbering));
          }
        } else if (level == -1) {
          const Obj<typename sieve_type::coneSet> closure = this->getTopology()->getPatch(patch)->closure(p);
          typename sieve_type::coneSet::iterator  end     = closure->end();

          for(typename sieve_type::coneSet::iterator p_iter = closure->begin(); p_iter != end; ++p_iter) {
            this->_indexArray->push_back(this->getIndex(patch, *p_iter, numbering));
          }
        } else {
          const Obj<typename sieve_type::coneArray> cone = this->getTopology()->getPatch(patch)->nCone(p, level);
          typename sieve_type::coneArray::iterator  end  = cone->end();

          for(typename sieve_type::coneArray::iterator p_iter = cone->begin(); p_iter != end; ++p_iter) {
            this->_indexArray->push_back(this->getIndex(patch, *p_iter, numbering));
          }
        }
        return this->_indexArray;
      };
    public: // Allocation
      void orderPoint(const Obj<atlas_type>& atlas, const Obj<sieve_type>& sieve, const patch_type& patch, const point_type& point, int& offset) {
        const Obj<typename sieve_type::coneSequence>& cone = sieve->cone(point);
        typename sieve_type::coneSequence::iterator   end  = cone->end();
        index_type                                    idx  = atlas->restrictPoint(patch, point)[0];
        const int&                                    dim  = idx.prefix;
        const index_type                              defaultIdx(0, -1);

        if (atlas->getPatch(patch).count(point) == 0) {
          idx = defaultIdx;
        }
        if (idx.index < 0) {
          for(typename sieve_type::coneSequence::iterator c_iter = cone->begin(); c_iter != end; ++c_iter) {
            if (this->_debug > 1) {std::cout << "    Recursing to " << *c_iter << std::endl;}
            this->orderPoint(atlas, sieve, patch, *c_iter, offset);
          }
          if (dim > 0) {
            if (this->_debug > 1) {std::cout << "  Ordering point " << point << " at " << offset << std::endl;}
            idx.index = offset;
            atlas->updatePoint(patch, point, &idx);
            offset += dim;
          }
        }
      }
      void orderPatch(const Obj<atlas_type>& atlas, const patch_type& patch, int& offset) {
        const typename atlas_type::chart_type& chart = atlas->getPatch(patch);

        for(typename atlas_type::chart_type::const_iterator p_iter = chart.begin(); p_iter != chart.end(); ++p_iter) {
          if (this->_debug > 1) {std::cout << "Ordering closure of point " << *p_iter << std::endl;}
          this->orderPoint(atlas, this->getTopology()->getPatch(patch), patch, *p_iter, offset);
        }
      };
      void orderPatches(const Obj<atlas_type>& atlas) {
        const typename topology_type::sheaf_type& patches = this->getTopology()->getPatches();

        for(typename topology_type::sheaf_type::const_iterator p_iter = patches.begin(); p_iter != patches.end(); ++p_iter) {
          if (this->_debug > 1) {std::cout << "Ordering patch " << p_iter->first << std::endl;}
          int offset = 0;

          if (!atlas->hasPatch(p_iter->first)) continue;
          this->orderPatch(atlas, p_iter->first, offset);
        }
      };
      void orderPatches() {
        this->orderPatches(this->_atlas);
      };
      void allocateStorage() {
        const typename topology_type::sheaf_type& patches = this->getTopology()->getPatches();

        for(typename topology_type::sheaf_type::const_iterator p_iter = patches.begin(); p_iter != patches.end(); ++p_iter) {
          if (!this->_atlas->hasPatch(p_iter->first)) continue;
          this->_arrays[p_iter->first] = new value_type[this->size(p_iter->first)];
          PetscMemzero(this->_arrays[p_iter->first], this->size(p_iter->first) * sizeof(value_type));
        }
      };
      void allocate() {
        this->orderPatches();
        this->allocateStorage();
      };
      void addPoint(const patch_type& patch, const point_type& point, const int dim) {
        if (dim == 0) return;
        //const typename atlas_type::chart_type& chart = this->_atlas->getPatch(patch);

        //if (chart.find(point) == chart.end()) {
        if (this->_atlasNew.isNull()) {
          this->_atlasNew = new atlas_type(this->getTopology());
          this->_atlasNew->copy(this->_atlas);
        }
        const index_type idx(dim, -1);
        this->_atlasNew->updatePatch(patch, point);
        this->_atlasNew->update(patch, point, &idx);
      };
      void reallocate() {
        if (this->_atlasNew.isNull()) return;
        const typename topology_type::sheaf_type& patches = this->getTopology()->getPatches();

        // Since copy() preserves offsets, we must reinitialize them before ordering
        for(typename topology_type::sheaf_type::const_iterator p_iter = patches.begin(); p_iter != patches.end(); ++p_iter) {
          const patch_type&                      patch = p_iter->first;
          const typename atlas_type::chart_type& chart = this->_atlasNew->getPatch(patch);
          index_type                             defaultIdx(0, -1);

          for(typename atlas_type::chart_type::const_iterator c_iter = chart.begin(); c_iter != chart.end(); ++c_iter) {
            defaultIdx.prefix = this->_atlasNew->restrict(patch, *c_iter)[0].prefix;
            this->_atlasNew->update(patch, *c_iter, &defaultIdx);
          }
        }
        this->orderPatches(this->_atlasNew);
        // Copy over existing values
        for(typename topology_type::sheaf_type::const_iterator p_iter = patches.begin(); p_iter != patches.end(); ++p_iter) {
          const patch_type&                      patch    = p_iter->first;
          value_type                            *newArray = new value_type[this->size(this->_atlasNew, patch)];

          if (!this->_atlas->hasPatch(patch)) {
            this->_arrays[patch] = newArray;
            continue;
          }
          const typename atlas_type::chart_type& chart    = this->_atlas->getPatch(patch);
          const value_type                      *array    = this->_arrays[patch];

          for(typename atlas_type::chart_type::const_iterator c_iter = chart.begin(); c_iter != chart.end(); ++c_iter) {
            const index_type& idx       = this->_atlas->restrict(patch, *c_iter)[0];
            const int         size      = idx.prefix;
            const int         offset    = idx.index;
            const int&        newOffset = this->_atlasNew->restrict(patch, *c_iter)[0].index;

            for(int i = 0; i < size; ++i) {
              newArray[newOffset+i] = array[offset+i];
            }
          }
          delete [] this->_arrays[patch];
          this->_arrays[patch] = newArray;
        }
        this->_atlas    = this->_atlasNew;
        this->_atlasNew = NULL;
      };
    public: // Restriction
      // Return a pointer to the entire contiguous storage array
      const value_type *restrict(const patch_type& patch) {
        this->checkPatch(patch);
        return this->_arrays[patch];
      };
      // Update the entire contiguous storage array
      void update(const patch_type& patch, const value_type v[]) {
        const value_type *array = this->_arrays[patch];
        const int         size  = this->size(patch);

        for(int i = 0; i < size; i++) {
          array[i] = v[i];
        }
      };
      // Return the values for the closure of this point
      //   use a smart pointer?
      const value_type *restrict(const patch_type& patch, const point_type& p) {
        this->checkPatch(patch);
        const value_type *a      = this->_arrays[patch];
        const int         size   = this->size(patch, p);
        value_type       *values = this->getRawArray(size);
        int               j      = -1;

        if (this->getTopology()->height(patch) < 2) {
          // Avoids the copy of both
          //   points  in topology->closure()
          //   indices in _atlas->restrict()
          const index_type& pInd = this->_atlas->restrictPoint(patch, p)[0];

          for(int i = pInd.index; i < pInd.prefix + pInd.index; ++i) {
            values[++j] = a[i];
          }
          const Obj<typename sieve_type::coneSequence>& cone = this->getTopology()->getPatch(patch)->cone(p);
          typename sieve_type::coneSequence::iterator   end  = cone->end();

          for(typename sieve_type::coneSequence::iterator p_iter = cone->begin(); p_iter != end; ++p_iter) {
            const index_type& ind    = this->_atlas->restrictPoint(patch, *p_iter)[0];
            const int&        start  = ind.index;
            const int&        length = ind.prefix;

            for(int i = start; i < start + length; ++i) {
              values[++j] = a[i];
            }
          }
        } else {
          const Obj<IndexArray>& ind = this->getIndices(patch, p);

          for(typename IndexArray::iterator i_iter = ind->begin(); i_iter != ind->end(); ++i_iter) {
            const int& start  = i_iter->index;
            const int& length = i_iter->prefix;

            for(int i = start; i < start + length; ++i) {
              values[++j] = a[i];
            }
          }
        }
        if (j != size-1) {
          ostringstream txt;

          txt << "Invalid restrict to point " << p << std::endl;
          txt << "  j " << j << " should be " << (size-1) << std::endl;
          std::cout << txt.str();
          throw ALE::Exception(txt.str().c_str());
        }
        return values;
      };
      // Update the values for the closure of this point
      void update(const patch_type& patch, const point_type& p, const value_type v[]) {
        this->checkPatch(patch);
        value_type *a = this->_arrays[patch];
        int         j = -1;

        if (this->getTopology()->height(patch) < 2) {
          // Avoids the copy of both
          //   points  in topology->closure()
          //   indices in _atlas->restrict()
          const index_type& pInd = this->_atlas->restrictPoint(patch, p)[0];

          for(int i = pInd.index; i < pInd.prefix + pInd.index; ++i) {
            a[i] = v[++j];
          }
          const Obj<typename sieve_type::coneSequence>& cone = this->getTopology()->getPatch(patch)->cone(p);
          typename sieve_type::coneSequence::iterator   end  = cone->end();

          for(typename sieve_type::coneSequence::iterator p_iter = cone->begin(); p_iter != end; ++p_iter) {
            const index_type& ind    = this->_atlas->restrictPoint(patch, *p_iter)[0];
            const int&        start  = ind.index;
            const int&        length = ind.prefix;

            for(int i = start; i < start + length; ++i) {
              a[i] = v[++j];
            }
          }
        } else {
          const Obj<IndexArray>& ind = this->getIndices(patch, p);

          for(typename IndexArray::iterator i_iter = ind->begin(); i_iter != ind->end(); ++i_iter) {
            const int& start  = i_iter->index;
            const int& length = i_iter->prefix;

            for(int i = start; i < start + length; ++i) {
              a[i] = v[++j];
            }
          }
        }
      };
      // Update the values for the closure of this point
      void updateAdd(const patch_type& patch, const point_type& p, const value_type v[]) {
        this->checkPatch(patch);
        value_type *a = this->_arrays[patch];
        int         j = -1;

        if (this->getTopology()->height(patch) < 2) {
          // Avoids the copy of both
          //   points  in topology->closure()
          //   indices in _atlas->restrict()
          const index_type& pInd = this->_atlas->restrictPoint(patch, p)[0];

          for(int i = pInd.index; i < pInd.prefix + pInd.index; ++i) {
            a[i] += v[++j];
          }
          const Obj<typename sieve_type::coneSequence>& cone = this->getTopology()->getPatch(patch)->cone(p);
          typename sieve_type::coneSequence::iterator   end  = cone->end();

          for(typename sieve_type::coneSequence::iterator p_iter = cone->begin(); p_iter != end; ++p_iter) {
            const index_type& ind    = this->_atlas->restrictPoint(patch, *p_iter)[0];
            const int&        start  = ind.index;
            const int&        length = ind.prefix;

            for(int i = start; i < start + length; ++i) {
              a[i] += v[++j];
            }
          }
        } else {
          const Obj<IndexArray>& ind = this->getIndices(patch, p);

          for(typename IndexArray::iterator i_iter = ind->begin(); i_iter != ind->end(); ++i_iter) {
            const int& start  = i_iter->index;
            const int& length = i_iter->prefix;

            for(int i = start; i < start + length; ++i) {
              a[i] += v[++j];
            }
          }
        }
      };
      // Update the values for the closure of this point
      template<typename Input>
      void update(const patch_type& patch, const point_type& p, const Obj<Input>& v) {
        this->checkPatch(patch);
        value_type *a = this->_arrays[patch];

        if (this->getTopology()->height(patch) == 1) {
          // Only avoids the copy
          const index_type& pInd = this->_atlas->restrictPoint(patch, p)[0];
          typename Input::iterator v_iter = v->begin();
          typename Input::iterator v_end  = v->end();

          for(int i = pInd.index; i < pInd.prefix + pInd.index; ++i) {
            a[i] = *v_iter;
            ++v_iter;
          }
          const Obj<typename sieve_type::coneSequence>& cone = this->getTopology()->getPatch(patch)->cone(p);
          typename sieve_type::coneSequence::iterator   end  = cone->end();

          for(typename sieve_type::coneSequence::iterator p_iter = cone->begin(); p_iter != end; ++p_iter) {
            const index_type& ind    = this->_atlas->restrictPoint(patch, *p_iter)[0];
            const int&        start  = ind.index;
            const int&        length = ind.prefix;

            for(int i = start; i < start + length; ++i) {
              a[i] = *v_iter;
              ++v_iter;
            }
          }
        } else {
          const Obj<IndexArray>& ind = this->getIndices(patch, p);
          typename Input::iterator v_iter = v->begin();
          typename Input::iterator v_end  = v->end();

          for(typename IndexArray::iterator i_iter = ind->begin(); i_iter != ind->end(); ++i_iter) {
            const int& start  = i_iter->index;
            const int& length = i_iter->prefix;

            for(int i = start; i < start + length; ++i) {
              a[i] = *v_iter;
              ++v_iter;
            }
          }
        }
      };
      // Return only the values associated to this point, not its closure
      const value_type *restrictPoint(const patch_type& patch, const point_type& p) {
        this->checkPatch(patch);
        return &(this->_arrays[patch][this->_atlas->restrictPoint(patch, p)[0].index]);
      };
      // Update only the values associated to this point, not its closure
      void updatePoint(const patch_type& patch, const point_type& p, const value_type v[]) {
        this->checkPatch(patch);
        const index_type& idx = this->_atlas->restrictPoint(patch, p)[0];
        value_type       *a   = &(this->_arrays[patch][idx.index]);

        for(int i = 0; i < idx.prefix; ++i) {
          a[i] = v[i];
        }
      };
      // Update only the values associated to this point, not its closure
      void updateAddPoint(const patch_type& patch, const point_type& p, const value_type v[]) {
        this->checkPatch(patch);
        const index_type& idx = this->_atlas->restrictPoint(patch, p)[0];
        value_type       *a   = &(this->_arrays[patch][idx.index]);

        for(int i = 0; i < idx.prefix; ++i) {
          a[i] += v[i];
        }
      };
    public:
      void view(const std::string& name, MPI_Comm comm = MPI_COMM_NULL) const {
        ostringstream txt;
        int rank;

        if (comm == MPI_COMM_NULL) {
          comm = this->comm();
          rank = this->commRank();
        } else {
          MPI_Comm_rank(comm, &rank);
        }
        if (name == "") {
          if(rank == 0) {
            txt << "viewing a Section" << std::endl;
          }
        } else {
          if(rank == 0) {
            txt << "viewing Section '" << name << "'" << std::endl;
          }
        }
        for(typename values_type::const_iterator a_iter = this->_arrays.begin(); a_iter != this->_arrays.end(); ++a_iter) {
          const patch_type&  patch = a_iter->first;
          const value_type  *array = a_iter->second;

          txt << "[" << this->commRank() << "]: Patch " << patch << std::endl;
          const typename atlas_type::chart_type& chart = this->_atlas->getPatch(patch);

          for(typename atlas_type::chart_type::const_iterator p_iter = chart.begin(); p_iter != chart.end(); ++p_iter) {
            const point_type& point = *p_iter;
            const index_type& idx   = this->_atlas->restrict(patch, point)[0];

            if (idx.prefix != 0) {
              txt << "[" << this->commRank() << "]:   " << point << " dim " << idx.prefix << " offset " << idx.index << "  ";
              for(int i = 0; i < idx.prefix; i++) {
                txt << " " << array[idx.index+i];
              }
              txt << std::endl;
            }
          }
        }
        if (this->_arrays.empty()) {
          txt << "[" << this->commRank() << "]: empty" << std::endl;
        }
        PetscSynchronizedPrintf(comm, txt.str().c_str());
        PetscSynchronizedFlush(comm);
      };
    };

    // An Overlap is a Sifter describing the overlap of two Sieves
    //   Each arrow is local point ---(remote point)---> remote rank right now
    //     For XSifter, this should change to (local patch, local point) ---> (remote rank, remote patch, remote point)

    template<typename Topology_, typename Value_>
    class ConstantSection : public ALE::ParallelObject {
    public:
      typedef Topology_                          topology_type;
      typedef typename topology_type::patch_type patch_type;
      typedef typename topology_type::sieve_type sieve_type;
      typedef typename topology_type::point_type point_type;
      typedef Value_                             value_type;
    protected:
      Obj<topology_type> _topology;
      const value_type   _value;
    public:
      ConstantSection(MPI_Comm comm, const value_type value, const int debug = 0) : ParallelObject(comm, debug), _value(value) {
        this->_topology = new topology_type(comm, debug);
      };
      ConstantSection(const Obj<topology_type>& topology, const value_type value) : ParallelObject(topology->comm(), topology->debug()), _topology(topology), _value(value) {};
      virtual ~ConstantSection() {};
    public:
      bool hasPoint(const patch_type& patch, const point_type& point) const {return true;};
      const value_type *restrict(const patch_type& patch) {return &this->_value;};
      // This should return something the size of the closure
      const value_type *restrict(const patch_type& patch, const point_type& p) {return &this->_value;};
      const value_type *restrictPoint(const patch_type& patch, const point_type& p) {return &this->_value;};
      void update(const patch_type& patch, const point_type& p, const value_type v[]) {
        throw ALE::Exception("Cannot update a ConstantSection");
      };
      void updateAdd(const patch_type& patch, const point_type& p, const value_type v[]) {
        throw ALE::Exception("Cannot update a ConstantSection");
      };
      void updatePoint(const patch_type& patch, const point_type& p, const value_type v[]) {
        throw ALE::Exception("Cannot update a ConstantSection");
      };
      template<typename Input>
      void update(const patch_type& patch, const point_type& p, const Obj<Input>& v) {
        throw ALE::Exception("Cannot update a ConstantSection");
      };
    public:
      void view(const std::string& name, MPI_Comm comm = MPI_COMM_NULL) const {
        ostringstream txt;
        int rank;

        if (comm == MPI_COMM_NULL) {
          comm = this->comm();
          rank = this->commRank();
        } else {
          MPI_Comm_rank(comm, &rank);
        }
        if (name == "") {
          if(rank == 0) {
            txt << "viewing a ConstantSection with value " << this->_value << std::endl;
          }
        } else {
          if(rank == 0) {
            txt << "viewing ConstantSection '" << name << "' with value " << this->_value << std::endl;
          }
        }
        PetscSynchronizedPrintf(comm, txt.str().c_str());
        PetscSynchronizedFlush(comm);
      };
    };

    template<typename Point_>
    class DiscreteSieve {
    public:
      typedef Point_                  point_type;
      typedef std::vector<point_type> coneSequence;
      typedef std::vector<point_type> coneSet;
      typedef std::vector<point_type> coneArray;
      typedef std::vector<point_type> supportSequence;
      typedef std::vector<point_type> supportSet;
      typedef std::vector<point_type> supportArray;
      typedef std::set<point_type>    points_type;
      typedef points_type             baseSequence;
      typedef points_type             capSequence;
    protected:
      Obj<points_type>  _points;
      Obj<coneSequence> _empty;
      Obj<coneSequence> _return;
      void _init() {
        this->_points = new points_type();
        this->_empty  = new coneSequence();
        this->_return = new coneSequence();
      };
    public:
      DiscreteSieve() {
        this->_init();
      };
      template<typename Input>
      DiscreteSieve(const Obj<Input>& points) {
        this->_init();
        this->_points->insert(points->begin(), points->end());
      };
      virtual ~DiscreteSieve() {};
    public:
      void addPoint(const point_type& point) {
        this->_points->insert(point);
      };
      template<typename Input>
      void addPoints(const Obj<Input>& points) {
        this->_points->insert(points->begin(), points->end());
      };
      const Obj<coneSequence>& cone(const point_type& p) {return this->_empty;};
      template<typename Input>
      const Obj<coneSequence>& cone(const Input& p) {return this->_empty;};
      const Obj<coneSet>& nCone(const point_type& p, const int level) {
        if (level == 0) {
          return this->closure(p);
        } else {
          return this->_empty;
        }
      };
      const Obj<coneArray>& closure(const point_type& p) {
        this->_return->clear();
        this->_return->push_back(p);
        return this->_return;
      };
      const Obj<supportSequence>& support(const point_type& p) {return this->_empty;};
      template<typename Input>
      const Obj<supportSequence>& support(const Input& p) {return this->_empty;};
      const Obj<supportSet>& nSupport(const point_type& p, const int level) {
        if (level == 0) {
          return this->star(p);
        } else {
          return this->_empty;
        }
      };
      const Obj<supportArray>& star(const point_type& p) {
        this->_return->clear();
        this->_return->push_back(p);
        return this->_return;
      };
      const Obj<capSequence>& roots() {return this->_points;};
      const Obj<capSequence>& cap() {return this->_points;};
      const Obj<baseSequence>& leaves() {return this->_points;};
      const Obj<baseSequence>& base() {return this->_points;};
      template<typename Color>
      void addArrow(const point_type& p, const point_type& q, const Color& color) {
        throw ALE::Exception("Cannot add an arrow to a DiscreteSieve");
      };
      void stratify() {};
      void view(const std::string& name, MPI_Comm comm = MPI_COMM_NULL) const {
        ostringstream txt;
        int rank;

        if (comm == MPI_COMM_NULL) {
          comm = MPI_COMM_SELF;
          rank = 0;
        } else {
          MPI_Comm_rank(comm, &rank);
        }
        if (name == "") {
          if(rank == 0) {
            txt << "viewing a DiscreteSieve" << std::endl;
          }
        } else {
          if(rank == 0) {
            txt << "viewing DiscreteSieve '" << name << "'" << std::endl;
          }
        }
        for(typename points_type::const_iterator p_iter = this->_points->begin(); p_iter != this->_points->end(); ++p_iter) {
          txt << "  Point " << *p_iter << std::endl;
        }
        PetscSynchronizedPrintf(comm, txt.str().c_str());
        PetscSynchronizedFlush(comm);
      };
    };


    template<typename Overlap_, typename Topology_, typename Value_>
    class OverlapValues : public Section<Topology_, Value_> {
    public:
      typedef Overlap_                          overlap_type;
      typedef Section<Topology_, Value_>        base_type;
      typedef typename base_type::patch_type    patch_type;
      typedef typename base_type::topology_type topology_type;
      typedef typename base_type::chart_type    chart_type;
      typedef typename base_type::atlas_type    atlas_type;
      typedef typename base_type::value_type    value_type;
      typedef enum {SEND, RECEIVE}              request_type;
      typedef std::map<patch_type, MPI_Request> requests_type;
    protected:
      int           _tag;
      MPI_Datatype  _datatype;
      requests_type _requests;
    public:
      OverlapValues(MPI_Comm comm, const int debug = 0) : Section<Topology_, Value_>(comm, debug) {
        this->_tag      = this->getNewTag();
        this->_datatype = this->getMPIDatatype();
      };
      OverlapValues(MPI_Comm comm, const int tag, const int debug) : Section<Topology_, Value_>(comm, debug), _tag(tag) {
        this->_datatype = this->getMPIDatatype();
      };
      virtual ~OverlapValues() {};
    protected:
      MPI_Datatype getMPIDatatype() {
        if (sizeof(value_type) == 4) {
          return MPI_INT;
        } else if (sizeof(value_type) == 8) {
          return MPI_DOUBLE;
        } else if (sizeof(value_type) == 28) {
          int          blen[2];
          MPI_Aint     indices[2];
          MPI_Datatype oldtypes[2], newtype;
          blen[0] = 1; indices[0] = 0;           oldtypes[0] = MPI_INT;
          blen[1] = 3; indices[1] = sizeof(int); oldtypes[1] = MPI_DOUBLE;
          MPI_Type_struct(2, blen, indices, oldtypes, &newtype);
          MPI_Type_commit(&newtype);
          return newtype;
        } else if (sizeof(value_type) == 32) {
          int          blen[2];
          MPI_Aint     indices[2];
          MPI_Datatype oldtypes[2], newtype;
          blen[0] = 1; indices[0] = 0;           oldtypes[0] = MPI_DOUBLE;
          blen[1] = 3; indices[1] = sizeof(int); oldtypes[1] = MPI_DOUBLE;
          MPI_Type_struct(2, blen, indices, oldtypes, &newtype);
          MPI_Type_commit(&newtype);
          return newtype;
        }
        throw ALE::Exception("Cannot determine MPI type for value type");
      };
      int getNewTag() {
        static int tagKeyval = MPI_KEYVAL_INVALID;
        int *tagvalp = NULL, *maxval, flg;

        if (tagKeyval == MPI_KEYVAL_INVALID) {
          PetscMalloc(sizeof(int), &tagvalp);
          MPI_Keyval_create(MPI_NULL_COPY_FN, Petsc_DelTag, &tagKeyval, (void *) NULL);
          MPI_Attr_put(this->_comm, tagKeyval, tagvalp);
          tagvalp[0] = 0;
        }
        MPI_Attr_get(this->_comm, tagKeyval, (void **) &tagvalp, &flg);
        if (tagvalp[0] < 1) {
          MPI_Attr_get(MPI_COMM_WORLD, MPI_TAG_UB, (void **) &maxval, &flg);
          tagvalp[0] = *maxval - 128; // hope that any still active tags were issued right at the beginning of the run
        }
        //std::cout << "[" << this->commRank() << "]Got new tag " << tagvalp[0] << std::endl;
        return tagvalp[0]--;
      };
    public: // Accessors
      int getTag() const {return this->_tag;};
      void setTag(const int tag) {this->_tag = tag;};
    public:
      void construct(const int size) {
        const typename topology_type::sheaf_type& patches = this->getTopology()->getPatches();

        for(typename topology_type::sheaf_type::const_iterator p_iter = patches.begin(); p_iter != patches.end(); ++p_iter) {
          const Obj<typename topology_type::sieve_type::baseSequence>& base = p_iter->second->base();
          int                                  rank = p_iter->first;

          for(typename topology_type::sieve_type::baseSequence::iterator b_iter = base->begin(); b_iter != base->end(); ++b_iter) {
            this->setFiberDimension(rank, *b_iter, size);
          }
        }
      };
      template<typename Sizer>
      void construct(const Obj<Sizer>& sizer) {
        const typename topology_type::sheaf_type& patches = this->getTopology()->getPatches();

        for(typename topology_type::sheaf_type::const_iterator p_iter = patches.begin(); p_iter != patches.end(); ++p_iter) {
          const Obj<typename topology_type::sieve_type::baseSequence>& base = p_iter->second->base();
          int                                  rank = p_iter->first;

          for(typename topology_type::sieve_type::baseSequence::iterator b_iter = base->begin(); b_iter != base->end(); ++b_iter) {
            this->setFiberDimension(rank, *b_iter, *(sizer->restrict(rank, *b_iter)));
          }
        }
      };
      void constructCommunication(const request_type& requestType) {
        const typename topology_type::sheaf_type& patches = this->getAtlas()->getTopology()->getPatches();

        for(typename topology_type::sheaf_type::const_iterator p_iter = patches.begin(); p_iter != patches.end(); ++p_iter) {
          const patch_type patch = p_iter->first;
          MPI_Request request;

          if (requestType == RECEIVE) {
            if (this->_debug) {std::cout <<"["<<this->commRank()<<"] Receiving data(" << this->size(patch) << ") from " << patch << " tag " << this->_tag << std::endl;}
            MPI_Recv_init(this->_arrays[patch], this->size(patch), this->_datatype, patch, this->_tag, this->_comm, &request);
          } else {
            if (this->_debug) {std::cout <<"["<<this->commRank()<<"] Sending data (" << this->size(patch) << ") to " << patch << " tag " << this->_tag << std::endl;}
            MPI_Send_init(this->_arrays[patch], this->size(patch), this->_datatype, patch, this->_tag, this->_comm, &request);
          }
          this->_requests[patch] = request;
        }
      };
      void startCommunication() {
        const typename topology_type::sheaf_type& patches = this->getAtlas()->getTopology()->getPatches();

        for(typename topology_type::sheaf_type::const_iterator p_iter = patches.begin(); p_iter != patches.end(); ++p_iter) {
          MPI_Request request = this->_requests[p_iter->first];

          MPI_Start(&request);
        }
      };
      void endCommunication() {
        const typename topology_type::sheaf_type& patches = this->getAtlas()->getTopology()->getPatches();
        MPI_Status status;

        for(typename topology_type::sheaf_type::const_iterator p_iter = patches.begin(); p_iter != patches.end(); ++p_iter) {
          MPI_Request request = this->_requests[p_iter->first];

          MPI_Wait(&request, &status);
        }
      };
    };
  }
}

#endif
