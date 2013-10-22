#include "readhop.i"
#include "ascii2ls.i"
#include "plot2ls.i"
#include "STECKMAP/Pierre/POP/sfit.i"

// search for MW candidates
boxsize=60.; // Mpc
npart=256^3;
rhob=1.4e10; // Msun/Mpc^3
Mpart=(rhob*boxsize^3)/npart;

pos=read_pos("output_hop.pos");
pos*=boxsize;
sizes=read_size("zregroup.size");
masses=sizes*Mpart;
gpos=gpos=myread("zregroup.pos",0,nlines=numberof(sizes),ncol=9);
xg=boxsize*gpos(4,);
yg=boxsize*gpos(5,);
zg=boxsize*gpos(6,);
Rg=((gpos(3,))^(1./3.))*boxsize;
MWcand=where((masses>=1.e12)&(masses<=3.e12));
M31cand=where((masses>=5.e12)&(masses<=1.5e13));
write,pr1(numberof(MWcand))+" MW candidates";
write,pr1(numberof(M31cand))+" M31 candidates";

couple=array(0,[3,2,numberof(MWcand),numberof(M31cand)]);
dist=array(0.,[2,numberof(MWcand),numberof(M31cand)]);

//i=1;j=1;
for(i=1;i<=numberof(MWcand);i++){
  for(j=1;j<=numberof(M31cand);j++){
    couple(,i,j)=[MWcand(i),M31cand(j)];
    dist(i,j)=sqrt((xg(MWcand(i))-xg(M31cand(j)))^2+(yg(MWcand(i))-yg(M31cand(j)))^2+(zg(MWcand(i))-zg(M31cand(j)))^2);
  };
 };

cands=where(dist(*)<=1.5); // so M31 and MW are closer than 1Mpc
couplecands=couple(,*)(,cands);

ws;
//plcol,pos(3,::1000),pos(2,::1000),pos(1,::1000),marker=1;
pl,pos(2,::1000),pos(1,::1000),marker=1;
plcir,Rg,yg,xg,color="red",width=5;
plcir,Rg(couplecands(*)),yg(couplecands(*)),xg(couplecands(*)),color="green",width=5;

i=2;
xc=xg(couplecands(,i))(avg);
yc=yg(couplecands(,i))(avg);
zc=zg(couplecands(,i))(avg);
ccands=couplecands(,i);
depth=2.;
ind=where((pos(3,)>=zc-depth)&(pos(3,)<=zc+depth));
indg=where((zg>=zc-depth)&(zg<=zc+depth));
//ind=where(pos(3,)>=
ws;
//plcol,pos(3,ind)(::1),pos(2,ind)(::1),pos(1,ind)(::1),marker=1;
pl,pos(2,ind)(::1),pos(1,ind)(::1),marker=1;

plcir,Rg(indg),yg(indg),xg(indg),color="blue",width=5;
plcir,Rg(ccands(*)),yg(ccands(*)),xg(ccands(*)),color="green",width=5;
xyleg,"Mpc","Mpc";
pltitle,"MW & M31";
