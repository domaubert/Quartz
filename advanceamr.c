#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "prototypes.h"
#include "amr.h"
#include "hydro_utils.h"
#ifdef WRAD
#include "rad_utils.h"
#include "src_utils.h"
#endif
#include "friedmann.h"
#include "cic.h"
#include "particle.h"

// ===============================================================
// ===============================================================

void dispndt(struct RUNPARAMS *param, struct CPUINFO *cpu, int *ndt){
  
  int level;
  int ndtmax=pow(param->nsubcycles,param->lmax-param->lcoarse+1);
  int i,j,na;
  int nl;

  if(cpu->rank==0){
    printf("\n");
    for(j=0;j<ndtmax;j++)printf("#");
    printf("\n");
  }
  for(level=param->lcoarse;level<=param->lmax;level++){
    na=pow(2,param->lmax-level+1);
    nl=ndt[level-1];

#ifdef WMPI
    int nlmax;
    MPI_Allreduce(&nl,&nlmax,1,MPI_INT,MPI_MAX,cpu->comm);
    //printf("levef=%d rank=%d nl=%d nmax=%d\n",level,cpu->rank,nl,nlmax);
    nl=nlmax;
#endif

    if(cpu->rank==0){
      for(j=0;j<nl;j++){
      //one arrow
      for(i=0;i<(na-1);i++) printf("~");
      printf(">");
      }
      for(j=nl;j<ndtmax;j++) printf(" ");
      printf("\n");
    }
  }
  

  if(cpu->rank==0){
  for(j=0;j<ndtmax;j++)printf("#");
  printf("\n \n");
  }  
}

// ===============================================================
#ifdef WHYDRO2
REAL L_comptstep_hydro(int level, struct RUNPARAMS *param,struct OCT** firstoct, REAL fa, REAL fa2, struct CPUINFO* cpu, REAL tmax){
  
  struct OCT *nextoct;
  struct OCT *curoct;
  REAL dxcur;
  int icell;
  REAL aa;
  REAL va,vx,vy,vz;
  REAL dt;
  REAL Smax=0.,S1;

  //Smax=fmax(Smax,sqrt(Warray[i].u*Warray[i].u+Warray[i].v*Warray[i].v+Warray[i].w*Warray[i].w)+Warray[i].a);
  // Computing new timestep
  dt=tmax;
  // setting the first oct
  
  nextoct=firstoct[level-1];
  
  if(nextoct!=NULL){
    dxcur=pow(0.5,level); // +1 to protect level change
    do // sweeping through the octs of level
      {
	curoct=nextoct;
	nextoct=curoct->next;
	for(icell=0;icell<8;icell++) // looping over cells in oct
	  {
	    vx=curoct->cell[icell].field.u; 
	    vy=curoct->cell[icell].field.v; 
	    vz=curoct->cell[icell].field.w; 
	    va=sqrt(vx*vx+vy*vy+vz*vz); 
	    aa=sqrt(GAMMA*curoct->cell[icell].field.p/curoct->cell[icell].field.d); 
	    Smax=fmax(Smax,va+aa); 

	  }
      }while(nextoct!=NULL);
  }

  dt=fmin(dxcur*CFL/(Smax*3.),dt);

  /* #ifdef WMPI */
  /*   // reducing by taking the smallest time step */
  /*   MPI_Allreduce(MPI_IN_PLACE,&dt,1,MPI_REAL,MPI_MIN,cpu->comm); */
  /* #endif   */

  return dt;
}
#endif

// ===============================================================
#ifdef WGRAV
REAL L_comptstep_ff(int level,struct RUNPARAMS *param,struct OCT** firstoct, REAL aexp, struct CPUINFO* cpu, REAL tmax){
  
  struct OCT *nextoct;
  struct OCT *curoct;
  REAL dxcur;
  int icell;
  REAL dtloc;
  REAL dt;

  dt=tmax;
  // setting the first oct
      
  nextoct=firstoct[level-1];
      
  if(nextoct!=NULL){
    dxcur=pow(0.5,level); // +1 to protect level change
    do // sweeping through the octs of level
      {
	curoct=nextoct;
	nextoct=curoct->next;
	for(icell=0;icell<8;icell++) // looping over cells in oct
	  {
	    dtloc=0.1*sqrt(2.*M_PI/(3.*curoct->cell[icell].gdata.d*aexp));
	    dt=fmin(dt,dtloc);
	  }
      }while(nextoct!=NULL);
  }
  return dt;
}
#endif

// ===============================================================

#ifdef WRAD
REAL L_comptstep_rad(int level, struct RUNPARAMS *param,struct OCT** firstoct, REAL aexp, struct CPUINFO* cpu, REAL tmax){
  
  struct OCT *nextoct;
  struct OCT *curoct;
  REAL dxcur;
  int icell;
  REAL dtloc;
  REAL dt;

  dt=tmax;
  // setting the first oct
      
  nextoct=firstoct[level-1];
      
  if(nextoct!=NULL){
    dxcur=pow(0.5,level); // +1 to protect level change
    dt=CFL*dxcur/(aexp*param->clight*LIGHT_SPEED_IN_M_PER_S/param->unit.unit_v)/3.0; // UNITS OK
  }

  return dt;
}
#endif





// ===============================================================
// ===============================================================

REAL Advance_level(int level,REAL *adt, struct CPUINFO *cpu, struct RUNPARAMS *param, struct OCT **firstoct,  struct OCT ** lastoct, struct HGRID *stencil, struct STENGRAV *gstencil, struct RGRID *rstencil,int *ndt, int nsteps,REAL tloc){
 
#ifdef TESTCOSMO
  struct COSMOPARAM *cosmo;
  cosmo=param->cosmo;
#endif
   struct OCT *curoct;
  REAL dtnew;
  REAL dt=0.;
  REAL dtold;
  REAL dtvel;
  REAL dtfine;
  int npart=0;
  int mtot;
  int is;
  REAL aexp;
  int nsource;
  int hstride=param->hstride;
  int gstride=param->gstride;
  int nsub=param->nsubcycles;
  int ptot=0;
  int ip;
  
  REAL ekp,epp;
  REAL RHS;
  REAL delta_e;
  REAL drift_e;
  REAL htilde;


#ifdef TESTCOSMO
  aexp=cosmo->aexp;
#else
  aexp=1.0;
#endif

  if(cpu->rank==0){
    printf("\n === entering level =%d with gstride=%d hstride=%d sten=%p aexp=%e adt=%e\n",level,gstride,hstride,stencil,aexp,adt[level-1]);
  }


  // ==================================== Check the number of particles and octs
  ptot=0; for(ip=1;ip<=param->lmax;ip++){
    ptot+=cpu->npart[ip-1]; // total of local particles
    /* if((level==12)&&(cpu->rank==217)){ */
    /*   printf("l=%ip n=%ip\n",ip,cpu->npart[ip-1]); */
    /* } */
  }
  mtot=multicheck(firstoct,ptot,param->lcoarse,param->lmax,cpu->rank,cpu,0);

  if(level==param->lcoarse){
    // =========== Grid Census ========================================
    grid_census(param,cpu);
  }
  
  is=0;
#ifdef PIC
  //reset substep index of the particles
  L_reset_is_part(level,firstoct);
#endif

  
 
  // ========================== subcycling starts here ==================

  do{

    if(cpu->rank==0){
      printf("----\n");
      printf("subscyle #%d subt=%e nsub=%d\n",is,dt,nsub);
    }

#ifdef TESTCOSMO
    aexp=interp_aexp(tloc,cosmo->tab_aexp,cosmo->tab_ttilde);
    //aexp=cosmo->aexp;
#endif


    
    // =============================== cleaning 
#ifdef WHYDRO2
    clean_new_hydro(level,param,firstoct,cpu);
#endif


#ifdef PIC
    L_clean_dens(level,param,firstoct,cpu);
#endif

#ifdef WRAD
    clean_new_rad(level,param,firstoct,cpu,aexp);
#endif

    // == Ready to advance




  // ================= I we refine the current level
  if((param->lmax!=param->lcoarse)&&(level<param->lmax)){

    if(ndt[level-1]%2==0){
      // enforcing the 2 levels rule
      L_check_rule(level,param,firstoct,cpu);
      
#ifdef WMPI
      mpi_cic_correct(cpu, cpu->sendbuffer, cpu->recvbuffer, 3);
      mpi_exchange_level(cpu,cpu->sendbuffer,cpu->recvbuffer,3,1,level); // propagate the rule check
#ifdef WHYDRO2
      mpi_exchange_hydro(cpu,cpu->hsendbuffer,cpu->hrecvbuffer,0); // propagate hydro for refinement
#endif
#ifdef WRAD
      mpi_exchange_rad_level(cpu,cpu->Rsendbuffer,cpu->Rrecvbuffer,0,level); // propagate rad for refinement
#endif
#endif
      // refining (and destroying) octs
      curoct=L_refine_cells(level,param,firstoct,lastoct,cpu->freeoct,cpu,firstoct[0]+param->ngridmax,aexp);
      cpu->freeoct=curoct;
    }
    // ==================================== Check the number of particles and octs
    ptot=0; for(ip=1;ip<=param->lmax;ip++){
      ptot+=cpu->npart[ip-1]; // total of local particles
    }
    mtot=multicheck(firstoct,ptot,param->lcoarse,param->lmax,cpu->rank,cpu,1);

    ptot=0; for(ip=1;ip<=param->lmax;ip++){
      ptot+=cpu->npart[ip-1]; // total of local particles
      /* if((level==11)&&(cpu->rank==217)){ */
      /* 	printf("AP l=%ip n=%ip\n",ip,cpu->npart[ip-1]); */
      /* } */
    }
    
#ifdef WMPI
    //reset the setup in case of refinement
    setup_mpi(cpu,firstoct,param->lmax,param->lcoarse,param->ngridmax,1); // out of WMPI to compute the hash table
    MPI_Barrier(cpu->comm);
#endif
  }
  
 


 // ================= IV advance solution at the current level


    if(cpu->rank==0) printf("ndt=%d nsteps=%d\n",ndt[param->lcoarse-1],nsteps);


#ifdef PIC
    // ==================================== performing the CIC assignement
    L_cic(level,firstoct,param,cpu);
    MPI_Barrier(cpu->comm);
    if(cpu->rank==0) printf("Local CIC done\n");
#ifdef WMPI
    mpi_cic_correct(cpu, cpu->sendbuffer, cpu->recvbuffer, 0);
    mpi_exchange(cpu,cpu->sendbuffer, cpu->recvbuffer,1,1); 
#endif 
    MPI_Barrier(cpu->comm);
    if(cpu->rank==0) printf("CIC done\n");
#endif
    
    /* //==================================== Getting Density ==================================== */
#ifdef WGRAV

    FillDens(level,param,firstoct,cpu);  // Here Hydro and Gravity are coupled

    /* //====================================  Poisson Solver ========================== */
    PoissonSolver(level,param,firstoct,cpu,gstencil,gstride,aexp); 




    /* //====================================  Force Field ========================== */
#ifdef WMPI
    mpi_exchange(cpu,cpu->sendbuffer, cpu->recvbuffer,2,1);
#endif 
    PoissonForce(level,param,firstoct,cpu,gstencil,gstride,aexp);
#endif

#ifdef PIC
    // Mid point rule correction step
    if((is>0)||(cpu->nsteps>0)){
      L_accelpart(level,firstoct,adt,-1,cpu); // computing the particle acceleration and velocity
      // here -1 forces the correction : all particles must be corrected
    }
    L_levpart(level,firstoct,is); // assigning all the particles to the current level
#endif


    /* //====================================  I/O======= ========================== */
    // at this stage particles are synchronized at aexp
    // ready to dump particles
    // (note fields are dumped in quartz.c

#ifndef EDBERT
    if(level==param->lcoarse){
      if((cpu->nsteps%(param->ndumps)==0)||(tloc>=param->time_max)){
	if(cpu->rank==0) printf(" tsim=%e adt=%e\n",tloc,adt[level-1]);
	dumpIO(tloc,param,cpu,firstoct,adt,1);
      }
    }
#endif

    // ================= II We compute the timestep of the current level

    REAL adtold=adt[level-1]; // for energy conservation
    dtnew=param->dt;//*(cpu->nsteps>2);


    // Overshoot tmax
    dtnew=((param->time_max-tloc)<dtnew?(param->time_max-tloc):dtnew);
    //printf("dtnew=%e %e %e",dtnew,param->time_max,tloc);
    //printf("aexp=%e tloc=%e param->tmax=%e dtnew=%e ",aexp,tloc,param->time_max,dtnew);

    // Free Fall
#ifdef WGRAV
    REAL dtff;
#ifdef TESTCOSMO
    dtff=L_comptstep_ff(level,param,firstoct,aexp,cpu,1e9);
#else
    dtff=L_comptstep_ff(level,param,firstoct,1.0,cpu,1e9);
#endif
    dtnew=(dtff<dtnew?dtff:dtnew);
    printf("dtff= %e ",dtff);
#endif


    // Cosmo Expansion
#ifdef TESTCOSMO
    REAL dtcosmo;
    dtcosmo=-0.5*sqrt(cosmo->om)*integ_da_dt_tilde(aexp*1.05,aexp,cosmo->om,cosmo->ov,1e-8);
    dtnew=(dtcosmo<dtnew?dtcosmo:dtnew);
    printf("dtcosmo= %e ",dtcosmo);
#endif


    // Courant Condition Hydro
#ifdef WHYDRO2
    REAL dthydro;
    dthydro=L_comptstep_hydro(level,param,firstoct,1.0,1.0,cpu,1e9);
    //printf("dtnew=%e dthydro= %e ",dtnew,dthydro);
    dtnew=(dthydro<dtnew?dthydro:dtnew);
#endif

    // Courant Condition Particle
#ifdef PIC
    REAL dtpic;
    dtpic=L_comptstep(level,param,firstoct,1.0,1.0,cpu,1e9);
    printf("dtpic= %e \n",dtpic);
    dtnew=(dtpic<dtnew?dtpic:dtnew);
#endif

    // Courant Condition Radiation
#ifdef WRAD
    REAL dtrad;
    dtrad=L_comptstep_rad(level,param,firstoct,aexp,cpu,1e9);
    //printf("dtrad= %e ",dtrad);
    dtnew=(dtrad<dtnew?dtrad:dtnew);
#endif

#ifdef FLOORDT
    // REALLY WEIRD ===
    // Apparently there are some truncation errors in REDUCTION operation on double
    // that makes multi-processor dt different than the mono processor ones !
    // the few lines below removes this...

    REAL dtf=floor(dtnew*1e10)/1e10;
    dtnew=dtf;
#endif

    // SOME hack to force refinement before integration
    /* if(nsteps==0){ */
    /*   printf("skip udpate\n"); */
    /*   dtnew=0.; */
    /* } */

    /// ================= Assigning a new timestep for the current level
    dtold=adt[level-1];
    adt[level-1]=dtnew;
    if(dtnew==0.){
      printf("WARNING NULL dt rank %d n %d dt %e\n",cpu->rank,cpu->noct[level-1],dtnew);
      //abort();
    }
    if(level==param->lcoarse) adt[level-2]=adt[level-1]; // we synchronize coarser levels with the coarse one

    // the coarser level may be shorter than the finer one
    if(adt[level-2]<adt[level-1]){
      adt[level-1]=adt[level-2];
    }
    else{ // otherwise standard case
      adt[level-1]=fmin(adt[level-1],adt[level-2]-dt);// we force synchronization
    }

#ifdef WMPI
    REAL tdum=0.;
    REAL tdum2=0.;
    MPI_Allreduce(adt+level-1,&tdum,1,MPI_DOUBLE,MPI_MIN,cpu->comm);
    MPI_Allreduce(adt+level-2,&tdum2,1,MPI_DOUBLE,MPI_MIN,cpu->comm);
    adt[level-1]=tdum;
    adt[level-2]=tdum2;
#endif


   // ================= III Recursive call to finer level

  double tt2,tt1;
    MPI_Barrier(cpu->comm);
    tt1=MPI_Wtime();

    int nlevel=cpu->noct[level]; // number at next level
#ifdef WMPI
    MPI_Allreduce(MPI_IN_PLACE,&nlevel,1,MPI_INT,MPI_SUM,cpu->comm);
#endif


    if(level<param->lmax){
      if(nlevel>0){
	dtfine=Advance_level(level+1,adt,cpu,param,firstoct,lastoct,stencil,gstencil,rstencil,ndt,nsteps,tloc);
	// coarse and finer level must be synchronized now
	adt[level-1]=dtfine;
	if(level==param->lcoarse) adt[level-2]=adt[level-1]; // we synchronize coarser levels with the coarse one
      }
    }

#ifdef WMPI
    tdum=0.;
    tdum2=0.;
    MPI_Allreduce(adt+level-1,&tdum,1,MPI_DOUBLE,MPI_MIN,cpu->comm);
    MPI_Allreduce(adt+level-2,&tdum2,1,MPI_DOUBLE,MPI_MIN,cpu->comm);
    adt[level-1]=tdum;
    adt[level-2]=tdum2;
#endif

    MPI_Barrier(cpu->comm);
    tt2=MPI_Wtime();
    
    if(cpu->rank==0) printf("SUBLEVEL TIME lev=%d %e\n",level,tt2-tt1);
    // ==================================== Check the number of particles and octs
    ptot=0; for(ip=1;ip<=param->lmax;ip++) ptot+=cpu->npart[ip-1]; // total of local particles
    mtot=multicheck(firstoct,ptot,param->lcoarse,param->lmax,cpu->rank,cpu,2);

#ifdef PIC
    //================ Part. Update ===========================
#ifdef WMPI
    int maxnpart;
    MPI_Allreduce(cpu->npart+level-1,&maxnpart,1,MPI_DOUBLE,MPI_SUM,cpu->comm);
#endif
    if(cpu->rank==0) printf("Start PIC on %d part with dt=%e on level %d\n",maxnpart,adt[level-1],level);

    //printf("1 next=%p on proc=%d\n",firstoct[0]->next,cpu->rank);

    // =============== Computing Energy diagnostics

#ifdef PIC
    if(level==param->lcoarse){
      egypart(cpu,&ekp,&epp,param,aexp);
#ifdef WMPI
      REAL sum_ekp,sum_epp;
      MPI_Allreduce(&ekp,&sum_ekp,1,MPI_DOUBLE,MPI_SUM,cpu->comm);
      MPI_Allreduce(&epp,&sum_epp,1,MPI_DOUBLE,MPI_SUM,cpu->comm);
      ekp=sum_ekp;
      epp=sum_epp;
#endif
      if(nsteps==1){
	htilde=2./sqrt(param->cosmo->om)/faexp_tilde(aexp,param->cosmo->om,param->cosmo->ov)/aexp;
	param->egy_0=ekp+epp;
	param->egy_rhs=0.;
	param->egy_last=epp*htilde;
	param->egy_timelast=tloc;
	param->egy_totlast=ekp+epp;
	
      }
    }

    
#ifdef TESTCOSMO
    if((level==param->lcoarse)&&(nsteps>1)) {
      htilde=2./sqrt(param->cosmo->om)/faexp_tilde(aexp,param->cosmo->om,param->cosmo->ov)/aexp;
      RHS=param->egy_rhs;
      
      RHS=RHS+0.5*(epp*htilde+param->egy_last)*(tloc-param->egy_timelast); // trapezoidal rule

      delta_e=(((ekp+epp)-param->egy_totlast)/(0.5*(epp*htilde+param->egy_last)*(tloc-param->egy_timelast))-1.);
      drift_e=(((ekp+epp)-param->egy_0)/RHS-1.);

      param->egy_rhs=RHS;
      param->egy_last=epp*htilde;
      param->egy_timelast=tloc;
      param->egy_totlast=ekp+epp;


      if(cpu->rank==0){
	fprintf(param->fpegy,"%e %e %e %e %e %e\n",aexp,delta_e,drift_e,ekp,epp,RHS);
	printf("Egystat rel. err= %e drift=%e\n",delta_e,drift_e); 
      }
    }
#endif


    // moving particles around

    // predictor step
    L_accelpart(level,firstoct,adt,is,cpu); // computing the particle acceleration and velocity

#ifndef PARTN
    L_movepart(level,firstoct,adt,is,cpu); // moving the particles
#endif

    L_partcellreorg(level,firstoct); // reorganizing the particles of the level throughout the mesh

#ifdef WMPI
    //reset the setup in case of refinement
    //printf("2 next=%p on proc=%d\n",firstoct[0]->next,cpu->rank);
    setup_mpi(cpu,firstoct,param->lmax,param->lcoarse,param->ngridmax,1); // out of WMPI to compute the hash table
    MPI_Barrier(cpu->comm);
#endif

#ifdef WMPI

    int deltan;

    //printf("3 next=%p on proc=%d\n",firstoct[0]->next,cpu->rank);
    cpu->firstoct=firstoct;
    deltan=mpi_exchange_part(cpu, cpu->psendbuffer, cpu->precvbuffer);

    //printf("4 next=%p on proc=%d\n",firstoct[0]->next,cpu->rank);
    printf("proc %d receives %d particles freepart=%p\n",cpu->rank,deltan,cpu->freepart);
    //update the particle number within this process
    //npart=npart+deltan;
    ptot=deltan; for(ip=1;ip<=param->lmax;ip++) ptot+=cpu->npart[ip-1]; // total of local particles
    mtot=multicheck(firstoct,ptot,param->lcoarse,param->lmax,cpu->rank,cpu,3);
#endif


#endif
#endif




#ifdef WHYDRO2
    double th[10];
    MPI_Barrier(cpu->comm);
    th[0]=MPI_Wtime();
#ifdef WMPI
    mpi_exchange_hydro_level(cpu,cpu->hsendbuffer,cpu->hrecvbuffer,1,level);
#endif

    MPI_Barrier(cpu->comm);
    th[1]=MPI_Wtime();
    //=============== Hydro Update ======================
    HydroSolver(level,param,firstoct,cpu,stencil,hstride,adt[level-1]);

    MPI_Barrier(cpu->comm);
    th[2]=MPI_Wtime();

#ifdef WGRAV
    // ================================= gravitational correction for Hydro
    grav_correction(level,param,firstoct,cpu,adt[level-1]); // Here Hydro and Gravity are coupled
#endif

    MPI_Barrier(cpu->comm);
    th[3]=MPI_Wtime();

#ifdef WMPI
    if(level>param->lcoarse){
      mpi_hydro_correct(cpu,cpu->hsendbuffer,cpu->hrecvbuffer,level);
    }
    MPI_Barrier(cpu->comm);
#endif

    MPI_Barrier(cpu->comm);
    th[4]=MPI_Wtime();

    if(cpu->rank==0) printf("HYD -- Ex=%e HS=%e GCorr=%e HCorr=%e Tot=%e\n",th[1]-th[0],th[2]-th[1],th[3]-th[2],th[4]-th[3],th[4]-th[0]);

#endif


#ifdef WRAD
    double tcomp[10];
    MPI_Barrier(cpu->comm);
    tcomp[0]=MPI_Wtime();
    //=============== Building Sources and counting them ======================
    nsource=FillRad(level,param,firstoct,cpu,(level==param->lcoarse)&&(nsteps==0),aexp);  // Computing source distribution and filling the radiation fields
 
#ifdef WMPI
    MPI_Barrier(cpu->comm);
    tcomp[1]=MPI_Wtime();
    mpi_exchange_rad_level(cpu,cpu->Rsendbuffer,cpu->Rrecvbuffer,1,level);
    //mpi_exchange_rad(cpu,cpu->Rsendbuffer,cpu->Rrecvbuffer,(nsteps==0),level);
    MPI_Barrier(cpu->comm);
    tcomp[2]=MPI_Wtime();
#endif
    //=============== Radiation Update ======================
    RadSolver(level,param,firstoct,cpu,rstencil,hstride,adt[level-1],aexp);

#ifdef WMPI
    MPI_Barrier(cpu->comm);
    tcomp[3]=MPI_Wtime();
    if(level>param->lcoarse){
      mpi_rad_correct(cpu,cpu->Rsendbuffer,cpu->Rrecvbuffer,level);
    }
    MPI_Barrier(cpu->comm);
    tcomp[4]=MPI_Wtime();
#endif

    MPI_Barrier(cpu->comm);
    tcomp[5]=MPI_Wtime();
    //mpi_exchange_rad_level(cpu,cpu->Rsendbuffer,cpu->Rrecvbuffer,1,level);

    if(cpu->rank==0) printf("RAD -- Fill=%e Ex=%e RS=%e Corr=%e Tot=%e\n",tcomp[1]-tcomp[0],tcomp[2]-tcomp[1],tcomp[3]-tcomp[2],tcomp[4]-tcomp[3],tcomp[5]-tcomp[0]);


#ifdef WRADHYD
    if(cpu->rank==0) printf("TRADHYD l=%d Total=%e Noct=%d\n",level,tcomp[5]-th[0],cpu->noct[level-1]);
#endif

#endif



  // ================= V Computing the new refinement map
    

    if((param->lmax!=param->lcoarse)&&(level<param->lmax)){
      if((ndt[level-1]%2==1)||(level==param->lcoarse)){
      
#ifdef WHYDRO2
      mpi_exchange_hydro_level(cpu,cpu->hsendbuffer,cpu->hrecvbuffer,0,level); // propagate hydro for refinement
#endif
#ifdef WRAD
      mpi_exchange_rad_level(cpu,cpu->Rsendbuffer,cpu->Rrecvbuffer,0,level); // propagate rad for refinement
#endif
      
      // cleaning the marks
      L_clean_marks(level,firstoct);
      // marking the cells of the current level
      L_mark_cells(level,param,firstoct,param->nsmooth,param->amrthresh,cpu,cpu->sendbuffer,cpu->recvbuffer);
      }
    }


    // ====================== VI Some bookkeeping ==========
    dt+=adt[level-1]; // advance local time
    tloc+=adt[level-1]; // advance local time
    is++;
    ndt[level-1]++;

    if(cpu->rank==0) printf("is=%d ndt=%d dt=%le tloc=%le\n",is,ndt[level-1],tloc,dt);

    // Some Eye candy for timesteps display

    //dispndt(param,cpu,ndt);
    
    // === Loop

  }while((dt<adt[level-2])&&(is<nsub));
  
  
  if(cpu->rank==0){
    printf("--\n");
    printf("exiting level =%d\n",level);
  }

  return dt;

}
