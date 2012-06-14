

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "prototypes.h"

//=================================================================================================

int main(int argc, char *argv[])
{
  //(int lmap,struct OCT **firstoct,int field,char filename[],float tsim)
  int lmap;
  float *map;
  int imap,jmap,kmap;
  int nmap;
  float dxmap,dxcur;
  int icur,ii,jj,kk,ic,icell;
  int level;
  struct OCT * nextoct;
  struct OCT oct;
  FILE *fp;
  float xc,yc,zc;
  int field;
  float tsim=0.;
  char fname[256];
  char format[256];
  char fname2[256];
  int ncpu;
  int icpu;
  struct OCT *zerooct;


  if(argc!=6){
    printf("USAGE: /a.out input level field output nproc\n");
    printf("Ex : data/grid.00002 7 101 data/grid.den 8\n");
    printf("field values :\n");
    printf(" case 0 oct.level;\n case 1 density;\n case 2 pot;\n case 3 cpu;\n case 4 marked;\n case 5 temp;\n case 101 field.d;\n case 102 field.u;\n case 103 field.v;\n case 104 field.w;\n case 105 field.p;\n");
    return 0;
  }

  // getting the field 
  sscanf(argv[3],"%d",&field);

  //getting the resolution
  sscanf(argv[2],"%d",&lmap);
  nmap=pow(2,lmap);
  map=(float *)calloc(nmap*nmap*nmap,sizeof(float));
  dxmap=1./nmap;

  // getting the number of CPUs
  sscanf(argv[5],"%d",&ncpu);


  // scanning the cpus
  for(icpu=0;icpu<ncpu;icpu++){
    
    memset(map,0,nmap*nmap*nmap*sizeof(float));
    
    // building the input file name
    strcpy(format,argv[1]);
    strcat(format,".p%05d");
    sprintf(fname,format,icpu);

    // building the output file name
    strcpy(format,argv[4]);
    strcat(format,".p%05d");
    sprintf(fname2,format,icpu);


    fp=fopen(fname,"rb");
  
    if(fp==NULL){
      printf("--ERROR -- file not found\n");
      return 1;
    }
    else{
      printf("Casting Rays on %dx%dx%d cube from %s\n",nmap,nmap,nmap,fname);
    }

    // reading the time
    fread(&tsim,sizeof(float),1,fp);
    printf("tsim=%e\n",tsim);
    // reading the zerooct
    fread(&zerooct,sizeof(struct OCT *),1,fp);
    printf("0oct=%p\n",zerooct);

    ic=0;
    
    fread(&oct,sizeof(struct OCT),1,fp);
    while(!feof(fp)){
      ic++;
      dxcur=1./pow(2,oct.level);
      for(icell=0;icell<8;icell++) // looping over cells in oct
	{
	  if((oct.cell[icell].child==NULL)||(oct.level==lmap))
	    {
	      xc=oct.x+( icell   %2)*dxcur;//+0.5*dxcur;
	      yc=oct.y+((icell/2)%2)*dxcur;//+0.5*dxcur;
	      zc=oct.z+( icell/4   )*dxcur;//+0.5*dxcur;
	      imap=xc/dxmap;
	      jmap=yc/dxmap;
	      kmap=zc/dxmap;
	      
	      for(kk=0;kk<pow(2,lmap-oct.level);kk++)
		{
		  for(jj=0;jj<pow(2,lmap-oct.level);jj++)
		    {
		      for(ii=0;ii<pow(2,lmap-oct.level);ii++)
			{
			
			  switch(field){
			  case 0:
			    map[(imap+ii)+(jmap+jj)*nmap+(kmap+kk)*nmap*nmap]+=oct.level;
			    break;
#ifdef WGRAV
 			  case 1:
			    map[(imap+ii)+(jmap+jj)*nmap+(kmap+kk)*nmap*nmap]=oct.cell[icell].gdata.d;
			    break;
			  case 2:
			    map[(imap+ii)+(jmap+jj)*nmap+(kmap+kk)*nmap*nmap]+=oct.cell[icell].gdata.p;
			    break;
#endif
			  case 3:
			    map[(imap+ii)+(jmap+jj)*nmap+(kmap+kk)*nmap*nmap]=oct.cpu;
			    break;
			  case 4:
			    map[(imap+ii)+(jmap+jj)*nmap+(kmap+kk)*nmap*nmap]=oct.cell[icell].marked;
			    break;
			  case 5:
			    map[(imap+ii)+(jmap+jj)*nmap+(kmap+kk)*nmap*nmap]=oct.cell[icell].temp;
			    break;
#ifdef WHYDRO2
			  case 101:
			    map[(imap+ii)+(jmap+jj)*nmap+(kmap+kk)*nmap*nmap]=oct.cell[icell].field.d;
			    //printf("%f\n",oct.cell[icell].field.d);
			    break;
			  case 102:
			    map[(imap+ii)+(jmap+jj)*nmap+(kmap+kk)*nmap*nmap]=oct.cell[icell].field.u;
			    break;
			  case 103:
			    map[(imap+ii)+(jmap+jj)*nmap+(kmap+kk)*nmap*nmap]=oct.cell[icell].field.v;
			    break;
			  case 104:
			    map[(imap+ii)+(jmap+jj)*nmap+(kmap+kk)*nmap*nmap]=oct.cell[icell].field.w;
			    break;
			  case 105:
			    map[(imap+ii)+(jmap+jj)*nmap+(kmap+kk)*nmap*nmap]=oct.cell[icell].field.p;
			    break;
#endif			  
			  }
			}
		    }
		} 
	    }
	}
      
      fread(&oct,sizeof(struct OCT),1,fp); //reading next oct
      
    }
    
    fclose(fp);
    printf("done with %d cells\n",ic);
      
    //============= dump
    
    printf("dumping %s with nmap=%d\n",fname2,nmap);
    fp=fopen(fname2,"wb");
    fwrite(&nmap,1,sizeof(int),fp);
    fwrite(&tsim,1,sizeof(float),fp);
    fwrite(map,nmap*nmap*nmap,sizeof(float),fp);
    fclose(fp);
  }
  free(map);
  
}