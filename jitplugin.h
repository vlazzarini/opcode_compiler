#ifndef __JITPLUGIN__
#define __JITPLUGIN__
#include <plugin.h>


namespace csnd {
/** Parameters template class
 */
 class Args {
  MYFLT **ptrs;
  std::size_t len;

public:
  /** Initialise prs
   */
  void set(MYFLT **p) {
    ptrs = p;
  }

  void size(std::size_t siz) {
    len = siz;
  }

  std::size_t  size() {
    return len;
  }
  

  /** parameter access via array subscript (write)
   */
  MYFLT &operator[](int n) { return *ptrs[n]; }

  /** parameter access via array subscript (read)
   */
  const MYFLT &operator[](int n) const { return *ptrs[n]; }

  /** iterator type
  */
  typedef MYFLT **iterator;

  /** const_iterator type
  */
  typedef const MYFLT **const_iterator;

  /** vector beginning
   */
  iterator begin() { return &ptrs[0]; }

  /** vector end
   */
  iterator end() { return  &ptrs[len]; }

  /** vector beginning
   */
  const_iterator begin() const { return (const MYFLT **)&ptrs[0]; }

  /** vector end
   */
  const_iterator end() const { return (const MYFLT **)&ptrs[len]; }

  /** vector beginning
   */
  const_iterator cbegin() const { return (const MYFLT **)&ptrs[0]; }

  /** vector end
   */
  const_iterator cend() const { return (const MYFLT **)&ptrs[len]; }

  /** parameter data (MYFLT pointer) at index n
   */
  MYFLT *operator()(int n) { return ptrs[n]; }

  /** @private:
       same as operator()
   */
  MYFLT *data(int n) { return ptrs[n]; }

  /** parameter string data (STRINGDAT ref) at index n
   */
  STRINGDAT &str_data(int n) { return (STRINGDAT &)*ptrs[n]; }

  /** parameter fsig data (Fsig ref) at index n
   */
  Fsig &fsig_data(int n) { return (Fsig &)*ptrs[n]; }

  /** 1-D array data as Vector template ref
   */
  template <typename T> Vector<T> &vector_data(int n) {
    return (Vector<T> &)*ptrs[n];
  }

  /** returns 1-D numeric array data
   */
  myfltvec &myfltvec_data(int n) { return (myfltvec &)*ptrs[n]; }

};

}

/** Base class for JIT plugins
 */
struct BasePlugin : OPDS {
   /** output arguments */
   csnd::Args outargs;
   /** input arguments */
   csnd::Args inargs;
   /** Csound engine */
   csnd::Csound *csound;
   /** sample-accurate offset */
   uint32_t offset;
   /** vector samples to process */
   uint32_t nsmps;

   MYFLT sr() { return insdshead->esr; }

   BasePlugin(OPDS h) : OPDS(h) { };
   virtual ~BasePlugin() { };
   virtual int init() = 0;
   virtual int perf() = 0;
 };


struct JITPlugin : BasePlugin {
  JITPlugin(OPDS h) : BasePlugin(h) { }; 
  int init() { return OK;}  
  int perf() { return OK;} 
 };

#endif // __JITPLUGIN__
