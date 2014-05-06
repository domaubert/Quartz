#ifdef STARS 

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> 

#include "prototypes.h"
#include "particle.h"
#include "oct.h"
#include "hydro_utils.h"

#include "cic.h"
#include "segment.h"

#ifdef WMPI
#include <mpi.h>
#endif

/*
known t_car :
 2.1 Gyr (Kennicutt 1998)
 2.4 Gyr (Rownd & Young 1999)
 3.0 Gyr (Rasera & Teyssier 2006)
 3.2 Gyr (Springel & Hernquist 2003)
*/

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 		SOME FONCTIONS
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

REAL a2t(struct RUNPARAMS *param, REAL az ){
//  	from Ned Wright http://www.astro.ucla.edu/~wright/CC.python

	REAL age = 0.;
	REAL a, adot;
	REAL h = param->cosmo->H0/100;

	REAL or = 4.165e-5/(h*h); 
	REAL ok = 1. - param->cosmo->om - or - param->cosmo->ov;
	
	int i, n=1000;
	for (i=0; i<n;i++){
		a = az*(i+0.5)/n;
		adot = sqrt(ok+(param->cosmo->om / a)+(or/ (a*a) )+ (param->cosmo->ov*a*a) );
		age = age + 1./adot;
	}
	REAL zage = az*age/n;
	REAL zage_Gyr = (977.8 /param->cosmo->H0)*zage;

	return zage_Gyr*1e9;
}

REAL rdm(REAL a, REAL b){
	return 	(rand()/(REAL)RAND_MAX ) * (b-a) + a ;
}

int gpoiss(REAL lambda){

	int k=1;                
	REAL p = rdm(0,1); 	
	REAL P = exp(-lambda);  
	REAL sum=P;               
	if (sum>=p) return 0;     
	do { 	P*=lambda/(REAL)k;
		sum+=P;           
		if (sum>=p) break;
		k++;
  	}while(k<1e6);

	return k;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//		FEEDBACK
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

REAL getFeedbackEgy(struct RUNPARAMS *param, REAL aexp){

	REAL 	rhoE 	= 3.7e-15; 		// erg.g-1 Kay 2002 // 4e48 erg.Mo-1 springel hernquist 2003 -> OK	
		rhoE 	*= 1.0e-7 * 1e3; 	// erg.g-1  en J.kg-1

	REAL H0 	= param->cosmo->H0 *1000.0/1e6/PARSEC;
	REAL rho0 	= 3.0 * pow(H0,2.0) * param->cosmo->om /(8.0*PI*NEWTON_G) ;

	REAL Mtot 	= rho0 * pow(param->cosmo->unit_l,3.0);
	REAL M 		= param->stars->mstars * Mtot;	

	REAL E 		= M * rhoE;

	REAL rs		= param->cosmo->unit_l;
	REAL ts 	= 2.0/(H0 *sqrt(param->cosmo->om));
	REAL es		= pow(rs/ts,2);

	E 		*=  pow(aexp,2.0) / es;

	return E;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void thermalFeedback(struct CELL *cell, struct RUNPARAMS *param, REAL E ,REAL t0, REAL dt, REAL aexp){

	REAL s8 	= 2e7;		// life time of a 8 M0 star
	s8 		*= 31556926; 	// years en s 

	REAL H0 	= param->cosmo->H0 *1000.0/1e6/PARSEC;
	REAL ts 	= 2.0/(H0 *sqrt(param->cosmo->om));
	dt 		*= pow(aexp,2.0) * ts;

	E *= exp( -t0/s8 ) *dt/s8;

	cell->field.E  += E  ;   		 //thermal Feedback
	cell->field.p  += E  * (GAMMA-1.);

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void kineticFeedback(struct CELL *cell, struct RUNPARAMS *param, struct CPUINFO *cpu, REAL E ,REAL t0, REAL dt, REAL aexp){

	REAL H0 	= param->cosmo->H0 *1000.0/1e6/PARSEC;
	REAL ts 	= 2.0/(H0 *sqrt(param->cosmo->om));
	dt 		*= pow(aexp,2.0) * ts;

  	int nread = 0;
	int stride = 0;
	int hstride=fmax(8,param->hstride);//pow(2,levelcoarse);
/*	struct HGRID *stencil;
	stencil=(struct HGRID*)calloc(hstride,sizeof(struct HGRID));
	
	struct OCT *curoct = cell2oct(cell);
	gatherstencil(curoct,stencil,stride,cpu, &nread);
*/

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int feedback(struct CELL *cell, struct RUNPARAMS *param, struct CPUINFO *cpu, REAL aexp, REAL t, REAL dt){

	if(param->stars->feedback_eff){
		REAL E = getFeedbackEgy(param,aexp)  * param->stars->feedback_eff ;

		REAL t0;
		struct PART *nexp;


		struct PART *curp;

		nexp=cell->phead;
		if(nexp==NULL) return 0;
	 	do{ 	curp=nexp;
			nexp=curp->next;

			if (curp->isStar){
				t0 = t - curp->age;
				if (  t0>0 && t0<2e8 ) {
					thermalFeedback(cell, param      , E* (     param->stars->feedback_frac), t0, dt, aexp);
				//	kineticFeedback(cell, param, cpu , E* (1. - param->stars->feedback_frac), t0, dt, aexp);
				}
			}
		}while(nexp!=NULL);
	}
	return 1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//		STARS
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void initStar(struct CELL * cell, struct PART *star, struct RUNPARAMS *param, int level, REAL xc, REAL yc, REAL zc,int idx, REAL aexp, int is, REAL dt,REAL dx ) {

	star->next  = NULL;
	star->idx   = idx;
	star->level = level;
	star->is    = is;

	star->x = xc + rdm(-0.5,0.5) * dx;
	star->y = yc + rdm(-0.5,0.5) * dx;
	star->z = zc + rdm(-0.5,0.5) * dx;

	star->vx = cell->field.u;
	star->vy = cell->field.v;
	star->vz = cell->field.w;


/*	REAL r 		= rdm(0,1) * cell->field.a ;
	REAL theta   	= acos(rdm(-1,1));
	REAL phi 	= rdm(0,2*PI);

	star->vx += r * sin(theta) * cos(phi);
	star->vy += r * sin(theta) * sin(phi);
	star->vz += r * cos(theta) ;
*/


	star->mass  = param->stars->mstars;

	star->epot = 0.0;
	star->ekin = 0.5 * star->mass * (pow(star->vx,2) + pow(star->vy,2) + pow(star->vz,2));

	star->isStar = 1;
	star->age = a2t(param,aexp);
	star->rhocell = cell->field.d;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int testCond(struct CELL *cell, REAL dttilde, REAL dxtilde, struct RUNPARAMS *param, REAL aexp, int level){

	if (cell->child != NULL) return 0;

	int A = cell->field.d > param->stars->thresh;
	int B = 0?						cell->field.a/pow(2.,-level) > sqrt(6.*aexp * cell->gdata.d +1.) 				: 1;


	return 		A && B;


	//return (cell->field.d>param->denthresh)&&(cell->rfield.temp<param->tmpthresh);

	

//	printf("%e\t%e\n", cell->field.d, n*param->stars->mstars);

//	return cell->field.d > param->amrthresh * (param->cosmo->ob/param->cosmo->om) / pow(2.0,-3.0*level);

//	printf("%e\t%e\n", (cell->gdata.d + 1.0)*pow(2.0,-3.0*level) , param->amrthresh);

//	return 	(cell->gdata.d+1.0)*pow(2.0,-3.0*level) > param->amrthresh;


/*	REAL DVcoarse 	=  pow(2.0,-3.0*param->lcoarse);
	REAL DV		=  pow(2.0,-3.0*param->level);

	REAL rhobarre 	= (param->cosmo->ob/param->cosmo->om);

	REAL N 		= param->amrthresh / pow(2.0,-3.0*param->lcoarse);

	REAL rhocrit 	= N * rhobarre * DVcoarse/DV ;
	*/


//	return cell->field.d > param->stars->overdensity_cond * (param->cosmo->ob/param->cosmo->om) * pow(aexp,-alpha0) ;

/*	REAL rhocrit 	= param->stars->density_cond * PROTON_MASS;

	REAL H0 	= param->cosmo->H0 *1000.0/1e6/PARSEC;
	REAL a3rhostar 	= pow(aexp,	3.0) *  3.0 * pow(H0,2.0) * param->cosmo->om /(8.0*PI*NEWTON_G);

//	printf("H0\t%e\t\t a3rs\t%e\t\t a^-3\t%e\n",H0 , a3rhostar, pow(aexp,-alpha0) );
//	printf("rho\t%e\t\t rcrit\t%e\t\t seuil\t%e\n",cell->field.d , rhocrit,rhocrit/a3rhostar*pow(aexp,-alpha0) );
	return cell->field.d > rhocrit/a3rhostar*pow(aexp,-alpha0);
*/
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void conserveField(struct Wtype *field, struct RUNPARAMS *param, struct PART *star, REAL dx, REAL aexp){

	REAL drho = param->stars->mstars / pow(dx,3.);
	struct Utype U;
	struct Wtype W;


	memcpy(&W,field,sizeof(struct Wtype));

	W2U(&W, &U);

//	density
	U.d    -= drho;

	if (U.d <=0) {
		printf("negative density in conserveField from createStars \t field->d %e U.d %e drho %e ", field->d, U.d, drho);
		abort();
	}

//	momentum
	U.du -= star->vx * drho;		
	U.dv -= star->vy * drho;		
	U.dw -= star->vz * drho;		

//	internal energy
/*	REAL H0 = param->cosmo->H0 *1000.0/1e6/PARSEC;
	REAL rs		= param->cosmo->unit_l;
	REAL ts 	= 2.0/(H0 *sqrt(param->cosmo->om));
	REAL es		= pow(rs/ts,2);

	U.eint -= (-3./5. * NEWTON_G *  pow(param->stars->mstars,2) / (dx * pow(4./3.*PI,-1./3.) ))*  pow(aexp,2.0) / es  ; //Viriel	
*/

	U2W(&U, &W);

//	total energy
	getE(&(W));
	W.a=sqrt(GAMMA*W.p/W.d);

	memcpy(field,&W,sizeof(struct Wtype));

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int getNstars2create(struct CELL *cell, struct RUNPARAMS *param, REAL dttilde, REAL aexp, int level){
	
	REAL gas_efficiency = 1.;	// maybe need to be passed in param??

	REAL H0 = param->cosmo->H0 *1000.0/1e6/PARSEC;
	REAL a2ts = pow(aexp,2.0) * 2.0/(H0 *sqrt(param->cosmo->om));


	REAL tstars 	= param->stars->tcar * 31556926 / sqrt(cell->field.d / param->stars->thresh );
	REAL tstartilde = tstars / a2ts;

	REAL M_in_cell = cell->field.d * pow(2.0,-3.0*level);

	REAL lambda =  gas_efficiency * M_in_cell / param->stars->mstars * dttilde/ tstartilde;

	REAL N = gpoiss(lambda);
	

	if(N * param->stars->mstars >= M_in_cell ) N = M_in_cell / param->stars->mstars ;

//	if (N) printf("M in cell %e \t Mstars %e \t dt %e\t dtt %e\t d %e \t l %e  \n",M_in_cell, param->stars->mstars, dttilde, tstartilde, cell->field.d, lambda);

	return N;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void addStar(struct CELL *cell, int level, REAL xc, REAL yc, REAL zc, struct CPUINFO *cpu, REAL dttilde, struct RUNPARAMS *param, REAL aexp, int is,  int nstars){

	struct PART *star = cpu->freepart;

	if (star==NULL){
		if( cpu->rank==0){
			printf("\n");
			printf("----------------------------\n");
			printf("No more memory for particles\n");
			printf("----------------------------\n");
		}
		exit(0);
	}else{

		cpu->freepart 		= cpu->freepart->next;
		cpu->freepart->prev 	= NULL;

		if (cell->phead==NULL){
			cell->phead 		= star;
			star->prev 		= NULL;				
		}else{
			struct PART *lasp 	= findlastpart(cell->phead);
			lasp->next 		= star;
			star->prev 		= lasp;
		}

		cpu->npart[level-1]++;
		cpu->nstar[level-1]++;
	
		REAL dx = pow(2.0,-level);

		initStar(cell, star, param, level, xc, yc, zc, param->stars->n+nstars, aexp, is, dttilde, dx);	

		conserveField(&(cell->field   ), param, star,  dx, aexp);
		conserveField(&(cell->fieldnew), param, star,  dx, aexp);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void initThresh(struct RUNPARAMS *param,  REAL aexp){

	REAL k =(param->stars->density_cond >0)? 1 : -pow(aexp,3.0);
	

	REAL H0 		= param->cosmo->H0 *1000.0/1e6/PARSEC;
	REAL rhostar 		= 3.0 * pow(H0,2.0) * param->cosmo->om /(8.0*PI*NEWTON_G);

	REAL rhocrittilde 	= param->stars->density_cond * PROTON_MASS;

	param->stars->thresh    = fmax( k * rhocrittilde / rhostar, param->stars->overdensity_cond * (param->cosmo->ob/param->cosmo->om));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void createStars(struct OCT **firstoct, struct RUNPARAMS *param, struct CPUINFO *cpu, REAL dt, REAL aexp, int level, int is){

	struct OCT  *curoct;
	struct OCT  *nextoct=firstoct[level-1];
	struct CELL *curcell;

	REAL xc, yc, zc;
	REAL dx = pow(2.0,-level);
	REAL mmax = 0;
	REAL mmin = 1e9;

	REAL t = a2t(param,aexp) ;

	int l,icell, ipart;
	int nstars = 0;
	int nsl = 0;
	int N;


	initThresh(param, aexp);
	param->stars->mstars	= (param->cosmo->ob/param->cosmo->om) * pow(2.0,-3.0*param->lcoarse);
	param->cosmo->tphy	= a2t(param, aexp);


	if(cpu->rank==0){
		printf("\n");
		printf("================================\n");
		printf("   Starting Add Stars routine   \n");
		printf("================================\n");	
	}

	do {	if(nextoct==NULL) 		continue;
		curoct=nextoct;
		nextoct=curoct->next;
		if(curoct->cpu != cpu->rank) 	continue;

	      	for(icell=0;icell<8;icell++) {
			curcell = &curoct->cell[icell];

			if( testCond(curcell, dt, dx, param, aexp, level) ) {
				xc=curoct->x+( icell    & 1)*dx+dx*0.5; 
				yc=curoct->y+((icell>>1)& 1)*dx+dx*0.5;
				zc=curoct->z+( icell>>2    )*dx+dx*0.5; 										

				N = getNstars2create(curcell, param, dt, aexp, level);

			//	if(N) printf("N_Rho_Temp_Seuil_z\t%d\t%e\t%e\t%e\t%e\n", N, curcell->field.d, curcell->rfield.temp, param->stars->thresh,1.0/aexp - 1.0  );

				if (N * param->stars->mstars >= curcell->field.d *dx*dx*dx){
					printf("Problem mass in createStars %e %e \n", N * param->stars->mstars, curcell->field.d *dx*dx*dx);
					abort();
				}

				for (ipart=0;ipart< N; ipart++){
						addStar(curcell, level, xc, yc, zc, cpu, dt, param, aexp, is,nstars++);						
				}
			}

			//feedback(curcell, param, cpu, aexp, t, dt);

			mmax = fmax(curcell->field.d, mmax);
			mmin = fmin(curcell->field.d, mmin);
		}
	}while(nextoct!=NULL);

#ifdef WMPI
	MPI_Allreduce(MPI_IN_PLACE,&nstars,1,MPI_INT,   MPI_SUM,cpu->comm);
	MPI_Allreduce(MPI_IN_PLACE,&mmax,  1,MPI_DOUBLE,MPI_MAX,cpu->comm);
	MPI_Allreduce(MPI_IN_PLACE,&mmin,  1,MPI_DOUBLE,MPI_MIN,cpu->comm);
#endif

	param->stars->n += nstars ;
	if(cpu->rank==0) {
		printf("Mmax_thresh_overthresh\t%e\t%e\t%e\n", mmax, param->stars->thresh, param->stars->overdensity_cond * (param->cosmo->ob/param->cosmo->om) );
		printf("%d stars added on level %d \n", nstars, level);
		printf("%d stars in total\n",param->stars->n);
		printf("\n");
	}

	for (l = param->lcoarse; l<=param->lmax;l++){
#ifdef WMPI
		MPI_Allreduce(&cpu->nstar[l-1],&nsl,1,MPI_INT,   MPI_SUM,cpu->comm);
		MPI_Barrier(cpu->comm);
#endif
		if(cpu->rank==0 && nsl) {	printf("%d stars on level %d \n", nsl, l);	}
	}
	if(cpu->rank==0) {	printf("\n");}
}
#endif
