#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "hilbert.h"
#include "prototypes.h"
#include <unistd.h>
#include <float.h>
#ifdef WHYDRO2
#include "hydro_utils.h"
#endif

void breakmpi()
{
  {
    int i = 0;
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    printf("PID %d on %s ready for attach\n", getpid(), hostname);
    fflush(stdout);
    while (0 == i)
      sleep(5);
  }
}

//==================================================================


 //------------------------------------------------------------------------
REAL multicheck(struct OCT **firstoct,int *npart,int levelcoarse, int levelmax, int rank, struct CPUINFO *cpu,struct RUNPARAMS *param, int label){

  int ntot;
  REAL ntotd;
  REAL nlevd;
  int level;
  struct OCT *nextoct;
  struct OCT *curoct;
  REAL dx,dv;
  int nlev,noct;
  int icell;
  struct PART *nexp;
  struct PART *curp;
  REAL mtot;
  REAL Mtot=0;
  REAL Mtotnew=0;
  REAL Etot=0;
  REAL Etotnew=0;
  REAL ptot=0;


  REAL xc,yc,zc;
  int *vnoct=cpu->noct;
  int *vnpart=cpu->npart;

  if(rank==0) printf("Check\n");
  ntot=0.;
  ntotd=0.;
  nlevd=0.;

#ifdef STARS
  int slev=0;
  int stot=0;
  int *vnstar=cpu->nstar;
#endif

  for(level=1;level<=levelmax;level++)
    {
      nextoct=firstoct[level-1];
      dx=1./POW(2,level);
      dv=POW(dx,3);
      nlev=0;
      nlevd=0.;
      noct=0;
#ifdef STARS
      slev=0;
#endif

      if(nextoct==NULL){
	vnoct[level-1]=0;
	vnpart[level-1]=0;
#ifdef STARS
	vnstar[level-1]=0;
#endif
	continue;
      }
      do // sweeping level
	{
	  curoct=nextoct;
	  nextoct=curoct->next;
	  if(curoct->cpu!=rank) continue;

	  if(level>=levelcoarse)
	    {
	//      if(rank==0) printf("np=%d nlev\n",nlev);
	      for(icell=0;icell<8;icell++) // looping over cells in oct
		{

//		  xc=curoct->x+(icell&1)*dx+dx*0.5;
//		  yc=curoct->y+((icell>>1)&1)*dx+dx*0.5;
//		  zc=curoct->z+(icell>>2)*dx+dx*0.5;

#ifdef WGRAV
		  ntotd+=curoct->cell[icell].density*dv;
		  nlevd+=curoct->cell[icell].density*dv;
#endif

#ifdef WHYDRO2
 		  if(curoct->cell[icell].child==NULL) Mtot +=curoct->cell[icell].field.d*dv;
		  if(curoct->cell[icell].child==NULL) Mtotnew +=curoct->cell[icell].fieldnew.d*dv;
		  struct Utype U;
		  struct Utype Unew;
		  W2U(&curoct->cell[icell].field,&U);
		  W2U(&curoct->cell[icell].fieldnew,&Unew);

		  if(curoct->cell[icell].child==NULL) Etot +=U.E*dv;
		  if(curoct->cell[icell].child==NULL) Etotnew +=Unew.E*dv;

		  if((curoct->cell[icell].field.d<=0)||isnan(curoct->cell[icell].field.u)){
		    if(cpu->rank==curoct->cpu){
		      printf("Negative or NAN value for density -> abort in multicheck %d\n",label);
		      printf("%e\t%e\t%e\t%e\t%e\t%e\t %d %d %d\n", curoct->cell[icell].field.d,curoct->cell[icell].field.u,curoct->cell[icell].field.v,curoct->cell[icell].field.w,curoct->cell[icell].field.p,curoct->cell[icell].field.E,cpu->rank,curoct->cpu,curoct->level);
			abort();
		    }
		  }
#endif

#ifdef PIC

		  //if(rank==0) printf("ic=%d %p\n",icell,nexp);
		  if((curoct->cell[icell].child!=NULL)&&(curoct->cell[icell].phead!=NULL)){
		    printf("check: split cell with particles !\n");
		    printf("curoct->cpu = %d curoct->level=%d\n",curoct->cpu,curoct->level);
		    abort();
		  }

		  nexp=curoct->cell[icell].phead; //sweeping the particles of the current cell
		  if(nexp==NULL)continue;
		  do{
		    curp=nexp;
		    nexp=curp->next;

		    nlev++;
		    ntot++;
#ifdef STARS
		    if (curp->isStar){
			slev++;
		 	stot++;

      if(curp->mass==0){
        printf("star mass == 0\n");
        abort();
      };
      Mtot+=curp->mass;
		    }
#endif // STARS


      REAL v2 = curp->vx*curp->vx+
                curp->vy*curp->vy+
                curp->vz*curp->vz;
      Etot+= 0.5*curp->mass*v2;
		  }while(nexp!=NULL);
#endif

		}
	    }
	  noct++;
	}while(nextoct!=NULL);
      //if((noct!=0)&&(level>=levelcoarse)) printf("proc %d level=%d npart=%d npartd=%f noct=%d\n",cpu->rank,level,nlev,nlevd,noct);

      if(level==levelcoarse) mtot=nlevd;
      vnoct[level-1] =noct;
      vnpart[level-1]=nlev;
#ifdef STARS
      vnstar[level-1]=slev;
#endif
    }

#ifdef WMPI
  MPI_Allreduce(MPI_IN_PLACE,&Mtot,1,MPI_REEL,MPI_SUM,cpu->comm);
  MPI_Allreduce(MPI_IN_PLACE,&Mtotnew,1,MPI_REEL,MPI_SUM,cpu->comm);
  MPI_Allreduce(MPI_IN_PLACE,&Etot,1,MPI_REEL,MPI_SUM,cpu->comm);
  MPI_Allreduce(MPI_IN_PLACE,&Etotnew,1,MPI_REEL,MPI_SUM,cpu->comm);
#endif

  if(cpu->rank==RANK_DISP){
    printf("Total Baryon mass=%e (new=%e) deviation=%e \n",Mtot,Mtotnew, Mtotnew-Mtot);
    printf("Total Baryon Egy=%e (new=%e) deviation=%e\n",Etot,Etotnew, Etotnew-Etot);

  }


  /* REAL tmw = param->cosmo->ob/param->cosmo->om ; */
  /* REAL dm = Mtot - tmw; */

  /* if(  FABS(dm) > 1e-6 && cpu->rank==RANK_DISP){ */
  /*   printf("\n=================================================\n"); */
  /*   printf("\tTotal Baryonic Mass \t\t %lf\n",Mtot); */
  /*   printf("\tTotal Baryonic Mass wanted \t %lf \n", tmw); */
  /*   printf("\tDelta \t\t\t\t %lf \n",dm);  */
  /*   printf("=================================================\n"); */
  /*   //	abort(); */
  /* } */


//printf("%d\t%d\t%d\n",cpu->rank,npart, ntot);
//printf("nPart %d\tnStar %d\n",npart[0], npart[1]);


#ifdef PIC
  if(label<7){
  //ultimate check
  if(npart[0]!=0){ // for initial call to multicheck that always provide a zero
    if(ntot!=npart[0]) {
      printf("ERROR npart=%d npart (counted)=%d abort on rank %d on stage %d\n",npart[0],ntot,cpu->rank,label);
      abort();
    }
#ifdef STARS
    if(stot!=npart[1]) {
      printf("ERROR nstar=%d nstar (counted)=%d abort on rank %d on stage %d\n",npart[1],stot,cpu->rank,label);
      abort();
    }
#endif

  }
  }
#endif

#ifdef WMPI
 MPI_Barrier(cpu->comm);
 if(rank==RANK_DISP) printf("Check done \n");
#endif

  return mtot;
}
 //------------------------------------------------------------------------
 //------------------------------------------------------------------------


void myradixsort(int *a,int n)
{
  int *b;
  int i,m=0,exp=1;
  b=(int*)calloc(n,sizeof(int));
  for(i=0;i<n;i++)
    {
      if(a[i]>m)
	m=a[i];
    }

  while(m/exp>0)
    {
      int bucket[10]={0};
      for(i=0;i<n;i++)
	bucket[a[i]/exp%10]++;
      for(i=1;i<10;i++)
	bucket[i]+=bucket[i-1];
      for(i=n-1;i>=0;i--)
	b[--bucket[a[i]/exp%10]]=a[i];
      for(i=0;i<n;i++)
	a[i]=b[i];
      exp*=10;
    }
  free(b);
}

//============================================================================
//============================================================================


void grid_census(struct RUNPARAMS *param, struct CPUINFO *cpu){

  int level;
  int ltot,gtot=0,nomax,nomin;
  int lpart,ptot=0;
#ifdef STARS
  int lstar;
#endif


  if(cpu->rank==RANK_DISP){
    printf("===============================================================\n");
  }
  for(level=2;level<=param->lmax;level++){
    ltot =cpu->noct[level-1];
    lpart=cpu->npart[level-1];
#ifdef STARS
    lstar=cpu->nstar[level-1];
#endif
    nomax=ltot;
    nomin=ltot;
    gtot+=ltot;
    ptot+=lpart;
#ifdef WMPI
    MPI_Allreduce(&ltot,&nomax,1,MPI_INT,MPI_MAX,cpu->comm);
    MPI_Allreduce(&ltot,&nomin,1,MPI_INT,MPI_MIN,cpu->comm);
    MPI_Allreduce(MPI_IN_PLACE,&ltot,1,MPI_INT,MPI_SUM,cpu->comm);
    MPI_Allreduce(MPI_IN_PLACE,&lpart,1,MPI_INT,MPI_SUM,cpu->comm);
#ifdef STARS
    MPI_Allreduce(MPI_IN_PLACE,&lstar,1,MPI_INT,MPI_SUM,cpu->comm);
    if(lstar>0) cpu->trigstar=1;
    lpart-=lstar;
#endif
#endif


    if(cpu->rank==RANK_DISP){
      if(ltot!=0) {printf("level=%2d noct=%9d min=%9d max=%9d npart=%9d",level,ltot,nomin,nomax,lpart);
#ifdef STARS
	printf(" nstar=%9d",lstar);
#endif
	int I;
	REAL frac=(ltot/(1.0*POW(2,3*(level-1))))*100.;
	printf(" [");
	for(I=0;I<12;I++) printf("%c",(I/12.*100<frac?'*':' '));
	printf("]\n");
      }
    }
  }
#ifdef WMPI
  MPI_Allreduce(MPI_IN_PLACE,&gtot,1,MPI_INT,MPI_MAX,cpu->comm);
#endif
  if(cpu->rank==RANK_DISP) printf("\n");

  if(cpu->rank==RANK_DISP){
    int I;
    REAL frac=(gtot/(1.0*param->ngridmax))*100.;
    printf("grid occupancy=%6.1f [",frac);
    for(I=0;I<24;I++) printf("%c",(I/24.*100<frac?'*':' '));
    printf("]\n");
  }

#ifdef PIC
#ifdef WMPI
  MPI_Allreduce(MPI_IN_PLACE,&ptot,1,MPI_INT,MPI_MAX,cpu->comm);
#endif

  if(cpu->rank==RANK_DISP){
    int I;
    REAL frac=(ptot/(1.0*param->npartmax))*100.;
    printf("part occupancy=%6.1f [",frac);
    for(I=0;I<24;I++) printf("%c",(I/24.*100<frac?'*':' '));
    printf("]\n");
  }
#endif

  if(cpu->rank==RANK_DISP){
    int I;
    REAL frac=(gtot/(1.0*POW(2,(param->lmax-1)*3)))*100.;
    printf("comp factor   =%6.1f [",frac);
    for(I=0;I<24;I++) printf("%c",(I/24.*100<frac?'*':' '));
    printf("]\n\n");
  }

/* #ifdef TESTCOSMO */
/*   if(ptot!=POW(2,param->lcoarse*3)){ */
/*     printf("Error on total number of particles %d found (%d expected)\n",ptot,(int)POW(2,3*param->lcoarse)); */
/*     abort(); */
/*   } */
/* #endif */


}


REAL rdm(REAL a, REAL b){
/// return a random number between a and b
	return 	(rand()/(REAL)RAND_MAX ) * (b-a) + a ;
}

unsigned int gpoiss(REAL lambda){
/// Poisson distribution
/// TODO use a better poisson algorithme

#ifdef SINGLEPRECISION
  const int kmax = log(FLT_MAX); // log( max(float32) )  = 88
#else
  const int kmax = log(DBL_MAX);// log( max(float64) ) = 708
#endif // SINGLEPRECISION

  int k=1;
  REAL p = rdm(0,1);
  REAL P = exp(-lambda);
  REAL sum=P;
  if (sum>=p){
    k=0;
  }
  else{
    do {
 	P*=lambda/(REAL)k;
	sum+=P;
	if (sum>=p) break;
	k++;
    }while(k<kmax);

    if (k==kmax) {
      printf("WARNING : numerical precision reached in Poisson drawning k=%d\n",k);
      abort();
    }
  }
  if(k==1e6) {
    printf("k=%d lambda=%e sum=%e p=%e\n",k,lambda,sum,p);
    k=lambda;
  }
  return k;
}
