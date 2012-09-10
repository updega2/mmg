#include "mmg3d.h"

extern Info  info;


/* read mesh data */
int loadMesh(pMesh mesh) {
  pTetra       pt;
  pTria        pt1;
	pEdge        pa;
  pPoint       ppt;
  float        fp1,fp2,fp3;
  int          i,k,inm,ia,nr,aux;
  char        *ptr,*name,data[128];

  name = mesh->namein;
  strcpy(data,name);
  ptr = strstr(data,".mesh");
  if ( !ptr ) {
    strcat(data,".meshb");
    if( !(inm = GmfOpenMesh(data,GmfRead,&mesh->ver,&mesh->dim)) ) {
      ptr = strstr(data,".mesh");
      *ptr = '\0';
      strcat(data,".mesh");
      if( !(inm = GmfOpenMesh(data,GmfRead,&mesh->ver,&mesh->dim)) ) {
        fprintf(stderr,"  ** %s  NOT FOUND.\n",data);
        return(0);
      }
    }
  }
  else if( !(inm = GmfOpenMesh(data,GmfRead,&mesh->ver,&mesh->dim)) ) {
    fprintf(stderr,"  ** %s  NOT FOUND.\n",data);
    return(0);
  }
  fprintf(stdout,"  %%%% %s OPENED\n",data);

  mesh->npi = GmfStatKwd(inm,GmfVertices);
  mesh->nt  = GmfStatKwd(inm,GmfTriangles);
  mesh->nei = GmfStatKwd(inm,GmfTetrahedra);
  mesh->nai = GmfStatKwd(inm,GmfEdges);
  if ( !mesh->npi || !mesh->nei ) {
    fprintf(stdout,"  ** MISSING DATA. Exit program.\n");
    return(0);
  }
  /* memory allocation */
  mesh->np = mesh->npi;
  mesh->ne = mesh->nei;
	mesh->na = mesh->nai;
  if ( !zaldy(mesh) )  return(0);

  /* read mesh vertices */
  GmfGotoKwd(inm,GmfVertices);
  for (k=1; k<=mesh->np; k++) {
    ppt = &mesh->point[k];
    if (mesh->ver == GmfFloat) {
      GmfGetLin(inm,GmfVertices,&fp1,&fp2,&fp3,&ppt->ref);  
      ppt->c[0] = fp1;  
      ppt->c[1] = fp2;  
      ppt->c[2] = fp3;  
    } else {
      GmfGetLin(inm,GmfVertices,&ppt->c[0],&ppt->c[1],&ppt->c[2],&ppt->ref);
    }             
    ppt->tag  = MG_NUL;
  }
  /* read mesh triangles */
  if ( mesh->nt ) {
    GmfGotoKwd(inm,GmfTriangles);
    for (k=1; k<=mesh->nt; k++) {
      pt1 = &mesh->tria[k];
      GmfGetLin(inm,GmfTriangles,&pt1->v[0],&pt1->v[1],&pt1->v[2],&pt1->ref);
    }
  }
  /* read mesh edges */
	nr = 0;
  if ( mesh->na ) {
    GmfGotoKwd(inm,GmfEdges);
    for (k=1; k<=mesh->na; k++) {
			pa = &mesh->edge[k];
      GmfGetLin(inm,GmfEdges,&pa->a,&pa->b,&pa->ref);
			pa->tag |= MG_REF;
    }
    /* get ridges */
    nr = GmfStatKwd(inm,GmfRidges);
    if ( nr ) {
      GmfGotoKwd(inm,GmfRidges);
      for (k=1; k<=nr; k++) {
        GmfGetLin(inm,GmfRidges,&ia);
				assert(ia <= mesh->na);
				pa = &mesh->edge[ia];
				pa->tag |= MG_GEO;
      }
    }
    /* get required edges */
    nr = GmfStatKwd(inm,GmfRequiredEdges);
    if ( nr ) {
      GmfGotoKwd(inm,GmfRequiredEdges);
      for (k=1; k<=nr; k++) {
        GmfGetLin(inm,GmfRequiredEdges,&ia);
				assert(ia <= mesh->na);
				pa = &mesh->edge[ia];
				pa->tag |= MG_REQ;
      }
    }
  }
  /* read mesh tetrahedra */
  GmfGotoKwd(inm,GmfTetrahedra);
  for (k=1; k<=mesh->ne; k++) {
    pt = &mesh->tetra[k];
    GmfGetLin(inm,GmfTetrahedra,&pt->v[0],&pt->v[1],&pt->v[2],&pt->v[3],&pt->ref);
    for (i=0; i<4; i++) {    
      ppt = &mesh->point[pt->v[i]];
      ppt->tag &= ~MG_NUL;
    }
    /* Possibly switch 2 vertices number so that each tet is positively oriented */
    if ( orvol(mesh->point,pt->v) < 0.0 ) {
      aux = pt->v[2];
      pt->v[2] = pt->v[3];
      pt->v[3] = aux;
    }
  }

  /* stats */
  if ( abs(info.imprim) > 4 ) {
    fprintf(stdout,"     NUMBER OF VERTICES     %8d / %8d\n",mesh->np,mesh->npmax);
    if ( mesh->na )
      fprintf(stdout,"     NUMBER OF EDGES/RIDGES %8d / %8d\n",mesh->na,nr);
    if ( mesh->nt )
      fprintf(stdout,"     NUMBER OF TRIANGLES    %8d\n",mesh->nt);
    fprintf(stdout,"     NUMBER OF ELEMENTS     %8d / %8d\n",mesh->ne,mesh->nemax);
  }
  GmfCloseMesh(inm);
  return(1);
}


int saveMesh(pMesh mesh) {
  pPoint       ppt;
  pTetra       pt;
  pTria        ptt;
  xPoint      *pxp;
	hgeom       *ph;
	double       dd;
  int          k,na,nc,np,ne,nn,nr,nre,nt,outm;
  char         data[128];

  mesh->ver = GmfDouble;
  strcpy(data,mesh->nameout);
  if ( !(outm = GmfOpenMesh(data,GmfWrite,mesh->ver,mesh->dim)) ) {
    fprintf(stderr,"  ** UNABLE TO OPEN %s\n",data);
    return(0);
  }
  fprintf(stdout,"  %%%% %s OPENED\n",data);

  /* vertices */
  np = nc = na = nr = nre = 0;
  for (k=1; k<=mesh->np; k++) {
    ppt = &mesh->point[k];
    if ( MG_VOK(ppt) ) {
	    ppt->tmp = ++np; 
      if ( ppt->tag & MG_CRN )  nc++;
      if ( ppt->tag & MG_REQ )  nre++;
    }
  }
  GmfSetKwd(outm,GmfVertices,np);
  for (k=1; k<=mesh->np; k++) {
    ppt = &mesh->point[k];
    if ( MG_VOK(ppt) )
      GmfSetLin(outm,GmfVertices,ppt->c[0],ppt->c[1],ppt->c[2],ppt->ref);
  }

  /* boundary mesh */
  mesh->nt = 0;
	if ( mesh->tria)  free(mesh->tria);
  if ( bdryTria(mesh) ) {
    GmfSetKwd(outm,GmfTriangles,mesh->nt);
    for (k=1; k<=mesh->nt; k++) {
      ptt = &mesh->tria[k];
      GmfSetLin(outm,GmfTriangles,mesh->point[ptt->v[0]].tmp,mesh->point[ptt->v[1]].tmp,\
                                  mesh->point[ptt->v[2]].tmp,ptt->ref);
    }
    free(mesh->adjt);
    free(mesh->tria);

    /* edges + ridges */
		na = nr = 0;
		for (k=0; k<=mesh->htab.max; k++) {
			ph = &mesh->htab.geom[k];
			if ( !ph->a )  continue;
			na++;
			if ( ph->tag & MG_GEO )  nr++;
		}
    if ( na ) {
      GmfSetKwd(outm,GmfEdges,na);
		  for (k=0; k<=mesh->htab.max; k++) {
			  ph = &mesh->htab.geom[k];
			  if ( !ph->a )  continue;
				GmfSetLin(outm,GmfEdges,mesh->point[ph->a].tmp,mesh->point[ph->b].tmp,ph->ref);
		  }
		  if ( nr ) {
        GmfSetKwd(outm,GmfRidges,nr);
				na = 0;
		    for (k=0; k<=mesh->htab.max; k++) {
			    ph = &mesh->htab.geom[k];
			    if ( !ph->a )  continue;
					na++;
				  if ( ph->tag & MG_GEO )  GmfSetLin(outm,GmfRidges,na);
				}
		  }
    }
  }

  /* corners+required */
  if ( nc ) {
    GmfSetKwd(outm,GmfCorners,nc);
    for (k=1; k<=mesh->np; k++) {
      ppt = &mesh->point[k];
      if ( MG_VOK(ppt) && ppt->tag & MG_CRN )
        GmfSetLin(outm,GmfCorners,ppt->tmp);
    }
  }
  if ( nre ) {
    GmfSetKwd(outm,GmfRequiredVertices,nre);
    for (k=1; k<=mesh->np; k++) {
      ppt = &mesh->point[k];
      if ( MG_VOK(ppt) && ppt->tag & MG_REQ )
        GmfSetLin(outm,GmfRequiredVertices,ppt->tmp);
    }
  }

  /* tetrahedra */
  ne = 0;
  for (k=1; k<=mesh->ne; k++) {
    pt = &mesh->tetra[k];
    if ( MG_EOK(pt) ) ne++;
  }
  GmfSetKwd(outm,GmfTetrahedra,ne); 
  for (k=1; k<=mesh->ne; k++) {
    pt = &mesh->tetra[k];
    if ( MG_EOK(pt) )
      GmfSetLin(outm,GmfTetrahedra,mesh->point[pt->v[0]].tmp,mesh->point[pt->v[1]].tmp, \
                     mesh->point[pt->v[2]].tmp,mesh->point[pt->v[3]].tmp,pt->ref);
  }

  /* write normals */
  nn = nt = 0;
  GmfSetKwd(outm,GmfNormals,mesh->xp);
  for (k=1; k<=mesh->np; k++) {
    ppt = &mesh->point[k];
		if ( MG_SIN(ppt->tag) )  continue;
    else if ( MG_VOK(ppt) && (ppt->tag & MG_BDY) && !(ppt->tag & MG_GEO) ) {
      pxp = &mesh->xpoint[ppt->xp];
      GmfSetLin(outm,GmfNormals,pxp->n1[0],pxp->n1[1],pxp->n1[2]);
			dd = pxp->n1[0]*pxp->n1[0] + pxp->n1[1]*pxp->n1[1] + pxp->n1[2]*pxp->n1[2];
			assert(dd > 0.9);
			nn++;
    }
    if ( MG_EDG(ppt->tag) ) nt++;
  }
  GmfSetKwd(outm,GmfNormalAtVertices,nn);
  nn = 0;
  for (k=1; k<=mesh->np; k++) {
    ppt = &mesh->point[k];
		if ( MG_SIN(ppt->tag) )  continue;
    else if ( MG_VOK(ppt) && (ppt->tag & MG_BDY) && !(ppt->tag & MG_GEO) )
      GmfSetLin(outm,GmfNormalAtVertices,ppt->tmp,++nn);
  }
  if ( nt ) {
    GmfSetKwd(outm,GmfTangents,nt);
    for (k=1; k<=mesh->np; k++) {
      ppt = &mesh->point[k];
		  if ( MG_SIN(ppt->tag) )  continue;
      else if ( MG_VOK(ppt) && MG_EDG(ppt->tag) ) {
        pxp = &mesh->xpoint[ppt->xp];
				GmfSetLin(outm,GmfTangents,pxp->t[0],pxp->t[1],pxp->t[2]);
      }
    }
    GmfSetKwd(outm,GmfTangentAtVertices,nt);
    nt = 0;
    for (k=1; k<=mesh->np; k++) {
      ppt = &mesh->point[k];
	  	if ( MG_SIN(ppt->tag) )  continue;
      else if ( MG_VOK(ppt) && MG_EDG(ppt->tag) )
        GmfSetLin(outm,GmfTangentAtVertices,ppt->tmp,++nt);
    }
  }

  if ( info.imprim ) {
    fprintf(stdout,"     NUMBER OF VERTICES   %8d   CORNERS %8d\n",np,nc+nre);
    if ( na )
      fprintf(stdout,"     NUMBER OF EDGES      %8d   RIDGES  %8d\n",na,nr);
    if ( mesh->nt )
      fprintf(stdout,"     NUMBER OF TRIANGLES  %8d\n",mesh->nt);
    fprintf(stdout,"     NUMBER OF ELEMENTS   %8d\n",ne);
  }

  GmfCloseMesh(outm);
  return(1);
}


/* load metric field */
int loadMet(pSol met) {
  double       dbuf[GmfMaxTyp];
  float        fbuf[GmfMaxTyp];
  int          i,k,iad,inm,typtab[GmfMaxTyp];
  char        *ptr,data[128];

  if ( !met->namein )  return(0);
  strcpy(data,met->namein);
  ptr = strstr(data,".mesh");
  if ( ptr )  *ptr = '\0';
  strcat(data,".solb");
  if (!(inm = GmfOpenMesh(data,GmfRead,&met->ver,&met->dim)) ) {
    ptr  = strstr(data,".sol");
    *ptr = '\0';
    strcat(data,".sol");
    if (!(inm = GmfOpenMesh(data,GmfRead,&met->ver,&met->dim)) ) {
      if ( info.imprim < 0 ) 
        fprintf(stderr,"  ** %s  NOT FOUND. USE DEFAULT METRIC.\n",data);
      return(-1);
    }
  }
  fprintf(stdout,"  %%%% %s OPENED\n",data);

  /* read solution or metric */
  met->np = GmfStatKwd(inm,GmfSolAtVertices,&met->type,&met->size,typtab);
  if ( !met->np ) {
    fprintf(stdout,"  ** MISSING DATA.\n");
    return(1);
  }
  if ( (met->type != 1) || (typtab[0] != 1 && typtab[0] != 3) ) {
    fprintf(stdout,"  ** DATA IGNORED %d  %d\n",met->type,typtab[0]);
    met->np = met->npmax = 0;
    return(-1);
  }

  /* mem alloc */
  met->m = (double*)calloc(met->size*met->npmax+1,sizeof(double));
  assert(met->m);

  /* read mesh solutions */
  GmfGotoKwd(inm,GmfSolAtVertices);
  for (k=1; k<=met->np; k++) {
    iad = met->size*(k-1)+1;
    if ( met->ver == GmfFloat ) {
      GmfGetLin(inm,GmfSolAtVertices,fbuf);
      for (i=0; i<met->size; i++)  met->m[iad+i] = fbuf[i];
    }
    else {
      GmfGetLin(inm,GmfSolAtVertices,dbuf);
      for (i=0; i<met->size; i++)  met->m[iad+i] = dbuf[i];
    }
  }

  GmfCloseMesh(inm);
  return(1);  
}


int saveSize(pMesh mesh) {
  pPoint       ppt;
  int          k,inm,np,type,typtab[GmfMaxTyp],ver;
  char        *ptr,data[128];

  strcpy(data,mesh->nameout);
  ptr = strstr(data,".mesh");
  if ( ptr )  *ptr = '\0';
  strcat(data,".sol");
  ver = GmfDouble;
  if (!(inm = GmfOpenMesh(data,GmfWrite,ver,mesh->dim)) ) {
    fprintf(stderr,"  ** UNABLE TO OPEN %s\n",data);
    return(0);
  }
  fprintf(stdout,"  %%%% %s OPENED\n",data);

  np = 0;
  for (k=1; k<=mesh->np; k++) {
    ppt = &mesh->point[k];
    if ( MG_VOK(ppt) )  np++;
  }

  /* write sol */
  type   = 1;
  typtab[0] = GmfSca;
  GmfSetKwd(inm,GmfSolAtVertices,np,type,typtab);
  for (k=1; k<=mesh->np; k++) {
    ppt = &mesh->point[k];
    if ( MG_VOK(ppt) ) {
      GmfSetLin(inm,GmfSolAtVertices,&ppt->h);
    }
  }
  GmfCloseMesh(inm);
  return(1);
}
