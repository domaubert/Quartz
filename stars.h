REAL a2t(struct RUNPARAMS *param, REAL az );
REAL rdm(REAL a, REAL b);
int gpoiss(REAL lambda);
REAL getFeedbackEgy(struct RUNPARAMS *param, REAL aexp, int level);
void thermalFeedback(struct CELL *cell, struct RUNPARAMS *param, REAL t0, REAL aexp, int level, REAL dt );
int feedback(struct CELL *cell, struct RUNPARAMS *param, struct CPUINFO *cpu, REAL aexp, int level, REAL dt);
void initStar(struct CELL *cell, struct PART *star,struct RUNPARAMS *param, int level,  REAL m ,REAL xc, REAL yc, REAL zc, int idx, REAL aexp, int is, REAL dthydro);
int  testCond(struct CELL *curcell,REAL dt, REAL dx, struct RUNPARAMS *param, REAL aexp, int level);
void conserveField(struct Wtype *field, struct RUNPARAMS *param, struct PART *star, REAL dx, REAL aexp);
int getNstars2create(struct CELL *cell, struct RUNPARAMS *param, REAL dttilde, REAL aexp, int level);
int  addStar(struct CELL * cell, int level, REAL xc, REAL yc, REAL zc, struct CPUINFO *cpu, REAL dt,struct RUNPARAMS *param, REAL aexp, REAL drho, int is, REAL dthydro, int nstars);
void initThresh(struct RUNPARAMS *param,  REAL aexp);
void createStars(struct OCT **firstoct, struct RUNPARAMS *param, struct CPUINFO *cpu, REAL dt,REAL aexp, int level, int is);

