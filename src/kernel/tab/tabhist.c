/*
 * TABHIST: histogram plotter program for ascii data in tabular format
 *          
 *
 *	17-Mar-88  V1.0 :  created by P.J.Teuben
 *	15-apr-88  V1.1 :  added higher order moment, scaling in Y  PJT
 *	 1-jun-88  V2.0 :  just name changed nemohist->tabhist      PJT
 *				mallocation of x[] now done
 *			a: added labels along axes		PJT
 *	25-nov-88  V2.1 :  minmax calulation corrected	PJT
 *	26-sep-89  V2.2 :  tab= keyword added to ignore plot    PJT
 *	13-nov-90  V2.3 :  drange->nemoinpd PJT
 *	20-nov-90  V2.3a:  fixed out-of-range bug		PJT
 *	14-jun-91      b:  format statement needed more acc.	PJT
 *	26-mar-92  V2.4 :  stole crummy ascii histo from miriad PJT
 *	 2-nov-93  V2.6 :  use moments() routines - fix upper edge bug PJT
 *		      a :  use file_lines() and  nmax		pjt
 *	13-nov-93  V2.7 : add optional gaussian overlay plot 	pjt
 *	 4-mar-94     a : ansi header - fixed ylabel bug	pjt
 *	 3-aug-95     b : also report true minmax if min=max= used	pjt
 *	21-aug-95     c : fixed default X and Y labels on plot	pjt
 *	 1-sep-95     d : prototypes
 *	11-jul-96  V2.8 : log units now based 10, was base e	pjt
 *	20-nov-96     a : report median
 *      10-jan-96  V2.9 : optionally not show the residual      pjt
 *	20-feb-97     a : fixed for SINGLEPREC			pjt
 *      25-mar-97  V2.10: allow all columns to be used          pjt
 *                          ** unfinished **
 *	12-apr-97  V3.0:  allow cumulative histogram            pjt
 *	24-apr-97   3.0a: fix median calculation when limits set   pjt
 *	15-feb-99   3.1:  median can be turned off now		pjt
 *	22-dec-99   3.1a: fix reporting bug when 1 point read	pjt
 *       3-jun-01   3.2 : added nsigma=                         pjt
 *	23-sep-01      b: ->nemo_file_lines
 *       7-may-03   4.0 : allow multiple columns                pjt
 *      12-nov-04   4.1 : added Sum
 *      28-jan-05   5.0 : major overhaul:
 *                        allow either xmin or xmax set, 
 *                        sortidx -> sort selected by name
 *                        fixed median & histogram if nsigma outliers 
 *      11-mar-05   5.1   added xcoord= keyword                 pjt
 *       7-apr-05   5.2   under and overflow reporting fixed    pjt
 *                        
 * 
 * TODO:
 *     option to do dual-pass to subtract the mean before computing
 *     the higher order moments - needed for accuracy
 *   allow bins= to give the actual bin edges, e.g.
 *     bins=1:100:0.5
 */

/**************** INCLUDE FILES ********************************/ 

#include <stdinc.h>
#include <strlib.h>
#include <getparam.h>
#include <moment.h>
#include <yapp.h>
#include <axis.h>
#include <mdarray.h>

/**************** COMMAND LINE PARAMETERS **********************/

string defv[] = {
    "in=???\n                     Input file name",
    "xcol=1\n			  Column(s) to use",
    "xmin=\n			  Set minimum, if no autoscale needed",
    "xmax=\n			  Set maximum, if no autoscale needed",
    "bins=16\n			  Number of bins",
    "maxcount=0\n		  Maximum along count-axis",
    "nmax=100000\n		  maximum number of data to be read if pipe",
    "ylog=f\n			  log scaling in Y?",
    "xlab=$in[$xcol]\n	          Optional Label along X",
    "ylab=N\n			  Optional Label along Y",
    "headline=\n		  Optional headline in graph",
    "tab=f\n			  Table (t) or Plot( f)",
    "gauss=t\n			  Overlay gaussian plot?",
    "residual=t\n		  Overlay residual data-gauss(fit)",
    "cumul=f\n                    Override and do cumulative histogram instead",
    "median=t\n			  Compute median too (can be time consuming)",
    "nsigma=-1\n                  delete points more than nsigma",
    "xcoord=\n		          Draw additional vertical coordinate lines along these X values",
    "sort=qsort\n                 Sort mode {qsort;...}",
    "dual=f\n                     Dual pass for large number",
    "scale=1\n                    Scale factor for data",
    "VERSION=5.4\n		  11-feb-07 PJT",
    NULL
};

string usage = "General tabular 1D statistics and histogram plotter";

string cvsid = "$Id$";

/**************** SOME GLOBAL VARIABLES ************************/

#ifndef MAXHIST
#define MAXHIST	1024
#endif

#ifndef MAXCOL
#define MAXCOL 256
#endif

#define MAXCOORD 16

local string input;			/* filename */
local stream instr;			/* input file */

local int ncol;                         /* number of columns used */
local int col[MAXCOL];			/* histogram column number(s) */
local real   xrange[2];			/* range of  histogram values */
local int    nsteps;			/* number of divisions */

local real   *x = NULL;			/* pointer to array of nmax*ncol points */
local int    *iq = NULL;                /* boolean masking array */
local int    nmax;			/* lines to use at all */
local int    npt;			/* actual number of points */
local real   xmin, xmax;		/* actual min and max as computed */
local real   nsigma;                    /* outlier rejection attempt */
local bool   Qauto;			/* autoscale ? */
local bool   Qmin, Qmax;                /* denotes if xmin or xmax were specified */
local bool   Qgauss;                    /* gaussian overlay ? */
local bool   Qresid;                    /* gaussian residual overlay ? */
local bool   Qtab;                      /* table output ? */
local bool   Qcumul;                    /* cumulative histogram ? */
local bool   Qmedian;			/* compute median also ? */
local bool   Qdual;                     /* dual pass ? */
local int    maxcount;
local int    Nunder, Nover;             /* number of data under or over min/max */
local real   dual_mean;                 /* mean value, if dual pass used */
local real   scale;                     /* scale factor */

local string headline;			/* text string above plot */
local string xlab, ylab, xlab2;		/* text string along axes */
local bool   ylog;			/* count axis in logarithmic scale? */
local real   xplot[2],yplot[2];		/* borders of plot */

local real xcoord[MAXCOORD];            /* coordinate lines */
local int nxcoord;

local iproc  mysort, getsort();

local real xtrans(real), ytrans(real);
local void setparams(void), read_data(void), histogram(void);
local iproc getsort(string name);



/****************************** START OF PROGRAM **********************/

nemo_main()
{
    setparams();			/* read the parameters */
    read_data();
    histogram();
}

local int compar_real(real *a, real *b)
{
  return *a < *b ? -1 : *a > *b ? 1 : 0;
}

local void setparams()
{
    input = getparam("in");
    ncol = nemoinpi(getparam("xcol"),col,MAXCOL);
    if (ncol < 0) error("parsing error col=%s",getparam("col"));
    
    nsteps=getiparam("bins");
    if (nsteps > MAXHIST) 
        error("bins=%d too large; MAXHIST=%d",nsteps,MAXHIST);
    Qmin = hasvalue("xmin");
    Qmax = hasvalue("xmax");
    if (Qmin) xrange[0] = getdparam("xmin");
    if (Qmax) xrange[1] = getdparam("xmax");
    if (Qmin && Qmax && xrange[0] >= xrange[1]) error("Need xmin < xmax");
    Qauto = (!Qmin || !Qmax) ;

    maxcount=getiparam("maxcount");
    headline = getparam("headline");
    ylog=getbparam("ylog");
    xlab=getparam("xlab");
    ylab=getparam("ylab");
    Qgauss = getbparam("gauss");
    Qresid = getbparam("residual");
    Qtab = getbparam("tab");
    Qcumul = getbparam("cumul");
    if (Qcumul) {
        Qgauss=Qresid=FALSE;
        ylog=FALSE;
    }
    Qmedian = getbparam("median");
    if (ylog && streq(ylab,"N")) ylab = scopy("log(N)");
    Qdual = getbparam("dual");

    nmax = nemo_file_lines(input,getiparam("nmax"));
    if (nmax<1) error("Problem reading from %s",input);

    nxcoord = nemoinpr(getparam("xcoord"),xcoord,MAXCOORD);

    nsigma = getdparam("nsigma");
    mysort = getsort(getparam("sort"));
    scale = getrparam("scale");
    if (scale != 1.0) {
      int n1=strlen(xlab);
      string s2 = getparam("scale");
      int n2=strlen(s2);
      xlab2 = (string) allocate(n1+n2+20);
      sprintf(xlab2,"%s [scale *%s]",xlab,s2);
      xlab = xlab2;
    }
    instr = stropen (input,"r");
}



local void read_data()
{
    real *coldat[MAXCOL];
    int   i,j,k;
    mdarray2 md2 = allocate_mdarray2(ncol,nmax);

    dprintf(0,"Reading %d column(s)\n",ncol);
    for (i=0; i<ncol; i++)
      coldat[i] = md2[i];
    npt = get_atable(instr,ncol,col,coldat,nmax);        /* read it */
    if (npt == -nmax) {
    	warning("Could only read %d data",nmax);
    	npt = nmax;
    }
    if (scale != 1.0) {
      warning("Scale factor=%g\n",scale);
      for (i=0, k=0; i<ncol; i++)
	for (j=0; j<npt; j++)
	  md2[i][j] *= scale;
    }
    x = (real *) allocate(npt*ncol*sizeof(real));
    if (Qdual) {
      warning("dual=t is a new test option");
      /* pass over the data, finding the mean */
      for (i=0, k=0; i<ncol; i++)
	for (j=0; j<npt; j++) {
	  dual_mean += md2[i][j];
      }
      dual_mean /= (ncol*npt);
      dprintf(0,"Dual pass mean       : %g\n",dual_mean);
    } else
      dual_mean = 0.0;

    Nunder = Nover = 0;
    for (i=0, k=0; i<ncol; i++) {
      for (j=0; j<npt; j++) {
	md2[i][j] -= dual_mean;
	if (Qmin && md2[i][j] < xrange[0]) { Nunder++; continue;}
	if (Qmax && md2[i][j] > xrange[1]) { Nover++;  continue;}
	x[k++] = md2[i][j];
      }
    }
    npt = k;
    dprintf(0,"Under/Over flow: %d %d\n",Nunder,Nover);

    free_mdarray2(md2,ncol,nmax);

    minmax(npt, x, &xmin, &xmax);
    if (!Qmin) xrange[0] = xmin;
    if (!Qmax) xrange[1] = xmax;


    /*  allocate index arrray , and compute sorted index for median */
    if (Qmedian) 
      (mysort)(x,npt,sizeof(real),compar_real);
}


local void histogram(void)
{
        int i,j,k, l, kmin, kmax, lcount = 0;
	real count[MAXHIST], under, over;
	real xdat,ydat,xplt,yplt,dx,r,sum,sigma2, q, qmax;
	real mean, sigma, skew, kurt, lmin, lmax, median;
	Moment m;

	dprintf (0,"read %d values\n",npt);
	dprintf (0,"min and max value in column(s)  %s: [%g : %g]\n",getparam("xcol"),xmin,xmax);
	if (!Qauto) {
	    xmin = xrange[0];
	    xmax = xrange[1];
	    dprintf (0,"min and max value reset to : [%g : %g]\n",xmin,xmax);
	    lmin = xmax;
	    lmax = xmin;
	    for (i=0; i<npt; i++) {
	    	if (x[i]>xmin && x[i]<=xmax) {
	    	    lmin = MIN(lmin, x[i]);
   	    	    lmax = MAX(lmax, x[i]);
	    	}
	    }
	    dprintf (0,"min and max value in range : [%g : %g]\n",lmin,lmax);
	} 

	for (k=0; k<nsteps; k++)
		count[k] = 0;		/* init histogram */
	under = over = 0;

	ini_moment(&m,4,0);
	for (i=0; i<npt; i++) {
		if (xmax != xmin)
		    k = (int) floor((x[i]-xmin)/(xmax-xmin)*nsteps);
		else
		    k = 0;
		dprintf(1,"%d k=%d %g\n",i,k,x[i]);
		if (k==nsteps && x[i]==xmax) k--;     /* include upper edge */
		if (k<0)       { under++; continue; }
		if (k>=nsteps) { over++;  continue; }
		count[k] = count[k] + 1;
		dprintf (4,"%d : %f %d\n",i,x[i],k);
		accum_moment(&m,x[i],1.0);
	}
	if (under > 0) error("bug: under = %d",under);
	if (over  > 0) error("bug: over = %d",over);
	under = Nunder;
	over  = Nover;

	mean = mean_moment(&m);
	sigma = sigma_moment(&m);
	skew = skewness_moment(&m);
	kurt = kurtosis_moment(&m);

	if (nsigma > 0) {    /* remove outliers iteratively, starting from the largest */
	  iq = (int *) allocate(npt*sizeof(int));
	  for (i=0; i<npt; i++) {
#if 1
	    iq[i] = x[i] < xmin  || x[i] > xmax;
#else
	    iq[i] = 0;
#endif
	  }
	  lcount = 0;
	  do {               /* loop to remove outliers one by one */
	    qmax = -1.0;
	    for (i=0, l=-1; i<npt; i++) {     /* find largest deviation from current mean */
	      if (iq[i]) continue;            /* but skip previously flagged points */
	      q = (x[i]-mean)/sigma;
	      q = ABS(q);
	      if (q > qmax) {
		qmax = q;
		l = i;
	      }
	    }
	    if (qmax > nsigma) {
	      lcount++;
	      iq[l] = 1;
	      decr_moment(&m,x[l],1.0);
	      mean = mean_moment(&m);
	      sigma = sigma_moment(&m);
	      skew = skewness_moment(&m);
	      kurt = kurtosis_moment(&m);
	      dprintf(1,"%d/%d: removing point %d, m/s=%g %g qmax=%g\n",
		      lcount,npt,l,mean,sigma,qmax);
	      if (sigma <= 0) {
		/* RELATED TO presetting MINMAX */
		warning("BUG");
		accum_moment(&m,x[l],1.0);
		mean = mean_moment(&m);
		sigma = sigma_moment(&m);
		skew = skewness_moment(&m);
		kurt = kurtosis_moment(&m);
		dprintf(1,"%d/%d: LAST removing point %d, m/s=%g %g qmax=%g\n",
		      lcount,npt,l,mean,sigma,qmax);
		break;
		
	      }

	    } else
	      dprintf(1,"%d/%d: keeping point %d, m/s=%g %g qmax=%g\n",
		      lcount,npt,l,mean,sigma,qmax);

	    /* if (lcount > npt/2) break; */
	  } while (qmax > nsigma);
	  dprintf(0,"Removed %d/%d points for nsigma=%g\n",lcount,npt,nsigma);

	  /* @algorithm      left shift array values from mask array */
	  /* now shift all points into the array, decreasing npt */
	  /* otherwise the median is not correctly computed */
	  for (i=0, k=0; i<npt; i++) {
	    dprintf(1,"iq->%d\n",iq[i]);
	    if (iq[i]) k++;
	    if (k==0) continue;  /* ?? */
	    if (i-k < 0) continue;
	    dprintf(1,"SHIFT: %d <= %d\n",i-k,i);
	    x[i-k] = x[i];
	  }
	  npt -= lcount;   /* correct for outliers */
	  free(iq);
	} /* nsigma > 0 */

	if (npt != n_moment(&m))
	  error("Counting error, probably in removing outliers...");
	dprintf (0,"Number of points     : %d\n",npt);
	if (npt>1)
	  dprintf (0,"Mean and dispersion  : %g %g %g\n",mean,sigma,sigma/sqrt(npt-1.0));
	else
	  dprintf (0,"Mean and dispersion  : %g %g 0.0\n",mean,sigma);
	dprintf (0,"Skewness and kurtosis: %g %g\n",skew,kurt);
	if (Qmedian) {

	  if (npt % 2) 
            median = x[(npt-1)/2];
	  else
            median = 0.5 * (x[npt/2] + x[npt/2-1]);
	  dprintf (0,"Median               : %g\n",median);
	}
	dprintf (0,"Sum                  : %g\n",show_moment(&m,1));

	if (lcount > 0) {
	  warning("Recompute histogram because of outlier removals");
	  /* recompute histogram if we've lost some outliers */
	  for (k=0; k<nsteps; k++)
	    count[k] = 0;		/* init histogram */
	  under = over = 0;
	  for (i=0; i<npt; i++) {
	    if (xmax != xmin)
	      k = (int) floor((x[i]-xmin)/(xmax-xmin)*nsteps);
	    else
	      k = 0;
	    if (k==nsteps && x[i]==xmax) k--;     /* include upper edge */
	    if (k<0)       { under++; continue; }
	    if (k>=nsteps) { over++;  continue; }
	    count[k] = count[k] + 1;
	    dprintf (4,"%d : %f %d\n",i,x[i],k);
	  }
	  if (under > 0 || over > 0) error("under=%d over=%d in recomputed histo",under,over);
	}

	dprintf (3,"Histogram values : \n");
        dx=(xmax-xmin)/nsteps;
	kmax=0;
	sum=0.0;
	for (k=0; k<nsteps; k++) {
	        sum = sum + dx*count[k];
		if (ylog) {
		    if (count[k]>0.0)
                        count[k] = log10(count[k]);
                    else
                        count[k] = -1.0;
		}
		if (count[k]>kmax)
			kmax=count[k];
		dprintf (3,"%f ",count[k]);
                if (Qcumul) {
                    if (k==0)
                        count[k] += under;
                    else
                        count[k] += count[k-1];
                }
	}
	dprintf (3,"\n");
        sigma2 = 2.0 * sigma * sigma;	/* gaussian */
        sum /= sigma * sqrt(2*PI);	/* scaling factor for equal area gauss */

	if (ylog && over>0)  over =  log10(over);
	if (ylog && under>0) under = log10(under);

	kmax *= 1.1;		/* add 10% */
        if (Qcumul) kmax = npt;
	if (maxcount>0)		/* force scaling by user ? */
		kmax=maxcount;	

	if (Qtab) {
            maxcount = 0;
            for (k=0; k<nsteps; k++)
                maxcount = MAX(maxcount,count[k]);
            if (maxcount>0)
                r = 29.0/maxcount;
            else
                r = 1.0;
            printf("  Bin    Value          Number\n");
            printf("       Underflow   %d\n",Nunder);
            for (k=0; k<nsteps; k++) {
                j = (int) (r*count[k]) + 1;
                if (ylog) printf("%3d %13.6g %13.6g ", 
				k+1, xmin+(k+0.5)*dx, count[k]);
		else printf("%3d %13.6g %8d ", 
				k+1, xmin+(k+0.5)*dx, (int)count[k]);
                while (j-- > 0) printf("*");
                printf("\n");
            }
            printf("       Overflow    %d\n",Nover);
	    stop(0);
	}

#ifdef YAPP
	/*	PLOTTING */	
	plinit("***",0.0,20.0,0.0,20.0);

	xplot[0] = xmin;
	xplot[1] = xmax;
	yplot[0] = 0.0;
	yplot[1] = (real) kmax;
	xaxis (2.0, 2.0, 16.0, xplot, -7, xtrans, xlab);
	xaxis (2.0, 18.0,16.0, xplot, -7, xtrans, NULL);
	yaxis (2.0, 2.0, 16.0, yplot, -7, ytrans, ylab);
	yaxis (18.0, 2.0, 16.0, yplot, -7, ytrans, NULL);

        pljust(-1);     /* set to left just */
        pltext(input,2.0,18.2,0.32,0.0);             /* filename */
        pljust(1);
        pltext(headline,18.0,18.2,0.24,0.0);         /* headline */
        pljust(-1);     /* return to left just */

	xdat=xmin;
	dx=(xmax-xmin)/nsteps;
	plmove(xtrans(xmin),ytrans(0.0));
	for (k=0; k<nsteps; k++) {	/* nsteps= */
		xplt = xtrans(xdat);
		yplt = ytrans((real)count[k]);
		plline (xplt,yplt);
		xdat += dx;
		xplt = xtrans(xdat);
		plline (xplt,yplt);	
	}
	plline(xplt,ytrans(0.0));

	for (i=0; i<nxcoord; i++) {
	  plmove(xtrans(xcoord[i]),ytrans(yplot[0]));
	  plline(xtrans(xcoord[i]),ytrans(yplot[1]));
	}

	if (Qgauss) {                   /* plot model and residuals */
            if (ylog)
		plmove(xtrans(xmin),ytrans(-1.0));
            else
		plmove(xtrans(xmin),ytrans(0.0));
	    for (k=0; k<100; k++) {
	        xdat = xmin + (k+0.5)*(xmax-xmin)/100.0;
	        ydat = sum * exp( -sqr(xdat-mean)/sigma2);
	        if (ylog) ydat = log10(ydat);
	        plline(xtrans(xdat), ytrans(ydat));
	    }
        }

        if (Qresid) {

	    plltype(0,2);   /* dotted residuals */
	    xdat = xmin+0.5*dx;
            dprintf(1,"# residuals from gauss\n");
	    for (k=0; k<nsteps; k++, xdat +=dx) {
	        ydat = sum * exp( -sqr(xdat-mean)/sigma2);
	        dprintf(1,"%g %g %g\n",xdat,count[k],ydat);
	        if (ylog) ydat = log10(ydat);
                ydat = count[k] - ydat;
                if (k==0)
                    plmove(xtrans(xdat),ytrans(ydat));
                else
                    plline(xtrans(xdat),ytrans(ydat));
	    }
	    plltype(0,1);   /* back to normal line type */

	}
	plstop();
#endif
}

local real xtrans(real x)
{
	return (2.0 + 16.0*(x-xplot[0])/(xplot[1]-xplot[0]));
}

local real ytrans(real y)
{
	return (2.0 + 16.0*(y-yplot[0])/(yplot[1]-yplot[0]));
}

/*
 *  Tabulate the valid sort names, plus their associated external
 *  routines. The accompanying getsort() routine returns the
 *  appropriate sort routine
 */
 
typedef struct sortmode {
    string name;
    iproc   fie;
} sortmode;
 
#define SortName(x)  x->name
#define SortProc(x)  x->fie
 
 
/* List of externally available sort routines */
 
/* extern int qsort();         /* Standard Unix : stdlib.h */
/* or:  void qsort(void *base, size_t nmemb, size_t size,
 *                 int(*compar)(const void *, const void *));
 */

#ifdef FLOGGER
                   
extern int bubble_sort();          /* Flogger library routines */
extern int heap_sort();
extern int insertion_sort();
extern int merge_sort();
extern int quick_sort();
extern int shell_sort();

#endif

local sortmode smode[] = {
#ifdef FLOGGER
    "bubble",   bubble_sort,        /* cute flogger routines */
    "heap",     heap_sort,
    "insert",   insertion_sort,
    "merge",    merge_sort,
    "quick",    quick_sort,
    "shell",    shell_sort,
#endif
    "qsort",    (iproc) qsort,      /* standard Unix qsort() */
    NULL, NULL,
};
                                                                                
local iproc getsort(string name)
{
    sortmode *s;
                                                                                
    for (s=smode; SortName(s); s++)  {
        dprintf(1,"GETSORT: Trying %s\n",SortName(s));
        if (streq(SortName(s),name))
            return SortProc(s);
    }
    error("%s: no valid sortname",name);
    return NULL;     /* better not get here ... */
}
