/*
 * $Id: mdrun.c,v 1.139.2.9 2009/05/04 16:13:29 hess Exp $
 *
 *                This source code is part of
 *
 *                 G   R   O   M   A   C   S
 *
 *          GROningen MAchine for Chemical Simulations
 *
 *                        VERSION 3.2.0
 * Written by David van der Spoel, Erik Lindahl, Berk Hess, and others.
 * Copyright (c) 1991-2000, University of Groningen, The Netherlands.
 * Copyright (c) 2001-2004, The GROMACS development team,
 * check out http://www.gromacs.org for more information.

 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * If you want to redistribute modifications, please consider that
 * scientific software is very special. Version control is crucial -
 * bugs must be traceable. We will be happy to consider code for
 * inclusion in the official distribution, but derived work must not
 * be called official GROMACS. Details are found in the README & COPYING
 * files - if they are missing, get the official version at www.gromacs.org.
 *
 * To help us fund GROMACS development, we humbly ask that you cite
 * the papers on the package - you can find them in the top README file.
 *
 * For more info, check our website at http://www.gromacs.org
 *
 * And Hey:
 * Gallium Rubidium Oxygen Manganese Argon Carbon Silicon
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <signal.h>
#include <stdlib.h>
#include "typedefs.h"
#include "smalloc.h"
#include "sysstuff.h"
#include "vec.h"
#include "statutil.h"
#include "macros.h"
#include "copyrite.h"
#include "main.h"
#include "futil.h"
#include "edsam.h"
#include "index.h"
#include "physics.h"
#include "names.h"
#include "mtop_util.h"
#include "tpxio.h"
#include "string2.h"
#include "gmx_membed.h"
#include "pbc.h"
#include "readinp.h"

typedef struct {
	int		id;
	char	*name;
	int 	nr;
	int 	natoms;	    /*nr of atoms per lipid*/
	int	mol1;	    /*id of the first lipid molecule*/
	real 	area;
} lip_t;

typedef struct {
	char	*name;
	t_block mem_at;
	int		*mol_id;
	int		nmol;
	real	lip_area;
	real	zmin;
	real	zmax;
	real	zmed;
} mem_t;

typedef struct {
	int		*mol;
	int		*block;
	int 	nr;
} rm_t;

int search_string(char *s,int ng,char ***gn)
{
	int i;

	for(i=0; (i<ng); i++)
		if (gmx_strcasecmp(s,*gn[i]) == 0)
			return i;

	gmx_fatal(FARGS,"Group %s not found in indexfile.\nMaybe you have non-default groups in your mdp file, while not using the '-n' option of grompp.\nIn that case use the '-n' option.\n",s);

	return -1;
}

int get_mol_id(int at,int nmblock,gmx_molblock_t *mblock, int *type, int *block)
{
	int mol_id=0;
	int i;

	for(i=0;i<nmblock;i++)
	{
		if(at<(mblock[i].nmol*mblock[i].natoms_mol))
		{
			mol_id+=at/mblock[i].natoms_mol;
			*type = mblock[i].type;
			*block = i;
			return mol_id;
		} else {
			at-= mblock[i].nmol*mblock[i].natoms_mol;
			mol_id+=mblock[i].nmol;
		}
	}

	gmx_fatal(FARGS,"Something is wrong in mol ids, at %d, mol_id %d",at,mol_id);

	return -1;
}

int get_block(int mol_id,int nmblock,gmx_molblock_t *mblock)
{
	int i;
	int nmol=0;

	for(i=0;i<nmblock;i++)
	{
		nmol+=mblock[i].nmol;
		if(mol_id<nmol)
			return i;
	}

	gmx_fatal(FARGS,"mol_id %d larger than total number of molecules %d.\n",mol_id,nmol);

	return -1;
}

int get_tpr_version(const char *infile)
{
	char  	buf[STRLEN];
	gmx_bool  	bDouble;
	int 	precision,fver;
        t_fileio *fio;

	fio = open_tpx(infile,"r");
	gmx_fio_checktype(fio);

	precision = sizeof(real);

	gmx_fio_do_string(fio,buf);
	if (strncmp(buf,"VERSION",7))
		gmx_fatal(FARGS,"Can not read file %s,\n"
				"             this file is from a Gromacs version which is older than 2.0\n"
				"             Make a new one with grompp or use a gro or pdb file, if possible",
				gmx_fio_getname(fio));
	gmx_fio_do_int(fio,precision);
	bDouble = (precision == sizeof(double));
	if ((precision != sizeof(float)) && !bDouble)
		gmx_fatal(FARGS,"Unknown precision in file %s: real is %d bytes "
				"instead of %d or %d",
				gmx_fio_getname(fio),precision,sizeof(float),sizeof(double));
	gmx_fio_setprecision(fio,bDouble);
	fprintf(stderr,"Reading file %s, %s (%s precision)\n",
			gmx_fio_getname(fio),buf,bDouble ? "double" : "single");

	gmx_fio_do_int(fio,fver);

	close_tpx(fio);

	return fver;
}

int get_mtype_list(t_block *at, gmx_mtop_t *mtop, t_block *tlist)
{
	int i,j,nr,mol_id;
        int type=0,block=0;
	gmx_bool bNEW;

	nr=0;
	snew(tlist->index,at->nr);
	for (i=0;i<at->nr;i++)
	{
		bNEW=TRUE;
		mol_id = get_mol_id(at->index[i],mtop->nmolblock,mtop->molblock,&type,&block);
		for(j=0;j<nr;j++)
		{
			if(tlist->index[j]==type)
				bNEW=FALSE;
		}
		if(bNEW==TRUE)
		{
			tlist->index[nr]=type;
			nr++;
		}
	}

	srenew(tlist->index,nr);
	return nr;
}

void check_types(t_block *ins_at,t_block *rest_at,gmx_mtop_t *mtop)
{
	t_block		*ins_mtype,*rest_mtype;
	int			i,j;

	snew(ins_mtype,1);
	snew(rest_mtype,1);
    ins_mtype->nr  = get_mtype_list(ins_at , mtop, ins_mtype );
    rest_mtype->nr = get_mtype_list(rest_at, mtop, rest_mtype);

    for(i=0;i<ins_mtype->nr;i++)
    {
    	for(j=0;j<rest_mtype->nr;j++)
    	{
    		if(ins_mtype->index[i]==rest_mtype->index[j])
    			gmx_fatal(FARGS,"Moleculetype %s is found both in the group to insert and the rest of the system.\n"
					"1. Your *.ndx and *.top do not match\n"
    					"2. You are inserting some molecules of type %s (for example xray-solvent), while\n"
					"the same moleculetype is also used in the rest of the system (solvent box). Because\n"
					"we need to exclude all interactions between the atoms in the group to\n"
    					"insert, the same moleculetype can not be used in both groups. Change the\n"
    					"moleculetype of the molecules %s in the inserted group. Do not forget to provide\n"
    					"an appropriate *.itp file",*(mtop->moltype[rest_mtype->index[j]].name),
    					*(mtop->moltype[rest_mtype->index[j]].name),*(mtop->moltype[rest_mtype->index[j]].name));
    	}
    }

    sfree(ins_mtype->index);
    sfree(rest_mtype->index);
    sfree(ins_mtype);
    sfree(rest_mtype);
}

void get_input(const char *membed_input, real *xy_fac, real *xy_max, real *z_fac, real *z_max,
		int *it_xy, int *it_z, real *probe_rad, int *low_up_rm, int *maxwarn, 
		int *pieces, gmx_bool *bALLOW_ASYMMETRY)
{
    warninp_t wi;
    t_inpfile *inp;
    int       ninp;

    wi = init_warning(TRUE,0);
    
    inp = read_inpfile(membed_input, &ninp, NULL, wi);
    ITYPE ("nxy", *it_xy, 1000);
    ITYPE ("nz", *it_z, 0);
    RTYPE ("xyinit", *xy_fac, 0.5);
    RTYPE ("xyend", *xy_max, 1.0);
    RTYPE ("zinit", *z_fac, 1.0);
    RTYPE ("zend", *z_max, 1.0);
    RTYPE ("rad", *probe_rad, 0.22);
    ITYPE ("ndiff", *low_up_rm, 0);
    ITYPE ("maxwarn", *maxwarn, 0);
    ITYPE ("pieces", *pieces, 1);
    EETYPE("asymmetry", *bALLOW_ASYMMETRY, yesno_names);
 
    write_inpfile(membed_input,ninp,inp,FALSE,wi);
}

int init_ins_at(t_block *ins_at,t_block *rest_at,t_state *state, pos_ins_t *pos_ins,gmx_groups_t *groups,int ins_grp_id, real xy_max)
{
	int i,gid,c=0;
	real x,xmin,xmax,y,ymin,ymax,z,zmin,zmax;

	snew(rest_at->index,state->natoms);

	xmin=xmax=state->x[ins_at->index[0]][XX];
	ymin=ymax=state->x[ins_at->index[0]][YY];
	zmin=zmax=state->x[ins_at->index[0]][ZZ];

	for(i=0;i<state->natoms;i++)
	{
		gid = groups->grpnr[egcFREEZE][i];
		if(groups->grps[egcFREEZE].nm_ind[gid]==ins_grp_id)
		{
			x=state->x[i][XX];
			if (x<xmin) 			xmin=x;
			if (x>xmax)  			xmax=x;
			y=state->x[i][YY];
			if (y<ymin)				ymin=y;
			if (y>ymax)				ymax=y;
			z=state->x[i][ZZ];
			if (z<zmin)				zmin=z;
			if (z>zmax)				zmax=z;
		} else {
			rest_at->index[c]=i;
			c++;
		}
	}

	rest_at->nr=c;
	srenew(rest_at->index,c);

	if(xy_max>1.000001)
	{
		pos_ins->xmin[XX]=xmin-((xmax-xmin)*xy_max-(xmax-xmin))/2;
		pos_ins->xmin[YY]=ymin-((ymax-ymin)*xy_max-(ymax-ymin))/2;

		pos_ins->xmax[XX]=xmax+((xmax-xmin)*xy_max-(xmax-xmin))/2;
		pos_ins->xmax[YY]=ymax+((ymax-ymin)*xy_max-(ymax-ymin))/2;
	} else {
		pos_ins->xmin[XX]=xmin;
		pos_ins->xmin[YY]=ymin;

		pos_ins->xmax[XX]=xmax;
		pos_ins->xmax[YY]=ymax;
	}

	/* 6.0 is estimated thickness of bilayer */
	if( (zmax-zmin) < 6.0 )
	{
		pos_ins->xmin[ZZ]=zmin+(zmax-zmin)/2.0-3.0;
		pos_ins->xmax[ZZ]=zmin+(zmax-zmin)/2.0+3.0;
	} else {
		pos_ins->xmin[ZZ]=zmin;
		pos_ins->xmax[ZZ]=zmax;
	}

	return c;
}

real est_prot_area(pos_ins_t *pos_ins,rvec *r,t_block *ins_at, mem_t *mem_p)
{
	real x,y,dx=0.15,dy=0.15,area=0.0;
	real add;
	int c,at;

	for(x=pos_ins->xmin[XX];x<pos_ins->xmax[XX];x+=dx)
	{
		for(y=pos_ins->xmin[YY];y<pos_ins->xmax[YY];y+=dy)
		{
			c=0;
			add=0.0;
			do
			{
				at=ins_at->index[c];
				if ( (r[at][XX]>=x) && (r[at][XX]<x+dx) &&
						(r[at][YY]>=y) && (r[at][YY]<y+dy) &&
						(r[at][ZZ]>mem_p->zmin+1.0) && (r[at][ZZ]<mem_p->zmax-1.0) )
					add=1.0;
				c++;
			} while ( (c<ins_at->nr) && (add<0.5) );
			area+=add;
		}
	}
	area=area*dx*dy;

	return area;
}

void init_lip(matrix box, gmx_mtop_t *mtop, lip_t *lip)
{
	int i;
	real mem_area;
	int mol1=0;

	mem_area = box[XX][XX]*box[YY][YY]-box[XX][YY]*box[YY][XX];
	for(i=0;i<mtop->nmolblock;i++)
	{
		if(mtop->molblock[i].type == lip->id)
		{
			lip->nr=mtop->molblock[i].nmol;
			lip->natoms=mtop->molblock[i].natoms_mol;
		}
	}
	lip->area=2.0*mem_area/(double)lip->nr;

	for (i=0;i<lip->id;i++)
		mol1+=mtop->molblock[i].nmol;
	lip->mol1=mol1;
}

int init_mem_at(mem_t *mem_p, gmx_mtop_t *mtop, rvec *r, matrix box, pos_ins_t *pos_ins)
{
	int i,j,at,mol,nmol,nmolbox,count;
	t_block *mem_a;
	real z,zmin,zmax,mem_area;
	gmx_bool bNew;
	atom_id *mol_id;
	int type=0,block=0;

	nmol=count=0;
	mem_a=&(mem_p->mem_at);
	snew(mol_id,mem_a->nr);
/*	snew(index,mem_a->nr); */
	zmin=pos_ins->xmax[ZZ];
	zmax=pos_ins->xmin[ZZ];
	for(i=0;i<mem_a->nr;i++)
	{
		at=mem_a->index[i];
		if(	(r[at][XX]>pos_ins->xmin[XX]) && (r[at][XX]<pos_ins->xmax[XX]) &&
			(r[at][YY]>pos_ins->xmin[YY]) && (r[at][YY]<pos_ins->xmax[YY]) &&
			(r[at][ZZ]>pos_ins->xmin[ZZ]) && (r[at][ZZ]<pos_ins->xmax[ZZ]) )
		{
			mol = get_mol_id(at,mtop->nmolblock,mtop->molblock,&type,&block);

			bNew=TRUE;
			for(j=0;j<nmol;j++)
				if(mol == mol_id[j])
					bNew=FALSE;

			if(bNew)
			{
				mol_id[nmol]=mol;
				nmol++;
			}

			z=r[at][ZZ];
			if(z<zmin)					zmin=z;
			if(z>zmax)					zmax=z;

/*			index[count]=at;*/
			count++;
		}
	}

	mem_p->nmol=nmol;
	srenew(mol_id,nmol);
	mem_p->mol_id=mol_id;
/*	srenew(index,count);*/
/*	mem_p->mem_at.nr=count;*/
/*	sfree(mem_p->mem_at.index);*/
/*	mem_p->mem_at.index=index;*/

	if((zmax-zmin)>(box[ZZ][ZZ]-0.5))
		gmx_fatal(FARGS,"Something is wrong with your membrane. Max and min z values are %f and %f.\n"
				"Maybe your membrane is not centered in the box, but located at the box edge in the z-direction,\n"
				"so that one membrane is distributed over two periodic box images. Another possibility is that\n"
				"your water layer is not thick enough.\n",zmax,zmin);
	mem_p->zmin=zmin;
	mem_p->zmax=zmax;
	mem_p->zmed=(zmax-zmin)/2+zmin;

	/*number of membrane molecules in protein box*/
	nmolbox = count/mtop->molblock[block].natoms_mol;
	/*mem_area = box[XX][XX]*box[YY][YY]-box[XX][YY]*box[YY][XX];
	mem_p->lip_area = 2.0*mem_area/(double)mem_p->nmol;*/
	mem_area = (pos_ins->xmax[XX]-pos_ins->xmin[XX])*(pos_ins->xmax[YY]-pos_ins->xmin[YY]);
	mem_p->lip_area = 2.0*mem_area/(double)nmolbox;

	return mem_p->mem_at.nr;
}

void init_resize(t_block *ins_at,rvec *r_ins,pos_ins_t *pos_ins,mem_t *mem_p,rvec *r, gmx_bool bALLOW_ASYMMETRY)
{
	int i,j,at,c,outsidesum,gctr=0;
    int idxsum=0;

    /*sanity check*/
    for (i=0;i<pos_ins->pieces;i++)
          idxsum+=pos_ins->nidx[i];
    if (idxsum!=ins_at->nr)
          gmx_fatal(FARGS,"Piecewise sum of inserted atoms not same as size of group selected to insert.");

    snew(pos_ins->geom_cent,pos_ins->pieces);
    for (i=0;i<pos_ins->pieces;i++)
    {
    	c=0;
    	outsidesum=0;
    	for(j=0;j<DIM;j++)
    		pos_ins->geom_cent[i][j]=0;

    	for(j=0;j<DIM;j++)
    		pos_ins->geom_cent[i][j]=0;
    	for (j=0;j<pos_ins->nidx[i];j++)
    	{
    		at=pos_ins->subindex[i][j];
    		copy_rvec(r[at],r_ins[gctr]);
    		if( (r_ins[gctr][ZZ]<mem_p->zmax) && (r_ins[gctr][ZZ]>mem_p->zmin) )
    		{
    			rvec_inc(pos_ins->geom_cent[i],r_ins[gctr]);
    			c++;
    		}
    		else
    			outsidesum++;
    		gctr++;
    	}
    	if (c>0)
    		svmul(1/(double)c,pos_ins->geom_cent[i],pos_ins->geom_cent[i]);
    	if (!bALLOW_ASYMMETRY)
    		pos_ins->geom_cent[i][ZZ]=mem_p->zmed;

    	fprintf(stderr,"Embedding piece %d with center of geometry: %f %f %f\n",i,pos_ins->geom_cent[i][XX],pos_ins->geom_cent[i][YY],pos_ins->geom_cent[i][ZZ]);
    }
    fprintf(stderr,"\n");
}

void resize(rvec *r_ins, rvec *r, pos_ins_t *pos_ins,rvec fac)
{
	int i,j,k,at,c=0;
	for (k=0;k<pos_ins->pieces;k++)
		for(i=0;i<pos_ins->nidx[k];i++)
		{
			at=pos_ins->subindex[k][i];
			for(j=0;j<DIM;j++)
				r[at][j]=pos_ins->geom_cent[k][j]+fac[j]*(r_ins[c][j]-pos_ins->geom_cent[k][j]);
			c++;
		}
}

int gen_rm_list(rm_t *rm_p,t_block *ins_at,t_block *rest_at,t_pbc *pbc, gmx_mtop_t *mtop,
		rvec *r, rvec *r_ins, mem_t *mem_p, pos_ins_t *pos_ins, real probe_rad, int low_up_rm, gmx_bool bALLOW_ASYMMETRY)
{
	int i,j,k,l,at,at2,mol_id;
        int type=0,block=0;
	int nrm,nupper,nlower;
	real r_min_rad,z_lip,min_norm;
	gmx_bool bRM;
	rvec dr,dr_tmp;
	real *dist;
	int *order;

	r_min_rad=probe_rad*probe_rad;
	snew(rm_p->mol,mtop->mols.nr);
	snew(rm_p->block,mtop->mols.nr);
	nrm=nupper=0;
	nlower=low_up_rm;
	for(i=0;i<ins_at->nr;i++)
	{
		at=ins_at->index[i];
		for(j=0;j<rest_at->nr;j++)
		{
			at2=rest_at->index[j];
			pbc_dx(pbc,r[at],r[at2],dr);

			if(norm2(dr)<r_min_rad)
			{
				mol_id = get_mol_id(at2,mtop->nmolblock,mtop->molblock,&type,&block);
				bRM=TRUE;
				for(l=0;l<nrm;l++)
					if(rm_p->mol[l]==mol_id)
						bRM=FALSE;
				if(bRM)
				{
					/*fprintf(stderr,"%d wordt toegevoegd\n",mol_id);*/
					rm_p->mol[nrm]=mol_id;
					rm_p->block[nrm]=block;
					nrm++;
					z_lip=0.0;
					for(l=0;l<mem_p->nmol;l++)
					{
						if(mol_id==mem_p->mol_id[l])
						{
							for(k=mtop->mols.index[mol_id];k<mtop->mols.index[mol_id+1];k++)
								z_lip+=r[k][ZZ];
							z_lip/=mtop->molblock[block].natoms_mol;
							if(z_lip<mem_p->zmed)
								nlower++;
							else
								nupper++;
						}
					}
				}
			}
		}
	}

	/*make sure equal number of lipids from upper and lower layer are removed */
	if( (nupper!=nlower) && (!bALLOW_ASYMMETRY) )
	{
		snew(dist,mem_p->nmol);
		snew(order,mem_p->nmol);
		for(i=0;i<mem_p->nmol;i++)
		{
			at = mtop->mols.index[mem_p->mol_id[i]];
			pbc_dx(pbc,r[at],pos_ins->geom_cent[0],dr);
			if (pos_ins->pieces>1)
			{
				/*minimum dr value*/
				min_norm=norm2(dr);
				for (k=1;k<pos_ins->pieces;k++)
				{
					pbc_dx(pbc,r[at],pos_ins->geom_cent[k],dr_tmp);
					if (norm2(dr_tmp) < min_norm)
					{
						min_norm=norm2(dr_tmp);
						copy_rvec(dr_tmp,dr);
					}
				}
			}
			dist[i]=dr[XX]*dr[XX]+dr[YY]*dr[YY];
			j=i-1;
			while (j>=0 && dist[i]<dist[order[j]])
			{
				order[j+1]=order[j];
				j--;
			}
			order[j+1]=i;
		}

		i=0;
		while(nupper!=nlower)
		{
			mol_id=mem_p->mol_id[order[i]];
			block=get_block(mol_id,mtop->nmolblock,mtop->molblock);

			bRM=TRUE;
			for(l=0;l<nrm;l++)
				if(rm_p->mol[l]==mol_id)
					bRM=FALSE;
			if(bRM)
			{
				z_lip=0;
				for(k=mtop->mols.index[mol_id];k<mtop->mols.index[mol_id+1];k++)
					z_lip+=r[k][ZZ];
				z_lip/=mtop->molblock[block].natoms_mol;
				if(nupper>nlower && z_lip<mem_p->zmed)
				{
					rm_p->mol[nrm]=mol_id;
					rm_p->block[nrm]=block;
					nrm++;
					nlower++;
				}
				else if (nupper<nlower && z_lip>mem_p->zmed)
				{
					rm_p->mol[nrm]=mol_id;
					rm_p->block[nrm]=block;
					nrm++;
					nupper++;
				}
			}
			i++;

			if(i>mem_p->nmol)
				gmx_fatal(FARGS,"Trying to remove more lipid molecules than there are in the membrane");
		}
		sfree(dist);
		sfree(order);
	}

	rm_p->nr=nrm;
	srenew(rm_p->mol,nrm);
	srenew(rm_p->block,nrm);

	return nupper+nlower;
}

void rm_group(t_inputrec *ir, gmx_groups_t *groups, gmx_mtop_t *mtop, rm_t *rm_p, t_state *state, t_block *ins_at, pos_ins_t *pos_ins)
{
	int i,j,k,n,rm,mol_id,at,block;
	rvec *x_tmp,*v_tmp;
	atom_id *list,*new_mols;
	unsigned char  *new_egrp[egcNR];
	gmx_bool bRM;

	snew(list,state->natoms);
	n=0;
	for(i=0;i<rm_p->nr;i++)
	{
		mol_id=rm_p->mol[i];
		at=mtop->mols.index[mol_id];
		block =rm_p->block[i];
		mtop->molblock[block].nmol--;
		for(j=0;j<mtop->molblock[block].natoms_mol;j++)
		{
			list[n]=at+j;
			n++;
		}

		mtop->mols.index[mol_id]=-1;
	}

	mtop->mols.nr-=rm_p->nr;
	mtop->mols.nalloc_index-=rm_p->nr;
	snew(new_mols,mtop->mols.nr);
	for(i=0;i<mtop->mols.nr+rm_p->nr;i++)
	{
		j=0;
		if(mtop->mols.index[i]!=-1)
		{
			new_mols[j]=mtop->mols.index[i];
			j++;
		}
	}
	sfree(mtop->mols.index);
	mtop->mols.index=new_mols;


	mtop->natoms-=n;
	state->natoms-=n;
	state->nalloc=state->natoms;
	snew(x_tmp,state->nalloc);
	snew(v_tmp,state->nalloc);

	for(i=0;i<egcNR;i++)
	{
		if(groups->grpnr[i]!=NULL)
		{
			groups->ngrpnr[i]=state->natoms;
			snew(new_egrp[i],state->natoms);
		}
	}

	rm=0;
	for (i=0;i<state->natoms+n;i++)
	{
		bRM=FALSE;
		for(j=0;j<n;j++)
		{
			if(i==list[j])
			{
				bRM=TRUE;
				rm++;
			}
		}

		if(!bRM)
		{
			for(j=0;j<egcNR;j++)
			{
				if(groups->grpnr[j]!=NULL)
				{
					new_egrp[j][i-rm]=groups->grpnr[j][i];
				}
			}
			copy_rvec(state->x[i],x_tmp[i-rm]);
			copy_rvec(state->v[i],v_tmp[i-rm]);
			for(j=0;j<ins_at->nr;j++)
			{
				if (i==ins_at->index[j])
					ins_at->index[j]=i-rm;
			}
			for(j=0;j<pos_ins->pieces;j++)
			{
				for(k=0;k<pos_ins->nidx[j];k++)
				{
					if (i==pos_ins->subindex[j][k])
						pos_ins->subindex[j][k]=i-rm;
				}
			}
		}
	}
	sfree(state->x);
	state->x=x_tmp;
	sfree(state->v);
	state->v=v_tmp;

	for(i=0;i<egcNR;i++)
	{
		if(groups->grpnr[i]!=NULL)
		{
			sfree(groups->grpnr[i]);
			groups->grpnr[i]=new_egrp[i];
		}
	}
}

int rm_bonded(t_block *ins_at, gmx_mtop_t *mtop)
{
	int i,j,m;
	int type,natom,nmol,at,atom1=0,rm_at=0;
	gmx_bool *bRM,bINS;
	/*this routine lives dangerously by assuming that all molecules of a given type are in order in the structure*/
	/*this routine does not live as dangerously as it seems. There is namely a check in mdrunner_membed to make
         *sure that g_membed exits with a warning when there are molecules of the same type not in the 
	 *ins_at index group. MGWolf 050710 */


	snew(bRM,mtop->nmoltype);
	for (i=0;i<mtop->nmoltype;i++)
	{
		bRM[i]=TRUE;
	}

	for (i=0;i<mtop->nmolblock;i++) 
	{
	    /*loop over molecule blocks*/
		type        =mtop->molblock[i].type;
		natom	    =mtop->molblock[i].natoms_mol;
		nmol		=mtop->molblock[i].nmol;

		for(j=0;j<natom*nmol && bRM[type]==TRUE;j++) 
		{
		    /*loop over atoms in the block*/
			at=j+atom1; /*atom index = block index + offset*/
			bINS=FALSE;

			for (m=0;(m<ins_at->nr) && (bINS==FALSE);m++)
			{
			    /*loop over atoms in insertion index group to determine if we're inserting one*/
				if(at==ins_at->index[m])
				{
					bINS=TRUE;
				}
			}
			bRM[type]=bINS;
		}
		atom1+=natom*nmol; /*update offset*/
		if(bRM[type])
		{
			rm_at+=natom*nmol; /*increment bonded removal counter by # atoms in block*/
		}
	}

	for(i=0;i<mtop->nmoltype;i++)
	{
		if(bRM[i])
		{
			for(j=0;j<F_LJ;j++)
			{
				mtop->moltype[i].ilist[j].nr=0;
			}
			for(j=F_POSRES;j<=F_VSITEN;j++)
			{
				mtop->moltype[i].ilist[j].nr=0;
			}
		}
	}
	sfree(bRM);

	return rm_at;
}

void top_update(const char *topfile, char *ins, rm_t *rm_p, gmx_mtop_t *mtop)
{
#define TEMP_FILENM "temp.top"
	int	bMolecules=0;
	FILE	*fpin,*fpout;
	char	buf[STRLEN],buf2[STRLEN],*temp;
	int		i,*nmol_rm,nmol,line;

	fpin  = ffopen(topfile,"r");
	fpout = ffopen(TEMP_FILENM,"w");

	snew(nmol_rm,mtop->nmoltype);
	for(i=0;i<rm_p->nr;i++)
		nmol_rm[rm_p->block[i]]++;

	line=0;
	while(fgets(buf,STRLEN,fpin))
	{
		line++;
		if(buf[0]!=';')
		{
			strcpy(buf2,buf);
			if ((temp=strchr(buf2,'\n')) != NULL)
				temp[0]='\0';
			ltrim(buf2);

			if (buf2[0]=='[')
			{
				buf2[0]=' ';
				if ((temp=strchr(buf2,'\n')) != NULL)
					temp[0]='\0';
				rtrim(buf2);
				if (buf2[strlen(buf2)-1]==']')
				{
					buf2[strlen(buf2)-1]='\0';
					ltrim(buf2);
					rtrim(buf2);
					if (gmx_strcasecmp(buf2,"molecules")==0)
						bMolecules=1;
				}
				fprintf(fpout,"%s",buf);
			} else if (bMolecules==1)
			{
				for(i=0;i<mtop->nmolblock;i++)
				{
					nmol=mtop->molblock[i].nmol;
					sprintf(buf,"%-15s %5d\n",*(mtop->moltype[mtop->molblock[i].type].name),nmol);
					fprintf(fpout,"%s",buf);
				}
				bMolecules=2;
			} else if (bMolecules==2)
			{
				/* print nothing */
			} else 
			{
				fprintf(fpout,"%s",buf);
			}
		} else 
		{
			fprintf(fpout,"%s",buf);
		}
	}

	fclose(fpout);
	/* use ffopen to generate backup of topinout */
	fpout=ffopen(topfile,"w");
	fclose(fpout);
	rename(TEMP_FILENM,topfile);
#undef TEMP_FILENM
}

void rescale_membed(int step_rel, gmx_membed_t *membed, rvec *x)
{
	/* Set new positions for the group to embed */
	if(step_rel<=membed->it_xy)
	{
		membed->fac[0]+=membed->xy_step;
		membed->fac[1]+=membed->xy_step;
	} else if (step_rel<=(membed->it_xy+membed->it_z))
	{
		membed->fac[2]+=membed->z_step;
	}
	resize(membed->r_ins,x,membed->pos_ins,membed->fac);
}

void init_membed(FILE *fplog, gmx_membed_t *membed, int nfile, const t_filenm fnm[], gmx_mtop_t *mtop, t_inputrec *inputrec, t_state *state, t_commrec *cr,real *cpt)
{
        char                    *ins;
        int                     i,rm_bonded_at,fr_id,fr_i=0,tmp_id,warn=0;
        int                     ng,j,max_lip_rm,ins_grp_id,ins_nat,mem_nat,ntype,lip_rm,tpr_version;
        real                    prot_area;
        rvec                    *r_ins=NULL;
        t_block                 *ins_at,*rest_at;
        pos_ins_t               *pos_ins;
        mem_t                   *mem_p;
        rm_t                    *rm_p;
        gmx_groups_t            *groups;
        gmx_bool                    bExcl=FALSE;
        t_atoms                 atoms;
        t_pbc                   *pbc;
        char                    **piecename=NULL;
    
        /* input variables */
	const char *membed_input;
        real xy_fac = 0.5;
        real xy_max = 1.0;
        real z_fac = 1.0;
        real z_max = 1.0;
        int it_xy = 1000;
        int it_z = 0;
        real probe_rad = 0.22;
        int low_up_rm = 0;
        int maxwarn=0;
        int pieces=1;
        gmx_bool bALLOW_ASYMMETRY=FALSE;

	snew(ins_at,1);
	snew(pos_ins,1);

	if(MASTER(cr))
	{
                /* get input data out membed file */
		membed_input = opt2fn("-membed",nfile,fnm);
		get_input(membed_input,&xy_fac,&xy_max,&z_fac,&z_max,&it_xy,&it_z,&probe_rad,&low_up_rm,&maxwarn,&pieces,&bALLOW_ASYMMETRY);

		tpr_version = get_tpr_version(ftp2fn(efTPX,nfile,fnm));
		if (tpr_version<58)
			gmx_fatal(FARGS,"Version of *.tpr file to old (%d). Rerun grompp with gromacs VERSION 4.0.3 or newer.\n",tpr_version);

		if( !EI_DYNAMICS(inputrec->eI) )
			gmx_input("Change integrator to a dynamics integrator in mdp file (e.g. md or sd).");

		if(PAR(cr))
			gmx_input("Sorry, parallel g_membed is not yet fully functional.");
     
#ifdef GMX_OPENMM
			gmx_input("Sorry, g_membed does not work with openmm.");
#endif

		if(*cpt>=0)
		{
			fprintf(stderr,"\nSetting -cpt to -1, because embedding cannot be restarted from cpt-files.\n");
 			*cpt=-1;
		}
		groups=&(mtop->groups);

		atoms=gmx_mtop_global_atoms(mtop);
		snew(mem_p,1);
		fprintf(stderr,"\nSelect a group to embed in the membrane:\n");
		get_index(&atoms,opt2fn_null("-mn",nfile,fnm),1,&(ins_at->nr),&(ins_at->index),&ins);
		ins_grp_id = search_string(ins,groups->ngrpname,(groups->grpname));
		fprintf(stderr,"\nSelect a group to embed %s into (e.g. the membrane):\n",ins);
		get_index(&atoms,opt2fn_null("-mn",nfile,fnm),1,&(mem_p->mem_at.nr),&(mem_p->mem_at.index),&(mem_p->name));

		pos_ins->pieces=pieces;
		snew(pos_ins->nidx,pieces);
		snew(pos_ins->subindex,pieces);
		snew(piecename,pieces);	
		if (pieces>1)
		{
			fprintf(stderr,"\nSelect pieces to embed:\n");
			get_index(&atoms,opt2fn_null("-mn",nfile,fnm),pieces,pos_ins->nidx,pos_ins->subindex,piecename);
		}
		else
		{	
			/*use whole embedded group*/
			snew(pos_ins->nidx,1);
			snew(pos_ins->subindex,1);
			pos_ins->nidx[0]=ins_at->nr;
			pos_ins->subindex[0]=ins_at->index;
		}

		if(probe_rad<0.2199999)
		{
			warn++;
			fprintf(stderr,"\nWarning %d:\nA probe radius (-rad) smaller than 0.2 can result in overlap between waters "
					"and the group to embed, which will result in Lincs errors etc.\nIf you are sure, you can increase maxwarn.\n\n",warn);
		}

		if(xy_fac<0.09999999)
		{
			warn++;
			fprintf(stderr,"\nWarning %d:\nThe initial size of %s is probably too smal.\n"
					"If you are sure, you can increase maxwarn.\n\n",warn,ins);
		}

		if(it_xy<1000)
		{
			warn++;
			fprintf(stderr,"\nWarning %d;\nThe number of steps used to grow the xy-coordinates of %s (%d) is probably too small.\n"
					"Increase -nxy or, if you are sure, you can increase maxwarn.\n\n",warn,ins,it_xy);
		}

		if( (it_z<100) && ( z_fac<0.99999999 || z_fac>1.0000001) )
                {
                        warn++;
                        fprintf(stderr,"\nWarning %d;\nThe number of steps used to grow the z-coordinate of %s (%d) is probably too small.\n"
                                       "Increase -nz or, if you are sure, you can increase maxwarn.\n\n",warn,ins,it_z);
                }

		if(it_xy+it_z>inputrec->nsteps)
		{
			warn++;
			fprintf(stderr,"\nWarning %d:\nThe number of growth steps (-nxy + -nz) is larger than the number of steps in the tpr.\n"
					"If you are sure, you can increase maxwarn.\n\n",warn);
		}

		fr_id=-1;
		if( inputrec->opts.ngfrz==1)
			gmx_fatal(FARGS,"You did not specify \"%s\" as a freezegroup.",ins);
		for(i=0;i<inputrec->opts.ngfrz;i++)
		{
			tmp_id = mtop->groups.grps[egcFREEZE].nm_ind[i];
			if(ins_grp_id==tmp_id)
			{
				fr_id=tmp_id;
				fr_i=i;
			}
		}
		if (fr_id == -1 )
			gmx_fatal(FARGS,"\"%s\" not as freezegroup defined in the mdp-file.",ins);

		for(i=0;i<DIM;i++)
			if( inputrec->opts.nFreeze[fr_i][i] != 1)
				gmx_fatal(FARGS,"freeze dimensions for %s are not Y Y Y\n",ins);

		ng = groups->grps[egcENER].nr;
		if (ng == 1)
			gmx_input("No energy groups defined. This is necessary for energy exclusion in the freeze group");

		for(i=0;i<ng;i++)
		{
			for(j=0;j<ng;j++)
			{
				if (inputrec->opts.egp_flags[ng*i+j] == EGP_EXCL)
				{
					bExcl = TRUE;
					if ( (groups->grps[egcENER].nm_ind[i] != ins_grp_id) || (groups->grps[egcENER].nm_ind[j] != ins_grp_id) )
						gmx_fatal(FARGS,"Energy exclusions \"%s\" and  \"%s\" do not match the group to embed \"%s\"",
								*groups->grpname[groups->grps[egcENER].nm_ind[i]],
								*groups->grpname[groups->grps[egcENER].nm_ind[j]],ins);
				}
			}
		}
		if (!bExcl)
			gmx_input("No energy exclusion groups defined. This is necessary for energy exclusion in the freeze group");

		/* Guess the area the protein will occupy in the membrane plane	 Calculate area per lipid*/
		snew(rest_at,1);
		ins_nat = init_ins_at(ins_at,rest_at,state,pos_ins,groups,ins_grp_id,xy_max);
		/* Check moleculetypes in insertion group */
		check_types(ins_at,rest_at,mtop);

		mem_nat = init_mem_at(mem_p,mtop,state->x,state->box,pos_ins);

		prot_area = est_prot_area(pos_ins,state->x,ins_at,mem_p);
		if ( (prot_area>7.5) && ( (state->box[XX][XX]*state->box[YY][YY]-state->box[XX][YY]*state->box[YY][XX])<50) )
		{
			warn++;
			fprintf(stderr,"\nWarning %d:\nThe xy-area is very small compared to the area of the protein.\n"
					"This might cause pressure problems during the growth phase. Just try with\n"
					"current setup (-maxwarn + 1), but if pressure problems occur, lower the\n"
					"compressibility in the mdp-file or use no pressure coupling at all.\n\n",warn);
		}
		if(warn>maxwarn)
					gmx_fatal(FARGS,"Too many warnings.\n");

		printf("The estimated area of the protein in the membrane is %.3f nm^2\n",prot_area);
		printf("\nThere are %d lipids in the membrane part that overlaps the protein.\nThe area per lipid is %.4f nm^2.\n",mem_p->nmol,mem_p->lip_area);

		/* Maximum number of lipids to be removed*/
		max_lip_rm=(int)(2*prot_area/mem_p->lip_area);
		printf("Maximum number of lipids that will be removed is %d.\n",max_lip_rm);

		printf("\nWill resize the protein by a factor of %.3f in the xy plane and %.3f in the z direction.\n"
				"This resizing will be done with respect to the geometrical center of all protein atoms\n"
				"that span the membrane region, i.e. z between %.3f and %.3f\n\n",xy_fac,z_fac,mem_p->zmin,mem_p->zmax);

		/* resize the protein by xy and by z if necessary*/
		snew(r_ins,ins_at->nr);
		init_resize(ins_at,r_ins,pos_ins,mem_p,state->x,bALLOW_ASYMMETRY);
		membed->fac[0]=membed->fac[1]=xy_fac;
		membed->fac[2]=z_fac;

		membed->xy_step =(xy_max-xy_fac)/(double)(it_xy);
		membed->z_step  =(z_max-z_fac)/(double)(it_z-1);

		resize(r_ins,state->x,pos_ins,membed->fac);

		/* remove overlapping lipids and water from the membrane box*/
		/*mark molecules to be removed*/
		snew(pbc,1);
		set_pbc(pbc,inputrec->ePBC,state->box);

		snew(rm_p,1);
		lip_rm = gen_rm_list(rm_p,ins_at,rest_at,pbc,mtop,state->x, r_ins, mem_p,pos_ins,probe_rad,low_up_rm,bALLOW_ASYMMETRY);
	        lip_rm -= low_up_rm;

		if(fplog)
			for(i=0;i<rm_p->nr;i++)
				fprintf(fplog,"rm mol %d\n",rm_p->mol[i]);

		for(i=0;i<mtop->nmolblock;i++)
		{
			ntype=0;
			for(j=0;j<rm_p->nr;j++)
				if(rm_p->block[j]==i)
					ntype++;
			printf("Will remove %d %s molecules\n",ntype,*(mtop->moltype[mtop->molblock[i].type].name));
		}

		if(lip_rm>max_lip_rm)
		{
			warn++;
			fprintf(stderr,"\nWarning %d:\nTrying to remove a larger lipid area than the estimated protein area\n"
					"Try making the -xyinit resize factor smaller.\n\n",warn);
		}

		/*remove all lipids and waters overlapping and update all important structures*/
		rm_group(inputrec,groups,mtop,rm_p,state,ins_at,pos_ins);

		rm_bonded_at = rm_bonded(ins_at,mtop);
		if (rm_bonded_at != ins_at->nr)
		{
			fprintf(stderr,"Warning: The number of atoms for which the bonded interactions are removed is %d, "
					"while %d atoms are embedded. Make sure that the atoms to be embedded are not in the same"
					"molecule type as atoms that are not to be embedded.\n",rm_bonded_at,ins_at->nr);
		}

		if(warn>maxwarn)
			gmx_fatal(FARGS,"Too many warnings.\nIf you are sure these warnings are harmless, you can increase -maxwarn");

		if (ftp2bSet(efTOP,nfile,fnm))
			top_update(opt2fn("-p",nfile,fnm),ins,rm_p,mtop);

		sfree(pbc);
		sfree(rest_at);
    		if (pieces>1) { 	sfree(piecename); }

                membed->it_xy=it_xy;
                membed->it_z=it_z;
                membed->pos_ins=pos_ins;
                membed->r_ins=r_ins;
	}
}