
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "hilbert.h"
#include "prototypes.h"
#include "vector.h"
#include "io.h"
#include "cic.h"
#include "oct.h"
#include "particle.h"
#include "amr.h"
#include "tools.h"
#include "segment.h"
#include "communication.h"
#include "friedmann.h"
#include <time.h>
#include <mpi.h>


#ifdef WGPU
#include "interface.h"
#include "vector_gpu.h"
#include "cic_gpu.h"
#endif


#ifdef WHYDRO2
#include "hydro_utils.h"
#endif


#ifdef WGRAV
#include "poisson_utils.h"
#endif

#ifdef WRAD
#include "rad_utils.h"
#ifdef WCHEM
#include "chem_utils.h"
#include "atomic_data/Atomic.h"
#endif

#endif

#include "advanceamr.h"

#ifdef GPUAXL
#include "interface.h"
#include "poisson_utils_gpu.h"
#include "hydro_utils_gpu.h"
#include "rad_utils_gpu.h"
#endif


void gdb_debug()
{
  int i = 0;
  char hostname[256];
  gethostname(hostname, sizeof(hostname));
  printf("PID %d on %s ready for attach\n", getpid(), hostname);
  fflush(stdout);
  while (0 == i)
    sleep(5);
}

// ===============================================================================



//------------------------------------------------------------------------
 // the MAIN CODE
 //------------------------------------------------------------------------

#ifdef TESTCOSMO
REAL f_aexp(REAL aexp, REAL omegam, REAL omegav)
{
  return 1./sqrt(omegam/aexp+omegav*aexp*aexp);
}
#endif

int main(int argc, char *argv[])
{
  struct OCT *grid;
  struct OCT **firstoct;
  struct OCT **lastoct;

  int level,levelcoarse,levelmax,levelmin;
  int nvcycles;
  int nrelax;
  int ngridmax,ngrid;
  int npartmax;
  int cur,curnext; // flat indexes with boundaries
  int i,il,ichild,icell,inext,ii,ip,j;
  int xp,yp,zp;
  int NBND=1,NBND2=2*NBND;
  REAL dx;
  int vnei[6],vcell[6]; // arrays to get neighbors
  int vnei2[6],vcell2[6]; // arrays to get neighbors
  int vnei3[6],vcell3[6]; // arrays to get neighbors
  int neip[7]; // contains the index of the six neighbors of the current parent cell +current parent
  int ci,cj,ck;
  int cinext,cjnext,cknext;
  REAL threshold;
  REAL tsim=0.;
  REAL tinit;
  struct OCT oct;
  struct OCT* nextoct;
  struct OCT* curoct;
  struct OCT* desoct;
  struct CELL * parcell;
  struct CELL * newcell;
  struct CELL * newcell2;
  int tag;
  REAL dxcur;
  REAL *dens;
  int nxoct;
  int lmap;
  int npart;
  struct PART *part;
  struct PART *nexploc, *curploc;
  struct PART *freepart;

  struct OCT *freeoct; // the first free oct available for refinement
  struct OCT *newoct;
  int nref=0,ndes=0;

  REAL xc,yc,zc;
  int hstride; 
  int rstride;
 int gstride;
  int ncomp;
  REAL acc;
  REAL dt;
  int ntot=0,nlev,noct;
  REAL ntotd=0.,nlevd=0.;

  REAL disp,mdisp;
  
  int dir;

  char filename[128]; 
  FILE *fd;
  struct PART *nexp;
  struct PART *nexp2;
  struct PART *curp;

  struct PART *lastpart;

  int curc;
  REAL dtnew=0.;
  int nbnd;

  REAL x,y,z;
  REAL vx,vy,vz;
  REAL mass,mtot;
  REAL idx;
  REAL faexp, faexp2;
  REAL aexp;
  unsigned key;
  int nsteps;
  int nstepstart=0;
  int ndumps=0;

  struct CPUINFO cpu;
  struct RUNPARAMS param;

  size_t rstat;


  REAL avgdens; 
  REAL tmax;
  REAL tdump,adump;
#ifdef PIC
  avgdens=1.;//we assume a unit mass in a unit length box
#else
  avgdens=0.;
#endif

  REAL ainit;

#ifdef TESTCOSMO
  double tab_aexp[NCOSMOTAB];
  double tab_ttilde[NCOSMOTAB];
  double tab_t[NCOSMOTAB];
  REAL amax;
  struct COSMOPARAM cosmo;
  param.cosmo=&cosmo;
#endif

#ifdef STARS
  struct STARSPARAM stars;
  param.stars=&stars;
#endif

#ifdef WDBG
  gdb_debug();
#endif

  //========== RIEMANN CHECK ====================/
#ifdef WHYDRO2
  int rtag=0;

#ifdef RIEMANN_EXACT
  rtag=1;
#endif

#ifdef RIEMANN_HLL
  rtag=2;
#endif

#ifdef RIEMANN_HLLC
  rtag=3;
#endif


  if(rtag==0){
    printf("ERROR : RIEMANN SOLVER NOT SELECTED\n");
    abort();
  }
#endif
  //========== TEST ZONE (IF REQUIRED)==========

/*   printf("size =%d\n",sizeof(struct CELL)); */
/*   printf("size =%d\n",sizeof(struct OCT)); */
/*   abort(); */


  //=========== some initial calls =============
  GetParameters(argv[1],&param); // reading the parameters file

#ifdef STARS
  param.stars->n=0;
  param.stars->mstars= (param.cosmo->ob/param.cosmo->om) * pow(2.0,-3.0*param.lcoarse);
#endif

#ifndef TESTCOSMO
  tmax=param.tmax;
#else
  //in  cosmo case tmax is understood as a maximal expansion factor
  amax=param.tmax;
#endif

#ifdef WMPI
  MPI_Status stat;

  MPI_Init(&argc,&argv);
  MPI_Comm_size(MPI_COMM_WORLD,&(cpu.nproc));
  MPI_Comm_rank(MPI_COMM_WORLD,&(cpu.rank));

  //========= creating a PACKET MPI type =======
  MPI_Datatype MPI_PACKET,oldtypes[3]; 
  int          blockcounts[3];
  
  /* MPI_Aint type used to be consistent with syntax of */
  /* MPI_Type_extent routine */
  MPI_Aint    offsets[3], extent;
  
  
  /* Setup description of the 8 MPI_REAL fields data */
  offsets[0] = 0;
  oldtypes[0] = MPI_DOUBLE;
  blockcounts[0] = 8;
  
  /* Setup description of the 2 MPI_INT fields key, level */
  /* Need to first figure offset by getting size of MPI_REAL */
  MPI_Type_extent(MPI_DOUBLE, &extent);
  offsets[1] = 8 * extent;
  oldtypes[1] = MPI_UNSIGNED_LONG;
  blockcounts[1] = 1;

  MPI_Type_extent(MPI_UNSIGNED_LONG, &extent);
  offsets[2] = offsets[1]+extent;
  oldtypes[2] = MPI_INT;
  blockcounts[2] = 1;

  /* Now define structured type and commit it */
  MPI_Type_struct(3, blockcounts, offsets, oldtypes, &MPI_PACKET);
  MPI_Type_commit(&MPI_PACKET);

#ifdef PIC
  //========= creating a PART MPI type =======
  MPI_Datatype MPI_PART;

  /* Setup description of the 7 MPI_REAL fields x,y,z,vx,vy,vz,mass,(age stars, rhocell) */
  offsets[0] = 0;
  oldtypes[0] = MPI_DOUBLE;
  blockcounts[0] = 7;
#ifdef STARS
  blockcounts[0] = 9;
#endif
  
  /* Setup description of the 4 MPI_INT fields idx level icell is*/
  /* Need to first figure offset by getting size of MPI_REAL */
  MPI_Type_extent(MPI_DOUBLE, &extent);

  offsets[1] = blockcounts[0] * extent;
  oldtypes[1] = MPI_INT;
  blockcounts[1] = 4;
#ifdef STARS
  blockcounts[1] = 5;
#endif

  MPI_Type_extent(MPI_INT, &extent);
  offsets[2] = blockcounts[1] * extent+offsets[1];
  oldtypes[2] = MPI_UNSIGNED_LONG;
  blockcounts[2] = 1;

  /* Now define structured type and commit it */
  MPI_Type_struct(3, blockcounts, offsets, oldtypes, &MPI_PART);
  MPI_Type_commit(&MPI_PART);
  
#endif

#ifdef WHYDRO2
  //========= creating a WTYPE MPI type =======
  MPI_Datatype MPI_WTYPE;

  /* Setup description of the 7/8 MPI_REAL fields d,u,v,w,p,a */
  offsets[0] = 0;
  oldtypes[0] = MPI_DOUBLE;
#ifdef WRADHYD
  blockcounts[0] = 8;
#else
  blockcounts[0] = 7;
#endif
  /* Now define structured type and commit it */
  MPI_Type_struct(1, blockcounts, offsets, oldtypes, &MPI_WTYPE);
  MPI_Type_commit(&MPI_WTYPE);


  //========= creating a HYDRO MPI type =======
  MPI_Datatype MPI_HYDRO;

  /* Setup description of the 8 MPI_WTYPE fields one per oct*/
  offsets[0] = 0;
  oldtypes[0] = MPI_WTYPE;
  blockcounts[0] = 8;

  /* Setup description of the 2 MPI_INT fields key, level */
  /* Need to first figure offset by getting size of MPI_REAL */
  MPI_Type_extent(MPI_WTYPE, &extent);
  offsets[1] = 8 * extent;
  oldtypes[1] = MPI_UNSIGNED_LONG;
  blockcounts[1] = 1;

  MPI_Type_extent(MPI_UNSIGNED_LONG, &extent);
  offsets[2] = offsets[1]+extent;
  oldtypes[2] = MPI_INT;
  blockcounts[2] = 1;

  /* Now define structured type and commit it */
  MPI_Type_struct(3, blockcounts, offsets, oldtypes, &MPI_HYDRO);
  MPI_Type_commit(&MPI_HYDRO);
  
#endif


#ifdef WRAD
  //========= creating a RTYPE MPI type =======
  MPI_Datatype MPI_RTYPE;

  /* Setup description of the 7/8 MPI_REAL fields d,u,v,w,p,a */
  offsets[0] = 0;
  oldtypes[0] = MPI_DOUBLE;
#ifdef WCHEM
  blockcounts[0] = NGRP*4+5;
#else
  blockcounts[0] = NGRP*4+1;
#endif
  /* Now define structured type and commit it */
  MPI_Type_struct(1, blockcounts, offsets, oldtypes, &MPI_RTYPE);
  MPI_Type_commit(&MPI_RTYPE);


  //========= creating a RAD MPI type =======
  MPI_Datatype MPI_RAD;

  /* Setup description of the 8 MPI_WTYPE fields one per oct*/
  offsets[0] = 0;
  oldtypes[0] = MPI_RTYPE;
  blockcounts[0] = 8;

  /* Setup description of the 2 MPI_INT fields key, level */
  /* Need to first figure offset by getting size of MPI_REAL */
  MPI_Type_extent(MPI_RTYPE, &extent);
  offsets[1] = 8 * extent;
  oldtypes[1] = MPI_UNSIGNED_LONG;
  blockcounts[1] = 1;

  MPI_Type_extent(MPI_UNSIGNED_LONG, &extent);
  offsets[2] = offsets[1]+extent;
  oldtypes[2] = MPI_INT;
  blockcounts[2] = 1;

  /* Now define structured type and commit it */
  MPI_Type_struct(3, blockcounts, offsets, oldtypes, &MPI_RAD);
  MPI_Type_commit(&MPI_RAD);
  
#endif


  //============================================

  cpu.MPI_PACKET=&MPI_PACKET;
#ifdef PIC
  cpu.MPI_PART=&MPI_PART;
#endif

#ifdef WHYDRO2
  cpu.MPI_HYDRO=&MPI_HYDRO;
#endif

#ifdef WRAD
  cpu.MPI_RAD=&MPI_RAD;
#endif

  cpu.comm=MPI_COMM_WORLD;
#else
  cpu.rank=0;
  cpu.nproc=1;
#endif

  //=========== assigning values =============
  levelcoarse=param.lcoarse;
  levelmax=param.lmax;
  levelmin=param.mgridlmin;
  nvcycles=param.nvcycles;
  nrelax=param.nrelax;

  ngridmax=param.ngridmax;

#ifdef PIC
  npartmax=param.npartmax;
#ifdef PART2
  npart=5;
#else
  npart=128*128*128;
#endif

#ifdef PARTN
  npart=32768;
#endif
#endif

  threshold=param.amrthresh;
  gstride=fmax(8,param.gstride);//pow(2,levelcoarse);
  hstride=fmax(8,param.hstride);//pow(2,levelcoarse);
  rstride=hstride;
  ncomp=10;
  acc=param.poissonacc;
  dt=param.dt;
  cpu.maxhash=param.maxhash;
  cpu.levelcoarse=levelcoarse;

#ifdef GPUAXL
  cpu.nthread=param.nthread;
  cpu.nstream=param.nstream;
#endif
  //breakmpi();
  //========== allocations ===================
  
  //  if(cpu.rank==0) printf("Allocating %f GB cell=%f GB part=%f GB book=%f",(sizeof(struct OCT)*ngridmax+sizeof(struct PART)*npart+cpu.maxhash*sizeof(struct OCT*)+stride*ncomp*sizeof(REAL))/(1024*1024*1024.),sizeof(struct OCT)*ngridmax/(1024*1024*1024.),sizeof(struct PART)*npart/(1024*1024*1024.),(cpu.maxhash*sizeof(struct OCT*)+stride*ncomp*sizeof(REAL))/(1024.*1024.*1024.));

  int memsize=0.;
  grid=(struct OCT*)calloc(ngridmax,sizeof(struct OCT)); memsize+=ngridmax*sizeof(struct OCT);// the oct grid
#ifdef PIC
  part=(struct PART*)calloc(npartmax,sizeof(struct PART)); memsize+=npartmax*sizeof(struct PART);// the particle array
  cpu.firstpart=part;
  // we set all the particles mass to -1
  for(ii=0;ii<npartmax;ii++) part[ii].mass=-1.0;
#endif
 
  if(cpu.rank==0){
    printf(" === alloc Memory ===\n");
    printf(" oct size=%f ngridmax=%d\n",sizeof(struct OCT)/1024./1024.,ngridmax);
    printf(" grid = %f MB\n",(ngridmax/(1024*1024.))*sizeof(struct OCT));
    printf(" part = %f MB\n",(npartmax/(1024*1024.))*sizeof(struct PART));
  }

  firstoct=(struct OCT **)calloc(levelmax,sizeof(struct OCT *)); memsize+=levelmax*sizeof(struct OCT *);// the firstoct of each level
  lastoct=(struct OCT **)calloc(levelmax,sizeof(struct OCT *)); memsize+=levelmax*sizeof(struct OCT *);// the last oct of each level
  cpu.htable=(struct OCT**) calloc(cpu.maxhash,sizeof(struct OCT *)); memsize+=cpu.maxhash*sizeof(struct OCT*);// the htable keys->oct address
  cpu.noct =(int *)calloc(levelmax,sizeof(int)); memsize+=levelmax*sizeof(int);// the number of octs per level
  cpu.npart=(int *)calloc(levelmax,sizeof(int)); memsize+=levelmax*sizeof(int);// the number of particles per level

  lastpart=part-1; // the last particle points before the first at the very beginning



  //===================================================================================================




  // allocating the vectorized tree
  

  
  // allocating the 6dim stencil
  struct HGRID *stencil;
  struct STENGRAV gstencil;
  struct RGRID *rstencil;

#ifndef GPUAXL
  //printf("stencil=%p with stride=%d\n",stencil,hstride);
  stencil=(struct HGRID*)calloc(hstride,sizeof(struct HGRID));
  //printf("stenci=%p mem=%f\n",stencil,hstride*sizeof(struct HGRID)/(1024.*1024.));

#ifdef WRAD
  rstencil=(struct RGRID*)calloc(hstride,sizeof(struct RGRID));
#endif

#ifndef FASTGRAV
  struct GGRID *grav_stencil;
  grav_stencil=(struct GGRID*)calloc(gstride,sizeof(struct GGRID));
  gstencil.stencil=grav_stencil;
  gstencil.res=(REAL *)calloc(gstride*8,sizeof(REAL));
  gstencil.pnew=(REAL *)calloc(gstride*8,sizeof(REAL));
  gstencil.resLR=(REAL *)calloc(gstride,sizeof(REAL));
  

#else
  gstencil.d=(REAL *)calloc(gstride*8,sizeof(REAL));
  gstencil.p=(REAL *)calloc(gstride*8,sizeof(REAL));
  gstencil.pnew=(REAL *)calloc(gstride*8,sizeof(REAL));

  gstencil.nei=(int *)calloc(gstride*7,sizeof(int));
  gstencil.level=(int *)calloc(gstride,sizeof(int));
  gstencil.cpu=(int *)calloc(gstride,sizeof(int));
  gstencil.valid=(char *)calloc(gstride,sizeof(char));
  
  gstencil.res=(REAL *)calloc(gstride*8,sizeof(REAL));
  gstencil.resLR=(REAL *)calloc(gstride,sizeof(REAL));
#endif
  
#endif

  

#ifdef GPUAXL
  // ================================== GPU ALLOCATIONS ===============
  countdevices(0);
  initlocaldevice(0,1);
  checkdevice(0);
#ifdef WGRAV
  create_pinned_gravstencil(&gstencil,gstride);
#ifdef FASTGRAV
  struct STENGRAV dev_stencil;
  cpu.dev_stencil=&dev_stencil;
#endif
  create_gravstencil_GPU(&cpu,gstride);
  cpu.gresA=GPUallocREAL(gstride*8);
  cpu.gresB=GPUallocREAL(gstride*8);
  cpu.cuparam=GPUallocScanPlan(gstride*8);
#endif

#ifdef WHYDRO2 
  stencil=(struct HGRID*)calloc(hstride,sizeof(struct HGRID));
  //printf("hstencil=%p mem=%f mem/elem=%f \n",stencil,hstride*sizeof(struct HGRID)/(1024.*1024.),sizeof(struct HGRID)/(1024.*1024.));
  // UNCOMMENT BELOW FOR FASTHYDRO GPU
  create_pinned_stencil(&stencil,hstride);  
  create_hydstencil_GPU(&cpu,hstride); 
#endif 

#ifdef WRAD
  rstencil=(struct RGRID*)calloc(rstride,sizeof(struct RGRID));
  //printf("rstencil=%p mem=%f\n",rstencil,rstride*sizeof(struct RGRID)/(1024.*1024.));

  // UNCOMMENT BELOW FOR FASTRT GPU
  create_pinned_stencil_rad(&rstencil,rstride);  
  create_radstencil_GPU(&cpu,rstride); 
#endif


  // ====================END GPU ALLOCATIONS ===============
#endif
    

  if(cpu.rank==0) printf("Allocations %f GB done\n",memsize/(1024.*1024*1024));

  //========== setting up the parallel topology ===

  cpu.nsend=NULL;
  cpu.nrecv=NULL;
  cpu.nsend_coarse=NULL;
  cpu.nrecv_coarse=NULL;

  // We segment the oct distributions at levelcoarse 
    cpu.bndoct=NULL;
    cpu.mpinei=NULL;
    cpu.dict=NULL;

    cpu.nbuff=param.nbuff;
    cpu.nbufforg=param.nbuff;
    cpu.bndoct=(struct OCT**)calloc(cpu.nbufforg,sizeof(struct OCT*));

    cpu.allkmin=(int*)calloc(cpu.nproc,sizeof(int));
    cpu.allkmax=(int*)calloc(cpu.nproc,sizeof(int));

    load_balance(levelcoarse,&cpu);

#ifdef WMPI
    MPI_Allgather(&cpu.kmin,1,MPI_INT,cpu.allkmin,1,MPI_INT,cpu.comm);
    MPI_Allgather(&cpu.kmax,1,MPI_INT,cpu.allkmax,1,MPI_INT,cpu.comm);
    MPI_Barrier(cpu.comm);
#else
    cpu.allkmin[0]=cpu.kmin;
    cpu.allkmax[0]=cpu.kmax;
#endif    
    

    
  //========== building the initial meshes ===

  if(cpu.rank==0) printf("building initial mesh\n");

  //breakmpi();
  // ZERO WE CREATE A ROOT CELL
  
  struct CELL root;
  root.child=grid;
  

  // FIRST WE POPULATE THE ROOT OCT
  grid->x=0.;
  grid->y=0.;
  grid->z=0.;

  grid->parent=NULL;
  grid->level=1;
  for(i=0;i<6;i++) grid->nei[i]=&root; //periodic boundary conditions
  grid->prev=NULL;
  grid->next=NULL;

  // setting the densities in the cells and the index
  for(icell=0;icell<8;icell++){ 
    /* grid->cell[icell].density=0.; */
    /* grid->cell[icell].pot=0.; */
    /* grid->cell[icell].temp=0.; */
    grid->cell[icell].idx=icell;


#ifdef WHYDRO2
    memset(&(grid->cell[icell].field),0,sizeof(struct Wtype));
#endif

  }

  grid->cpu=-1;
  grid->vecpos=-1;
  grid->border=0;

  // start the creation of the initial amr grid from level 1
  firstoct[0]=grid;
  lastoct[0]=grid;
  int noct2;
  int segok;

  newoct=grid+1;
  for(level=1;level<levelcoarse;level++){ // sweeping the levels from l=1 to l=levelcoarse
    dxcur=1./pow(2,level);
    nextoct=firstoct[level-1];
    noct2=0;
    if(nextoct==NULL) continue;
    do // sweeping level
      {
	curoct=nextoct;
	nextoct=curoct->next;
	for(icell=0;icell<8;icell++){ // sweeping the cells

	  segok=segment_cell(curoct,icell,&cpu,levelcoarse);// the current cell will be splitted according to a segmentation condition
	  if(segok==1){ 
	    //if(level==levelcoarse-1) printf(" segok=%d\n",segok);

	    noct2++;
	    // the newoct is connected to its mother cell
	    curoct->cell[icell].child=newoct;
	    
	    // a newoct is created
	    newoct->parent=&(curoct->cell[icell]);
	    newoct->level=curoct->level+1;
	    newoct->x=curoct->x+( icell   %2)*dxcur;
	    newoct->y=curoct->y+((icell/2)%2)*dxcur;
	    newoct->z=curoct->z+( icell   /4)*dxcur;

	    // filling the cells
	    for(ii=0;ii<8;ii++){
	      newoct->cell[ii].marked=0;
	      newoct->cell[ii].child=NULL;
	      /* newoct->cell[ii].density=0.; */
	      newoct->cell[ii].idx=ii;
#ifdef PIC
	      newoct->cell[ii].phead=NULL;
#endif

#ifdef WHYDRO2
	      memset(&(newoct->cell[icell].field),0,sizeof(struct Wtype));
#endif

#ifdef WGRAV
	      memset(&(newoct->cell[icell].gdata),0,sizeof(struct Gtype));
	      memset(newoct->cell[icell].f,0,sizeof(REAL)*3);
#endif
	    }
	    
	    //the neighbours
	    getcellnei(icell, vnei, vcell);
	    for(ii=0;ii<6;ii++){
	      if((vnei[ii]!=6)){ 
		newoct->nei[ii]=&(curoct->nei[vnei[ii]]->child->cell[vcell[ii]]);
	      }else{
		newoct->nei[ii]=&(curoct->cell[vcell[ii]]);
	      }
	    }

	    // vector data
	    newoct->vecpos=-1;
	    newoct->border=0;
	    
	    // preparing the next creations on level+1
	    newoct->next=NULL;
	    
	    if(firstoct[level]==NULL){
	      firstoct[level]=newoct;
	      newoct->prev=NULL;
	    }
	    else{
	      newoct->prev=lastoct[level];
	      lastoct[level]->next=newoct;
	    }
	    lastoct[level]=newoct;

	    // next oct ready
	    newoct++; 
	  }
 	}
      }while(nextoct!=NULL);
    if(cpu.rank==0) printf("level=%d noct=%d\n",level,noct2);
  }

  if(cpu.rank==0) printf("Initial Mesh done \n");
  

 // ==================================== assigning CPU number to levelcoarse OCTS // filling the hash table // Setting up the MPI COMMS

#ifdef WMPI
  if(cpu.rank==0) printf("Set up MPI \n");

  cpu.sendbuffer=NULL;
  cpu.recvbuffer=NULL;
  cpu.psendbuffer=NULL;
  cpu.precvbuffer=NULL;
  cpu.hsendbuffer=NULL;
  cpu.hrecvbuffer=NULL;
  cpu.Rsendbuffer=NULL;
  cpu.Rrecvbuffer=NULL;
#endif

  int newloadb=1;
  setup_mpi(&cpu,firstoct,levelmax,levelcoarse,ngridmax,newloadb); // out of WMPI to compute the hash table
  newloadb=0;



#if 0
#ifdef WMPI
  // allocating the communication buffers
  sendbuffer=(struct PACKET **)(calloc(cpu.nnei,sizeof(struct PACKET*)));
  recvbuffer=(struct PACKET **)(calloc(cpu.nnei,sizeof(struct PACKET*)));
  for(i=0;i<cpu.nnei;i++) {
    sendbuffer[i]=(struct PACKET *) (calloc(cpu.nbuff,sizeof(struct PACKET)));
    recvbuffer[i]=(struct PACKET *) (calloc(cpu.nbuff,sizeof(struct PACKET)));
  }

  cpu.sendbuffer=sendbuffer;
  cpu.recvbuffer=recvbuffer;

#ifdef PIC
  psendbuffer=(struct PART_MPI **)(calloc(cpu.nnei,sizeof(struct PART_MPI*)));
  precvbuffer=(struct PART_MPI **)(calloc(cpu.nnei,sizeof(struct PART_MPI*)));
  for(i=0;i<cpu.nnei;i++) {
    psendbuffer[i]=(struct PART_MPI *) (calloc(cpu.nbuff,sizeof(struct PART_MPI)));
    precvbuffer[i]=(struct PART_MPI *) (calloc(cpu.nbuff,sizeof(struct PART_MPI)));
  }

  cpu.psendbuffer=psendbuffer;
  cpu.precvbuffer=precvbuffer;

#endif 

#ifdef WHYDRO2
  hsendbuffer=(struct HYDRO_MPI **)(calloc(cpu.nnei,sizeof(struct HYDRO_MPI*)));
  hrecvbuffer=(struct HYDRO_MPI **)(calloc(cpu.nnei,sizeof(struct HYDRO_MPI*)));
  for(i=0;i<cpu.nnei;i++) {
    hsendbuffer[i]=(struct HYDRO_MPI *) (calloc(cpu.nbuff,sizeof(struct HYDRO_MPI)));
    hrecvbuffer[i]=(struct HYDRO_MPI *) (calloc(cpu.nbuff,sizeof(struct HYDRO_MPI)));
  }
  
  cpu.hsendbuffer=hsendbuffer;
  cpu.hrecvbuffer=hrecvbuffer;

  /* fsendbuffer=(struct FLUX_MPI **)(calloc(cpu.nnei,sizeof(struct FLUX_MPI*))); */
  /* frecvbuffer=(struct FLUX_MPI **)(calloc(cpu.nnei,sizeof(struct FLUX_MPI*))); */
  /* for(i=0;i<cpu.nnei;i++) { */
  /*   fsendbuffer[i]=(struct FLUX_MPI *) (calloc(cpu.nbuff,sizeof(struct FLUX_MPI))); */
  /*   frecvbuffer[i]=(struct FLUX_MPI *) (calloc(cpu.nbuff,sizeof(struct FLUX_MPI))); */
  /* } */
#endif 

#ifdef WRAD
  Rsendbuffer=(struct RAD_MPI **)(calloc(cpu.nnei,sizeof(struct RAD_MPI*)));
  Rrecvbuffer=(struct RAD_MPI **)(calloc(cpu.nnei,sizeof(struct RAD_MPI*)));
  for(i=0;i<cpu.nnei;i++) {
    Rsendbuffer[i]=(struct RAD_MPI *) (calloc(cpu.nbuff,sizeof(struct RAD_MPI)));
    Rrecvbuffer[i]=(struct RAD_MPI *) (calloc(cpu.nbuff,sizeof(struct RAD_MPI)));
  }
  
  cpu.Rsendbuffer=Rsendbuffer;
  cpu.Rrecvbuffer=Rrecvbuffer;
#endif 
#endif


#endif



  // =====================  computing the memory location of the first freeoct and linking the freeocts

  freeoct=lastoct[levelcoarse-1]+1; //(at this stage the memory is perfectly aligned)
  freeoct->prev=NULL;
  freeoct->next=freeoct+1;
  for(curoct=freeoct+1;curoct<(grid+ngridmax);curoct++){ // sweeping all the freeocts
    curoct->prev=curoct-1;
    curoct->next=NULL;
    if(curoct!=(grid+ngridmax-1)) curoct->next=curoct+1;
  }



  //=================================  building the array of timesteps

  REAL *adt;
  adt=(REAL *)malloc(sizeof(REAL)*levelmax);
  for(level=1;level<=levelmax;level++) adt[level-1]=param.dt;
  
  int *ndt;
  ndt=(int *)malloc(sizeof(int)*levelmax);

  // INITIALISATION FROM INITIAL CONDITIONS =========================
  if(param.nrestart==0){

#ifdef PIC
    //===================================================================================================================================
    //===================================================================================================================================
    //===================================================================================================================================
    //===================================================================================================================================
    //===================================================================================================================================
    //===================================================================================================================================
    //===================================================================================================================================
    //===================================================================================================================================
    //===================================================================================================================================
    //===================================================================================================================================

    // ==================================== assigning particles to cells
    //breakmpi();
    if(cpu.rank==0) printf("==> starting part\n");
 
    // initialisation of particles
  

#ifdef PART2

    int ir,nr=5;
    ip=0;
    REAL dxcell=1./pow(2.,levelcoarse);
    REAL epsilon=0.;
    REAL r0=0.12;
    REAL vf=0.8;
    npart=0;
    for(ir=0;ir<nr;ir++) {
      // first we read the position etc... (eventually from the file)
      if(ir==0){
	x=0.5;
	y=0.5;
	z=0.5;

	vx=0.;
	vy=0.;
	vz=0.;
      
	mass=1.0-epsilon;
      }
      else if(ir==1){

	x=0.5+r0;
	y=0.5;
	z=0.5;

	vx=0.;
	vy=sqrt((1.-epsilon)/r0)*1.0; // this one is circular
	vz=0.;
      
	mass=epsilon/(nr-1);
      }
      else if(ir==2){

	x=0.5;
	y=0.5+r0*0.3;
	z=0.5;

	vy=0.;
	vx=-sqrt((1.-epsilon)/(r0*0.3))*1.0;//this one is circular
	vz=0.;
      
	mass=epsilon/(nr-1);
      }
      else if(ir==3){

	x=0.5-r0;
	y=0.5;
	z=0.5;

	vx=0.;
	vy=-sqrt((1.-epsilon)/r0)*vf;
	vz=0.;
      
	mass=epsilon/(nr-1);
      }
      else if(ir==4){

	x=0.5;
	y=0.5-r0;
	z=0.5;

	vy=0.;
	vx=sqrt((1.-epsilon)/r0)*vf;
	vz=0.;
      
	mass=epsilon/(nr-1);
      }
    
      // periodic boundary conditions
    
      x+=(x<0)*((int)(-x)+1)-(x>1.)*((int)x); 
      y+=(y<0)*((int)(-y)+1)-(y>1.)*((int)y); 
      z+=(z<0)*((int)(-z)+1)-(z>1.)*((int)z); 
    
      // it it belongs to the current cpu, we proceed and assign the particle to the particle array
      if(segment_part(x,y,z,&cpu,levelcoarse)){
	part[ip].x=x;
	part[ip].y=y;
	part[ip].z=z;
      
	part[ip].vx=vx;
	part[ip].vy=vy;
	part[ip].vz=vz;
      
	part[ip].mass=mass;
	lastpart=part+ip;
	part[ip].idx=ir;
	ip++;
      }
    }
  
    npart=ip; // we compute the localnumber of particle

#endif


#ifdef PARTN

    int ir,nr=32768;
    ip=0;
    REAL dxcell=1./pow(2.,levelcoarse);
    REAL th,ph,r;
    for(ir=0;ir<nr;ir++) {
      // first we read the position etc... (eventually from the file)
      if(ir==0){
	x=0.5;
	y=0.5;
	z=0.5;

	vx=0.;
	vy=0.;
	vz=0.;
      
	mass=1.;
      }
      else{


	th=acos(((REAL)(rand())/RAND_MAX*2-1.));
	ph=2*M_PI*(REAL)(rand())/RAND_MAX;
	//r=(REAL)(rand())/RAND_MAX*0.3;
	r=0.12;

	x=r*sin(th)*cos(ph)+0.5;
	y=r*sin(th)*sin(ph)+0.5;
	z=r*cos(th)+0.5;

	vx=(REAL)(rand())/RAND_MAX*2.-1.;
	vy=(REAL)(rand())/RAND_MAX*2.-1.;
	vz=(REAL)(rand())/RAND_MAX*2.-1.;
      
	mass=0.;
      }
    
      // periodic boundary conditions
    
      x+=(x<0)*((int)(-x)+1)-(x>1.)*((int)x); 
      y+=(y<0)*((int)(-y)+1)-(y>1.)*((int)y); 
      z+=(z<0)*((int)(-z)+1)-(z>1.)*((int)z); 
    
      // it it belongs to the current cpu, we proceed and assign the particle to the particle array
      if(segment_part(x,y,z,&cpu,levelcoarse)){
	part[ip].x=x;
	part[ip].y=y;
	part[ip].z=z;
      
	part[ip].vx=vx;
	part[ip].vy=vy;
	part[ip].vz=vz;
      
	part[ip].mass=mass;
	lastpart=part+ip;
	part[ip].idx=ir;
	ip++;
      }
    }
  
    npart=ip; // we compute the localnumber of particle

#endif

#ifdef TESTPLUM
    int dummy;
    REAL dummyf;
    int npartf;

    //breakmpi();
    fd=fopen("utils/data.inp","r");
    if(fd==NULL) {
      printf("Error while reading particle file ABORT\n");
      abort();
    }
    fscanf(fd,"%d",&dummy);
    fscanf(fd,"%d",&npartf);
    fscanf(fd,"%f",&dummyf);

    ip=0.;
    for(i=0;i<npartf;i++)
      {
	//fscanf(fd,"%d %f %f %f %f %f %f %f",&part[i].idx,&part[i].mass,&(part[i].x),&(part[i].y),&(part[i].z),&(part[i].vx),&(part[i].vy),&(part[i].vz));
	fscanf(fd,"%d %f %f %f %f %f %f %f",&dummy,&mass,&x,&y,&z,&vx,&vy,&vz);
      
	x+=0.5;
	y+=0.5;
	z+=0.5;
	// periodic boundary conditions
    
	x+=(x<0)*((int)(-x)+1)-(x>1.)*((int)x); 
	y+=(y<0)*((int)(-y)+1)-(y>1.)*((int)y); 
	z+=(z<0)*((int)(-z)+1)-(z>1.)*((int)z); 

	// it it belongs to the current cpu, we proceed and assign the particle to the particle array
	if(segment_part(x,y,z,&cpu,levelcoarse)){
	  part[ip].x=x;
	  part[ip].y=y;
	  part[ip].z=z;
	
	  part[ip].vx=vx;
	  part[ip].vy=vy;
	  part[ip].vz=vz;
	
	  part[ip].mass=mass;
	  lastpart=part+ip;
	  ip++;
	}
      
      }
    fclose(fd);
    npart=ip; // we compute the localnumber of particle

#endif  

#ifdef TESTCOSMO // =================PARTICLE COSMOLOGICAL CASE

#ifdef HPC
    long dummy;
#else
    int dummy;
#endif

    REAL dummyf;
    int npartf;

    int nploc;
    REAL munit;
    REAL lbox;

#ifdef GRAFIC // ==================== read grafic file

    lastpart=read_grafic_part(part, &cpu, &munit, &ainit, &npart, &param);
#endif


#ifdef ZELDOVICH // ==================== read ZELDOVICH file

    lastpart=read_zeldovich_part(part, &cpu, &munit, &ainit, &npart, &param,firstoct);
#endif

#ifdef EDBERT // ==================== read ZELDOVICH file

    lastpart=read_edbert_part(part, &cpu, &munit, &ainit, &npart, &param,firstoct);
#endif


#endif

  
    // we set all the "remaining" particles mass to -1
    for(ii=npart;ii<npartmax;ii++) part[ii].mass=-1.0;


	/// assigning PARTICLES TO COARSE GRID
    if(cpu.rank==0) printf("start populating coarse grid with particles\n");
    double tp1,tp2;
    double *tp;
  
    tp=(double *)calloc(cpu.nproc,sizeof(double));

    MPI_Barrier(cpu.comm);
    tp1=MPI_Wtime();

    for(i=0;i<npart;i++){
      unsigned long int key;
      unsigned long hidx;
      int found=0;
      key=pos2key(part[i].x,part[i].y,part[i].z,levelcoarse);
      hidx=hfun(key,cpu.maxhash);
      nextoct=cpu.htable[hidx];
      if(nextoct!=NULL){
	do{ // resolving collisions
	  curoct=nextoct;
	  nextoct=curoct->nexthash;
	  found=((oct2key(curoct,curoct->level)==key)&&(levelcoarse==curoct->level));
	}while((nextoct!=NULL)&&(!found));
      }

      if(found){ // the reception oct has been found

	REAL dxcur=1./pow(2.,levelcoarse);
				
	int xp=(int)((part[i].x-curoct->x)/dxcur);//xp=(xp>1?1:xp);xp=(xp<0?0:xp);
	int yp=(int)((part[i].y-curoct->y)/dxcur);//yp=(yp>1?1:yp);yp=(yp<0?0:yp);
	int zp=(int)((part[i].z-curoct->z)/dxcur);//zp=(zp>1?1:zp);zp=(zp<0?0:zp);
	int ip=xp+yp*2+zp*4;
      
	if((ip>7)||(i<0)){
	  printf("%e %e %e oct= %e %e %e xp=%d %d %d ip=%d\n",part[i].x,part[i].y,part[i].z,curoct->x,curoct->y,curoct->z,xp,yp,zp,ip);
	  abort();
	}
      
	//part[i].icell=ip;
	part[i].level=levelcoarse;
  
	// the reception cell 
	struct CELL *newcell=&(curoct->cell[ip]);
	curp=findlastpart(newcell->phead);
	if(curp!=NULL){
	  curp->next=part+i;
	  part[i].next=NULL;
	  part[i].prev=curp;
	}
	else{
	  newcell->phead=part+i;
	  part[i].next=NULL;
	  part[i].prev=NULL;
	}
      }
    }

    MPI_Barrier(cpu.comm);
    tp2=MPI_Wtime();

    tp[cpu.rank]=tp2-tp1;
    MPI_Allgather(MPI_IN_PLACE,0,MPI_DOUBLE,tp,1,MPI_DOUBLE,cpu.comm);
    if(cpu.rank==0){
      int ic;
      for(ic=0;ic<cpu.nproc;ic++) printf("%e ",tp[ic]);
      printf("\n");
    }
 


  // ========================  computing the memory location of the first freepart and linking the free parts

    freepart=lastpart+1; // at this stage the memory is perfectly aligned
    freepart->prev=NULL;
    freepart->next=freepart+1;
    for(curp=freepart+1;curp<(part+npartmax);curp++){
      curp->prev=curp-1;
      curp->next=NULL;
      if(curp!=(freepart+npartmax-1)) curp->next=curp+1;
    }
    
#endif

    //===================================================================================================================================
    //===================================================================================================================================
    //===================================================================================================================================
    //===================================================================================================================================
    //===================================================================================================================================
    //===================================================================================================================================
    //===================================================================================================================================
    //===================================================================================================================================
    //===================================================================================================================================
    //===================================================================================================================================


#ifdef WHYDRO2
  
#ifdef GRAFIC
    int ncellhydro;
    ncellhydro=read_grafic_hydro(&cpu,&ainit, &param);

    if(cpu.rank==0) printf("%d hydro cell found in grafic file with aexp=%e\n",ncellhydro,ainit);
#else

#ifdef EVRARD
    read_evrard_hydro(&cpu,firstoct,&param);
#else

    //===================================================================================================================================
    //===================================================================================================================================
    //===================================================================================================================================
    //===================================================================================================================================
    //===================================================================================================================================
    //===================================================================================================================================
    //===================================================================================================================================
    //===================================================================================================================================
    //===================================================================================================================================
    //===================================================================================================================================

    // initialisation of hydro quantities
    // Shock Tube

#ifdef TUBE
    printf("Read Shock Tube\n");
    read_shocktube(&cpu, &tinit,&param,firstoct);
#endif


#endif
#endif




#endif

    //    abort();

    //===================================================================================================================================
#ifdef WRAD

#ifdef WCHEM
    if(NGRP!=NGRP_ATOMIC){
      printf("NGRP and NGRP_ATOMIC INCONSISTENT ! ERROR !\n");
      abort();
    }
#endif

#ifdef WRADTEST
    REAL X0=1./pow(2,levelcoarse);
    int igrp;
    param.unit.unit_v=LIGHT_SPEED_IN_M_PER_S;
    param.unit.unit_n=1.;

#ifndef TESTCOSMO
#ifndef TESTCLUMP
    param.unit.unit_l=15e3*PARSEC;
#else
    param.unit.unit_l=6.6e3*PARSEC;
    REAL vclump=4./3.*M_PI*pow(0.8e3*PARSEC,3); // clump volume in internal units
    param.unit.unit_mass=200.*(pow(param.unit.unit_l,3)+199.*vclump)*PROTON_MASS*MOLECULAR_MU;
    REAL pstar;
    pstar=param.unit.unit_n*param.unit.unit_mass*pow(param.unit.unit_v,2);
#endif
    param.unit.unit_t=param.unit.unit_l/param.unit.unit_v;
    ainit=1.;
#else
    ainit=1./(16.);;
    REAL om=0.27;
    REAL ov=0.73;
    REAL ob=0.045;
    REAL h0=0.73;
    REAL H0=h0*100*1e3/1e6/PARSEC;

    param.cosmo->om=om;
    param.cosmo->ov=ov;
    param.cosmo->ob=ob;
    param.cosmo->H0=h0*100.;

    REAL rstar= 20.*1e6*PARSEC; // box size in m
    double rhoc=3.*H0*H0/(8.*M_PI*NEWTON_G); // comoving critical density (kg/m3)

    REAL rhostar=rhoc*om;
    REAL tstar=2./H0/sqrt(om); // sec
    REAL vstar=rstar/tstar; //m/s

    param.unit.unit_l=rstar;
    param.unit.unit_v=vstar;
    param.unit.unit_t=tstar;
    param.unit.unit_n=1.;

#endif

    for(level=levelcoarse;level<=levelmax;level++) 
      {
	dxcur=pow(0.5,level);
	nextoct=firstoct[level-1];
	if(nextoct==NULL) continue;
	do // sweeping level
	  {
	    curoct=nextoct;
	    nextoct=curoct->next;
	    for(icell=0;icell<8;icell++) // looping over cells in oct
	      {
		xc=curoct->x+( icell&1)*dxcur+dxcur*0.5;
		yc=curoct->y+((icell>>1)&1)*dxcur+dxcur*0.5;
		zc=curoct->z+((icell>>2))*dxcur+dxcur*0.5;
		for(igrp=0;igrp<NGRP;igrp++){

#ifdef WCHEM
		  REAL xion,temperature;
		  REAL eint;
		  REAL nh;
#ifndef COOLING
		  temperature=1e4;
		  xion=1.2e-3;
#else
#ifndef TESTCLUMP
		  temperature=1e2;
		  xion=1e-6;
#else
		  temperature=8000.;
		  xion=1e-5;
#endif
#endif

#ifndef TESTCOSMO
#ifndef TESTCLUMP
		  nh=1000.;
#else
		  nh=200.;
#endif
#else
		  nh=0.2;
#endif

#ifdef TESTCLUMP
		  // defining the clump
		  REAL X0=5./6.6;
		  REAL rc=sqrt(pow(xc-X0,2)+pow(yc-0.5,2)+pow(zc-0.5,2));
		  if(rc<=(0.8/6.6)){
		    temperature=40.;
		    nh=40000.;
		  }
#endif

#ifndef TESTCLUMP		
		  param.unit.unit_mass=nh*pow(param.unit.unit_l,3)*PROTON_MASS*MOLECULAR_MU;
		  REAL pstar;
		  pstar=param.unit.unit_n*param.unit.unit_mass*pow(param.unit.unit_v,2);// note that below nh is already supercomiving hence the lack of unit_l in pstar
#endif

		  curoct->cell[icell].rfield.nh=nh*pow(param.unit.unit_l,3)/param.unit.unit_n; 
		  eint=(1.5*curoct->cell[icell].rfield.nh*(1.+xion)*KBOLTZ*temperature)/pstar;
		  curoct->cell[icell].rfield.eint=eint; 
		  curoct->cell[icell].rfield.xion=xion; 
		  E2T(&curoct->cell[icell].rfield,1.0,&param);

		
		
#ifdef WRADHYD
		  curoct->cell[icell].field.d=curoct->cell[icell].rfield.nh*PROTON_MASS*MOLECULAR_MU/param.unit.unit_mass;
		  curoct->cell[icell].field.u=0.0;
		  curoct->cell[icell].field.v=0.0;
		  curoct->cell[icell].field.w=0.0;
		  curoct->cell[icell].field.p=eint*(GAMMA-1.);
		  curoct->cell[icell].field.a=sqrt(GAMMA*curoct->cell[icell].field.p/curoct->cell[icell].field.d);
		  getE(&(curoct->cell[icell].field));
#endif

#endif
		}
	      }
	  }while(nextoct!=NULL);
      
      
      }
#endif
#endif

    // saving the absolute initial time
#ifdef TESTCOSMO
    tinit=ainit;
#else
    tinit=tsim;
#endif
    tsim=tinit;
  }
  else{
    //==================================== Restart =================================================
    MPI_Barrier(cpu.comm);
#ifdef PIC
    sprintf(filename,"bkp/part.%05d.p%05d",param.nrestart,cpu.rank); 
    freepart=restore_part(filename,firstoct,&tsim,&param,&cpu,part);
    cpu.freepart=freepart;
#endif

    sprintf(filename,"bkp/grid.%05d.p%05d",param.nrestart,cpu.rank); 
    freeoct=restore_amr(filename,firstoct,lastoct,&tsim,&tinit,&nstepstart,&ndumps,&param,&cpu,part,adt);
    cpu.freeoct=freeoct;

    nstepstart+=1.; // next timestep is n+1
    ndumps+=1.; // next timestep is n+1


    if(cpu.rank==0){
      printf(" ... Restarting from file #%d with nstep=%d tsim=%e ndumps=%d\n",param.nrestart,nstepstart,tsim,ndumps);
    }


#ifdef TESTCOSMO
    // temporal boundaries of the full run
    ainit=tinit;
#endif

    MPI_Barrier(cpu.comm);
    /* int errcode=777; */
    /* MPI_Abort(cpu.comm,errcode); */

  }
  //===================================================================================================================================
  //===================================================================================================================================
  //===================================================================================================================================
  //===================================================================================================================================
  //===================================================================================================================================
  //===================================================================================================================================
  //===================================================================================================================================
  //===================================================================================================================================
  //===================================================================================================================================
  //===================================================================================================================================



#ifdef TESTCOSMO
  // ================== COMPUTATION OF FRIEDMANN TABLES
  REAL treal,treal0,trealBB;
  // we compute the friedmann tables
  aexp=tsim;

  // at this stage we have to compute the conformal time
  tsim=-0.5*sqrt(cosmo.om)*integ_da_dt_tilde(aexp,1.0,cosmo.om,cosmo.ov,1e-8);

  // real times in units of 1./H0
  treal=-integ_da_dt(aexp,1.0,cosmo.om,cosmo.ov,1e-8);
  trealBB=-integ_da_dt(1e-5,1.0,cosmo.om,cosmo.ov,1e-8);
  treal0=treal;


  // interpolation table
#ifdef WMPI
      MPI_Barrier(cpu.comm);
#endif
      if(cpu.rank==0) printf("computing friedmann tables with ainit=%e amax=%e\n",ainit,amax);
  compute_friedmann(ainit*0.95,amax,NCOSMOTAB,cosmo.om,cosmo.ov,tab_aexp,tab_ttilde,tab_t);
  
  tmax=-0.5*sqrt(cosmo.om)*integ_da_dt_tilde(amax,1.0+1e-6,cosmo.om,cosmo.ov,1e-8);
  if(cpu.rank==0) printf("tmax=%e treal=%e\n",tmax,treal);
  cosmo.tab_aexp=tab_aexp;
  cosmo.tab_ttilde=tab_ttilde;
#endif

  param.time_max=tmax;

  if(cpu.rank==0) dumpHeader(&param,&cpu);

  //================================================================================
  //================================================================================
  //================================================================================
  //
  //          AT THIS STAGE THE INITIAL SETUP HAS BEEN COMPLETED
  //
  //================================================================================
  //================================================================================
  //================================================================================


  int pass;
  int smark;
  int ismooth,nsmooth=2;
  int marker;
  FILE *fegy;

    //==================================== MAIN LOOP ================================================
    //===============================================================================================
  
 

    // preparing freeocts
    cpu.freeoct=freeoct;

#ifdef PIC
    // preparing free part
    cpu.freepart=freepart;
    cpu.lastpart=part+npartmax-1;
#endif

#ifdef GPUAXL
    // creating params on GPU
#ifdef WRAD
    create_param_GPU(&param,&cpu); 
    // Note : present only in radiation routines but could be useful elsewhere ... to check
#endif
#endif

    // preparing energy stats

    //if(cpu.rank==0) param.fpegy=fopen("energystat.txt","w");
#ifdef TESTCOSMO
    tdump=interp_aexp(tsim,cosmo.tab_aexp,cosmo.tab_ttilde);
#else
    tdump=tsim;
#endif
    // dumping ICs
    cpu.ndumps=&ndumps; // preparing the embedded IO
    cpu.tinit=tinit;
    int ptot=0;
    mtot=multicheck(firstoct,ptot,param.lcoarse,param.lmax,cpu.rank,&cpu,0);
    sprintf(filename,"data/start.%05d.p%05d",0,cpu.rank);
    dumpgrid(levelmax,firstoct,filename,tdump,&param);
#ifdef PIC
    sprintf(filename,"data/pstart.%05d.p%05d",0,cpu.rank);
    dumppart(firstoct,filename,levelcoarse,levelmax,tdump,&cpu);
#endif




    // Loop over time
    for(nsteps=nstepstart;(nsteps<=param.nsteps)*(tsim<tmax);nsteps++){

      cpu.nsteps=nsteps;
      
#ifdef TESTCOSMO
      cosmo.aexp=interp_aexp(tsim,cosmo.tab_aexp,cosmo.tab_ttilde);
      cosmo.tsim=tsim;
      if(cpu.rank==0) printf("\n============== STEP %d aexp=%e z=%lf tconf=%e tmax=%e================\n",nsteps,cosmo.aexp,1./cosmo.aexp-1.,tsim,tmax);
#else
#ifndef WRAD
      if(cpu.rank==0) printf("\n============== STEP %d tsim=%e ================\n",nsteps,tsim);
#else
      if(cpu.rank==0) printf("\n============== STEP %d tsim=%e [%e Myr] ================\n",nsteps,tsim,tsim*param.unit.unit_t/MYR);
#endif
#endif

      // Resetting the timesteps

      for(level=1;level<=levelmax;level++){
	ndt[level-1]=0;
      }

      //Recursive Calls over levels
      double tg1,tg2;
      MPI_Barrier(cpu.comm);
      tg1=MPI_Wtime();
      Advance_level(levelcoarse,adt,&cpu,&param,firstoct,lastoct,stencil,&gstencil,rstencil,ndt,nsteps,tsim);
      MPI_Barrier(cpu.comm);
      tg2=MPI_Wtime();
      if(cpu.rank==0) printf("GLOBAL TIME = %e\n",tg2-tg1);


      // ==================================== dump
      if((nsteps%(param.ndumps)==0)||((tsim+adt[levelcoarse-1])>=tmax)){
#ifndef EDBERT
	// dumping fields only
	dumpIO(tsim+adt[levelcoarse-1],&param,&cpu,firstoct,adt,0);
#else
	
#ifndef TESTCOSMO
#ifdef WRAD
	tdump=(tsim+adt[levelcoarse-1])*param.unit.unit_t/MYR;
#else
	tdump=(tsim+adt[levelcoarse-1]);
#endif
#else
	tdump=interp_aexp(tsim+adt[levelcoarse-1],cosmo.tab_aexp,cosmo.tab_ttilde);
	adump=tdump;
	printf("tdump=%e tsim=%e adt=%e\n",tdump,tsim,adt[levelcoarse-1]);
#ifdef EDBERT

	treal=-integ_da_dt(tdump,1.0,cosmo.om,cosmo.ov,1e-8); // in units of H0^-1
	tdump=(treal-trealBB)/(treal0-trealBB);
#endif
#endif

	// === Hydro dump
	
	//printf("tdum=%f\n",tdump);
	sprintf(filename,"data/grid.%05d.p%05d",ndumps,cpu.rank); 
	if(cpu.rank==0){
	  printf("Dumping .......");
	  printf("%s\n",filename);
	}
	dumpgrid(levelmax,firstoct,filename,adump,&param); 

#ifdef PIC
	sprintf(filename,"data/part.%05d.p%05d",ndumps,cpu.rank);
	if(cpu.rank==0){
	  printf("Dumping .......");
	  printf("%s\n",filename);
	}
	dumppart(firstoct,filename,levelcoarse,levelmax,tdump,&cpu);
#endif

	// backups for restart
	sprintf(filename,"bkp/grid.%05d.p%05d",ndumps,cpu.rank); 
	save_amr(filename,firstoct,tdump,tinit,nsteps,ndumps,&param, &cpu,part,adt);
#ifdef PIC
	sprintf(filename,"bkp/part.%05d.p%05d",ndumps,cpu.rank); 
	save_part(filename,firstoct,param.lcoarse,param.lmax,tdump,&cpu,part);
#endif
	ndumps++;
#endif
      }
      
      //==================================== timestep completed, looping
      dt=adt[param.lcoarse-1];
      tsim+=dt;
    }
    

    // we are done let's free the ressources

    free(adt);
    free(ndt);


#ifdef GPUAXL

#ifdef WGRAV
    destroy_pinned_gravstencil(&gstencil,gstride);
    destroy_gravstencil_GPU(&cpu,gstride);
#endif

#ifdef WHYDRO2
    destroy_pinned_stencil(&stencil,hstride); 
    destroy_hydstencil_GPU(&cpu,hstride); 
#endif

#ifdef WRAD
    destroy_pinned_stencil_rad(&rstencil,rstride); 
    destroy_radstencil_GPU(&cpu,rstride); 
#endif

#endif




    if(cpu.rank==0){
      //fclose(param.fpegy);
      printf("Done .....\n");
    }
#ifdef WMPI
    MPI_Barrier(cpu.comm);
    MPI_Finalize();
#endif

    return 0;
}

