#pragma once

/// Class to make hierarchical N-body systems
/*!
  Depend on templete class particle and pair processing function parameters proc_params
*/
template<class Tparticle>
class ParticleTree{
private:
    void* member[2];
    bool is_member_tree[2]; ///indicator whether the leaf is ParticleTree (true) or not
    bool is_collected; /// indicate whether the leaf is pointed to a particle array 

public:

    /// typedef of pair processing function
    /*!
      @param[in] id: current tree depth (root is 0)
      @param[in] ib: current leaf index (count from left to right, total size is \f$ 2^{id} \f$
      @param[in] c: two particle pointer to the two pair members
      @param[in,out] pars: proc_params type parameters used in the processing function
      \return: new particle generated by the function (e.g. center-of-mass particle)
    */
    template <class Tprocess>
    using ProcessFunctionPair = Tparticle (*) (const std::size_t id, const std::size_t ib, Tparticle* c[], Tprocess& pars);

    /// typedef of particle shifting function used during split
    /*!
      Split function create new branch with two particles, when the two particles are stored, they are applied ths shifting function by referring the old particle stored in the leaf
      @param[in] a: particle need to be shifted
      @param[in] ref: reference paraticle
      \return: new particle
    */
    using FunctionParticleShift = Tparticle (*) (const Tparticle &a, const Tparticle &ref);

    /// construct the tree py filling two particles 
    /*!
      @param[in] a: particle one to left leaf
      @param[in] b: particle two to right leaf
    */
    ParticleTree(const Tparticle& a, const Tparticle& b): is_collected(false) {
        fill(a,b);
    }

    ParticleTree(): is_collected(false) {member[0]=member[1]=NULL; is_member_tree[0]=is_member_tree[1]=false;}

    ~ParticleTree() {clear();}

    void clear() {
        for (std::size_t i=0; i<2; i++) {
            if(is_member_tree[i]) {
                ((ParticleTree<Tparticle>*)member[i])->clear();
                is_member_tree[i]=false;
            }
            else if(member[i]!=NULL) {
                if(!is_collected) delete (Tparticle*)member[i];
                member[i]=NULL;
            }
        }
    }
      
    /// fill particles to two leaves/branches
    /*!
      @param[in] a: left particle
      @param[in] b: right particle
      \return If the branches are already filled (failure), return false, else true
    */
    bool fill(const Tparticle &a, const Tparticle &b) {
        if(member[0]==NULL) member[0]=new Tparticle(a);
        else return false;
        if(member[1]==NULL) member[1]=new Tparticle(b);
        else return false;
        is_member_tree[0]=is_member_tree[1]=false;
        return true;
    }

    /// Split (delete) one leaf, create a new branch (ParticleTree) and store two particles
    /*!
      @param[in] i: leaf index (0: left; 1: right; others return false)
      @param[in] a: left paricle
      @param[in] b: right particle
      @param[in] pshift: particle shifting function applied to the two particles referring to the origin particle before storing
      \return If the branch are successfully created and filled return true, otherwise return false
    */
    bool split(const int i, const Tparticle &a, const Tparticle &b, FunctionParticleShift pshift) {
        if(i<0||i>1) return false;
        if(!is_member_tree[i]) {
            if(member[i]!=NULL) {
                Tparticle* tmp=(Tparticle*)member[i];
                assert(tmp->mass==a.mass+b.mass);
                member[i]=new ParticleTree<Tparticle>;
                bool fg=((ParticleTree<Tparticle>*)member[i])->fill(pshift(a,*tmp),pshift(b,*tmp));
                delete tmp;
                if (!fg) return false;
            }
            else {
                member[i]=new ParticleTree<Tparticle>;
                bool fg=((ParticleTree<Tparticle>*)member[i])->fill(a,b);
                if (!fg) return false;
            }
            is_member_tree[i]=true;
            return true;
        }
        else return false;
    }

    /// add a particle pair to one of the leaf
    /*!
      Add particle pair with level and branch 
      Example:
      -------------------------------------------------\\
      level              branch                 \ \
      0                      0                   \\
      / \                  \\
      1                    0   1                 \\
      / \ / \                \\
      2                  0  1 2  3               \\
      -------------------------------------------------\\

      @param[in] level: depth of the tree, top is 0
      @param[in] branch: leaf index, counting from 0 from left to right (maximum index \f$ 2^{level} \f$) 
      @param[in] a: particle one
      @param[in] b: particle two
      @param[in] pshift: particle shifting function applied to the two particles referring to the origin particle stored at the splitted leaf. The two new particles generated will be stored
      \return true: successful adding
    */
    bool link(const std::size_t level, const std::size_t branch, const Tparticle &a, const Tparticle &b, FunctionParticleShift pshift) {
        if(level>1) {
            int boundary = std::pow(2,level-1);
            int branch_index = branch/boundary;
            if(is_member_tree[branch_index]) {
                // iteratively call link for member, with reducing level by 1 and getting sub-branch index
                return ((ParticleTree<Tparticle>*)(this->member[branch_index]))->link(level-1,branch%boundary,a,b,pshift);
            }
            else return false;
        }
        else if(level==1) return this->split(branch,a,b,pshift);
        else return this->fill(a,b);
    }

    /// collect particles into plist and map addresses to it
    /*! Scan tree and push back particles into plist, then delete the branch particle and link the corresponding particle address in plist. This means the data memory is moved from tree leafs to plist array.
      @param[in] plist: particle type data array
      @param[in] n: plist array size (maximum particle number that can be stored)
      \return remaining empty number in plist (if return value is -1: collection failed)
    */
    int collectAndStore(Tparticle plist[], const int n) {
        if(!is_collected) {
            is_collected=true;
            if (n<0) return n;
            int k=n; //remaining number
            for (std::size_t i=0; i<2; i++) {
                if(is_member_tree[i]) k=((ParticleTree<Tparticle>*)(this->member[i]))->collectAndStore(&plist[n-k],k);
                else if(k>0) {
                    plist[n-k]=*(Tparticle*)member[i];
                    delete (Tparticle*)member[i];
                    member[i] = &plist[n-k];
                    k--;
                }
                else return -1;
            }
            return k;
        }
        else return -1;
    }

    /// collect particles into plist
    /*! Scan tree and store particle address into plist. 
      @param[in] plist: particle address array
      @param[in] n: plist array size (maximum particle number that can be stored)
      \return remaining empty number in plist
    */
    int collect(Tparticle* plist[], const int n) {
        if (n<0) return n;
        int k=n; //remaining number
        for (std::size_t i=0; i<2; i++) {
            if(is_member_tree[i]) k=((ParticleTree<Tparticle>*)(this->member[i]))->collect(&plist[n-k],k);
            else if(k>0) {
                plist[n-k]=(Tparticle*)member[i];
                k--;
            }
            else return -1;
        }
        return k;
    }

//    /// Apply functions to pairs in the trees iteratively
//    /*!
//      @param[in] id: current tree depth (root is 0)
//      @param[in] ib: current leaf index (count from left to right, total size is \f$ 2^{id} \f$
//      @param[in] f: process function 
//      @param[in,out] pars: process function parameters 
//      \return: new particle generated by the function (e.g. center-of-mass particle)
//     */
//    template <class Tprocess>
//    Tparticle processPairIter(const std::size_t id, const std::size_t ib, ProcessFunctionPair<Tprocess> f, Tprocess &pars) {
//        Tparticle* c[2];
//        bool newflag[2]={false};
//        for (std::size_t i=0; i<2; i++) {
//            if(is_member_tree[i]) {
//                c[i]=new Tparticle(((ParticleTree<Tparticle>*)member[i])->processPairIter(id+1, (i==0?ib:ib+id+1), f,  pars));
//                newflag[i]=true;
//            }
//            else c[i]=(Tparticle*)member[i];
//        }
//        Tparticle nc(f(id, ib, c, pars));
//    
//        for (int i=0;i<2;i++) if(newflag[i]) delete c[i];
//        return nc;
//    }
// 
//    /// find particle
//    /*! Get particle due to the index
//      @param[in] id: current tree depth (root is 0)
//      @param[in] ib: current leaf index (count from left to right, total size is \f$ 2^{id} \f$
//      @param[in] im: leaf index 0: left, 1: right
//    */
//    Tparticle *getMember(const std::size_t id, const std::size_t ib, const std::size_t im) {
//        if(id>0) {
//            if(is_member_tree[ib/id]) return ((ParticleTree<Tparticle>*)(this->member[ib/id]))->getMember(id-1,ib%id,im);
//            else return NULL;
//        }
//        else return (Tparticle*)(this->member[im]);
//    }

};
