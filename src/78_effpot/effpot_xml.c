/*
 * Copyright (C) 2015-2015 ABINIT group (AM)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* ===============================================================
 * Set of C functions interfacing the LibXML library.
 * ===============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <config.h>

#if defined HAVE_LIBXML

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>

//define type for dynamical array 
typedef struct {
  double *array;
  size_t used;
  size_t size;
} Array;

void initArray(Array *a, size_t initialSize) {
  a->array = (double *)malloc(initialSize * sizeof(double));
  a->used = 0;
  a->size = initialSize;
}

void insertArray(Array *a, double element) {
  if (a->used == a->size) {
    a->size *= 2;
    a->array = (double *)realloc(a->array, a->size * sizeof(double));
  }
  a->array[a->used++] = element;
}

void freeArray(Array *a) {
  free(a->array);
  a->array = NULL;
  a->used = a->size = 0;
}


void effpot_xml_checkXML(char *filename,char *name_xml){  
  xmlDocPtr doc;
  xmlNodePtr cur;
  xmlNodeSetPtr nodeset;

  doc = xmlParseFile(filename);
  if (doc == NULL) printf(" error: could not parse file file.xml\n");

  cur = xmlDocGetRootElement(doc);
  printf(" Root node xml : %s\n",cur -> name);
  if (cur == NULL) {
    fprintf(stderr," The document is empty \n");
    xmlFreeDoc(doc);
    return;
  }

  if (xmlStrcmp(cur->name, (const  xmlChar *) name_xml)) {
    fprintf(stderr," Document of the wrong type, root node != %s\n",name_xml);
    xmlFreeDoc(doc);
     return;
  }
  xmlFreeDoc(doc);
}

void effpot_xml_getDims(char *filename,int *natom,int *ntypat, int *nqpt, int *nrpt){
  xmlDocPtr doc;
  int i,iatom,irpt,iqpt,itypat,present;
  xmlNodePtr cur,cur2;
  xmlChar *key,*uri;
  Array typat;

  initArray(&typat, 1);

  present  = 0;
  iatom    = 0;
  irpt     = 0;
  iqpt    = 0;
  itypat   = 0;
  typat.array[0] = 0;

  doc = xmlParseFile(filename);
  if (doc == NULL) printf(" error: could not parse file file.xml\n");

  cur = xmlDocGetRootElement(doc);
  if (cur == NULL) {
    fprintf(stderr," The document is empty \n");
    xmlFreeDoc(doc);
    return;
  }
  cur = cur->xmlChildrenNode;
  while (cur != NULL) {
    if ((!xmlStrcmp(cur->name, (const  xmlChar *) "atom"))) {
      iatom++;
      uri = xmlGetProp(cur, (const  xmlChar *) "mass");
      present = 0;
      for(i=0;i<=sizeof(typat.array);i++){
        if (typat.array[i] == strtod(uri,NULL)){
          present = 1;
          break;
        }
      }
      if(present==0){
        itypat++;
        insertArray(&typat,strtod(uri,NULL)); 
      }
      xmlFree(uri);
      
   } 
    if ((!xmlStrcmp(cur->name, (const  xmlChar *) "total_force_constant"))) {irpt++;}
    if ((!xmlStrcmp(cur->name, (const  xmlChar *) "phonon"))) {
      cur2 = cur->xmlChildrenNode;
      while (cur2 != NULL) {
        if ((!xmlStrcmp(cur2->name, (const  xmlChar *) "qpoint"))) {iqpt++;}
        cur2 = cur2->next;
      }
    }
    cur = cur->next;
  }
  freeArray(&typat);

  *natom  = iatom;
  *nrpt   = irpt;
  *nqpt   = iqpt;
  *ntypat = itypat;
}

void effpot_xml_read(char *filename,int *natom,int *ntypat,int *nrpt,int *nqpt,
                     double amu[*ntypat],double atmfrc[*nrpt][*natom][3][*natom][3][2],
                     int cell[*nrpt][3],double dynmat[*nqpt][*natom][3][*natom][3][2],
                     double elastic_constants[6][6],
                     double *energy,double epsilon_inf[3][3],
                     double ewald_atmfrc[*nrpt][*natom][3][*natom][3][2],
                     double internal_strain[3][*natom][6],
                     double phfrq[*nqpt][3* *natom],double rprimd[3][3],double qph1l[*nqpt][3],
                     double short_atmfrc[*nrpt][*natom][3][*natom][3][2],
                     int typat[*natom],double xcart[*natom][3],double zeff[*natom][3][3]){
  xmlDocPtr doc;
  char * pch;
  int iatom,iamu,irpt,iqpt,present;
  int ia,ib,mu,nu,voigt;
  int i,j;
  xmlNodePtr cur,cur2;
  xmlChar *key,*uri;

  if (*natom <= 0){ 
    printf(" error: The number of atom must be superior to zero\n");
    exit(0);
  }

  iatom   = 0;
  iamu    = 0;
  present = 0;
  irpt    = 0;
  iqpt   = 0;
  voigt   = 0;

  doc = xmlParseFile(filename);
  if (doc == NULL) printf(" error: could not parse file file.xml\n");

  cur = xmlDocGetRootElement(doc);
  if (cur == NULL) {
    fprintf(stderr," The document is empty \n");
    xmlFreeDoc(doc);
    return;
  }
 
  cur = cur->xmlChildrenNode;
  while (cur != NULL) {
    if ((!xmlStrcmp(cur->name, (const  xmlChar *) "energy"))) {
      key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
      *energy = strtod(key, NULL);
      xmlFree(key);
    }
    else if ((!xmlStrcmp(cur->name, (const  xmlChar *) "unit_cell"))) {
      key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
      char * pch;
      i=0 ; j=0;
      pch = strtok(key,"\t \n");
      for(mu=0;mu<3;mu++){
        for(nu=0;nu<3;nu++){
          if (pch != NULL){
            rprimd[mu][nu]=strtod(pch,NULL);
            pch = strtok(NULL,"\t \n");
          }
        }
      }
      xmlFree(key);
    }
    else if ((!xmlStrcmp(cur->name, (const  xmlChar *) "epsilon_inf"))) {
      key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
      char * pch;
      i=0 ; j=0;
      pch = strtok(key,"\t \n");
      for(mu=0;mu<3;mu++){
        for(nu=0;nu<3;nu++){
          if (pch != NULL){
            epsilon_inf[mu][nu]=strtod(pch,NULL);
            pch = strtok(NULL,"\t \n");
          }
        }
      }
      xmlFree(key);
    }
    else if ((!xmlStrcmp(cur->name, (const  xmlChar *) "elastic"))) {
      key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
      char * pch;
      i=0 ; j=0;
      pch = strtok(key,"\t \n");
      for(mu=0;mu<6;mu++){
        for(nu=0;nu<6;nu++){
          if (pch != NULL){
            elastic_constants[mu][nu]=strtod(pch,NULL);
            pch = strtok(NULL,"\t \n");
          }
        }
      }
      xmlFree(key);
    }
    else if ((!xmlStrcmp(cur->name, (const  xmlChar *) "atom"))) {
      uri = xmlGetProp(cur, (const  xmlChar *) "mass");
      present = 0;
      //1) fill the atomic mass unit
      for(i=0;i<=*ntypat;i++){
        if(amu[i]==strtod(uri,NULL)){
          present = 1;
          break;
        }
      }
      if(present==0){
        amu[iamu]=strtod(uri,NULL);
        iamu++;
      }
      // fill the typat table
      for(i=0;i<=*ntypat;i++){
        if(amu[i]==strtod(uri,NULL)){
          typat[iatom]=i+1;
        }
      }
      xmlFree(uri);
      
      cur2 = cur->xmlChildrenNode;
      while (cur2 != NULL) {
        if (iatom<=*natom) {
          if ((!xmlStrcmp(cur2->name, (const  xmlChar *) "position"))) {
            key = xmlNodeListGetString(doc, cur2->xmlChildrenNode, 1);
            pch = strtok(key,"\t \n");
            for(mu=0;mu<3;mu++){
              if (pch != NULL){
                xcart[iatom][mu]=strtod(pch,NULL);
                pch = strtok(NULL,"\t \n");
              }
            }
          }
          if ((!xmlStrcmp(cur2->name, (const  xmlChar *) "borncharge"))) {
            key = xmlNodeListGetString(doc, cur2->xmlChildrenNode, 1);
            pch = strtok(key,"\t \n");
            for(mu=0;mu<3;mu++){
              for(nu=0;nu<3;nu++){
                if (pch != NULL){
                  zeff[iatom][mu][nu]=strtod(pch,NULL);
                  pch = strtok(NULL,"\t \n");
                }
              }
            }
            iatom++;
          }
        }
        else{
          printf(" error: The number of atom doesn't match with the XML file\n");
          exit(0);
        } 
        cur2 = cur2->next;
      }
    }
    else if ((!xmlStrcmp(cur->name, (const  xmlChar *) "local_force_constant"))) {      
      cur2 = cur->xmlChildrenNode;
      while (cur2 != NULL) {
        if (irpt<=*nrpt) {
          if ((!xmlStrcmp(cur2->name, (const  xmlChar *) "data"))) {
            key = xmlNodeListGetString(doc, cur2->xmlChildrenNode, 1);
            pch = strtok(key,"\t \n");
            for(ia=0;ia<*natom;ia++){
              for(mu=0;mu<3;mu++){
                for(ib=0;ib<*natom;ib++){
                  for(nu=0;nu<3;nu++){
                    if (pch != NULL){
                      short_atmfrc[irpt][ib][nu][ia][mu][0]=strtod(pch,NULL);
                      pch = strtok(NULL,"\t \n");
                    }
                  }
                }
              }
            }
          }
        }
        else{
          printf(" error: The number of ifc doesn't match with the XML file %d %d\n",irpt,*nrpt);
          exit(0);
        } 
        cur2 = cur2->next;
      }
    }
    else if ((!xmlStrcmp(cur->name, (const  xmlChar *) "total_force_constant"))) {      
      cur2 = cur->xmlChildrenNode;
      while (cur2 != NULL) {
        if (irpt<=*nrpt) {
          if ((!xmlStrcmp(cur2->name, (const  xmlChar *) "data"))) {
            key = xmlNodeListGetString(doc, cur2->xmlChildrenNode, 1);
            pch = strtok(key,"\t \n");
            for(ia=0;ia<*natom;ia++){
              for(mu=0;mu<3;mu++){
                for(ib=0;ib<*natom;ib++){
                  for(nu=0;nu<3;nu++){
                    if (pch != NULL){
                      atmfrc[irpt][ib][nu][ia][mu][0]=strtod(pch,NULL);
                      ewald_atmfrc[irpt][ib][nu][ia][mu][0]=strtod(pch,NULL);
                      ewald_atmfrc[irpt][ib][nu][ia][mu][0]-=short_atmfrc[irpt][ib][nu][ia][mu][0];
                      pch = strtok(NULL,"\t \n");
                    }
                  }
                }
              }
            }
          }
          if ((!xmlStrcmp(cur2->name, (const  xmlChar *) "cell"))) {
            key = xmlNodeListGetString(doc, cur2->xmlChildrenNode, 1);
            pch = strtok(key,"\t \n");
            for(i=0;i<3;i++){
              cell[irpt][i]=atoi(pch);
              pch = strtok(NULL,"\t \n");
            }
          }
        }
        else{
          printf(" error: The number of ifc doesn't match with the XML file %d %d\n",irpt,*nrpt);
          exit(0);
        } 
        cur2 = cur2->next;
      }
      irpt++;
    }
    else if ((!xmlStrcmp(cur->name, (const  xmlChar *) "phonon"))){
      cur2 = cur->xmlChildrenNode;
      while (cur2 != NULL) {
        if (iqpt<=*nqpt) {
          if ((!xmlStrcmp(cur2->name, (const  xmlChar *) "qpoint"))) {
            key = xmlNodeListGetString(doc, cur2->xmlChildrenNode, 1);
            pch = strtok(key,"\t \n");
            for(mu=0;mu<3;mu++){
              if (pch != NULL){
                qph1l[iqpt][mu]=strtod(pch,NULL);
                pch = strtok(NULL,"\t \n");
              }
            }
          }
          if ((!xmlStrcmp(cur2->name, (const  xmlChar *) "frequencies"))) {
            key = xmlNodeListGetString(doc, cur2->xmlChildrenNode, 1);
            pch = strtok(key,"\t \n");
            for(mu=0;mu<3**natom;mu++){
              if (pch != NULL){
                phfrq[iqpt][mu]=strtod(pch,NULL);
                pch = strtok(NULL,"\t \n");
              }
            }
          }
          if ((!xmlStrcmp(cur2->name, (const  xmlChar *) "dynamical_matrix"))) {
            key = xmlNodeListGetString(doc, cur2->xmlChildrenNode, 1);
            pch = strtok(key,"\t \n");
            for(ia=0;ia<*natom;ia++){
              for(mu=0;mu<3;mu++){
                for(ib=0;ib<*natom;ib++){
                  for(nu=0;nu<3;nu++){
                    if (pch != NULL){
                      dynmat[iqpt][ia][mu][ib][nu][0]=strtod(pch,NULL);
                      pch = strtok(NULL,"\t \n");
                    }
                  }
                }
              }
            }
          }
        }
        else{
          printf(" error: The number of qpoints doesn't match with the XML file %d %d\n",irpt,*nrpt);
          exit(0);
        }
        cur2 = cur2->next;
      }
      iqpt++;
    }
    else if ((!xmlStrcmp(cur->name, (const  xmlChar *) "strain_coupling"))){
      uri = xmlGetProp(cur, (const  xmlChar *) "voigt");
      cur2 = cur->xmlChildrenNode;
      while (cur2 != NULL) {
        if (voigt<=12) {
          if ((!xmlStrcmp(cur2->name, (const  xmlChar *) "correction_force"))) {
            key = xmlNodeListGetString(doc, cur2->xmlChildrenNode, 1);
            pch = strtok(key,"\t \n");
            for(ia=0;ia<*natom;ia++){
              for(mu=0;mu<3;mu++){
                if (pch != NULL){
                  internal_strain[mu][ia][atoi(uri)]=strtod(pch,NULL);
                  pch = strtok(NULL,"\t \n");
                }
              }
            }
          }
          if ((!xmlStrcmp(cur2->name, (const  xmlChar *) "correction_force_constant"))) {
            key = xmlNodeListGetString(doc, cur2->xmlChildrenNode, 1);
            pch = strtok(key,"\t \n");
            for(ia=0;ia<*natom;ia++){
              for(mu=0;mu<3;mu++){
                if (pch != NULL){
                  for(ia=0;ia<*natom;ia++){
                    for(mu=0;mu<3;mu++){
                      for(ib=0;ib<*natom;ib++){
                        for(nu=0;nu<3;nu++){
                          if (pch != NULL){
                            //to do
                            //short_atmfrc[irpt][ib][nu][ia][mu][0]=strtod(pch,NULL);
                            pch = strtok(NULL,"\t \n");
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
        else{
          printf(" error: The number of strain doesn't match with the XML file %d %d\n",irpt,*nrpt);
          exit(0);
        }
        cur2 = cur2->next;
      }
      xmlFree(uri);
    }

    cur = cur->next;
  }
  xmlFreeDoc(doc);
}


void effpot_xml_getValue(char *filename,char*name_key,char*result){
  xmlDocPtr doc;
  xmlNodePtr cur;
  xmlChar *key;
  
  doc = xmlParseFile(filename);
  if (doc == NULL) printf(" error: could not parse file file.xml\n");

  cur = xmlDocGetRootElement(doc);
  if (cur == NULL) {
    fprintf(stderr," The document is empty \n");
    xmlFreeDoc(doc);
    return;
  }
  cur = cur->xmlChildrenNode;
  while (cur != NULL) {
    if ((!xmlStrcmp(cur->name, (const  xmlChar *) name_key))) {
      key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
      printf(" keyword: %s\n", key);
      //      *result = key;
      xmlFree(key);
    }
    cur = cur->next;
  }
  xmlFreeDoc(doc);
}

void effpot_xml_getAttribute(char *filename,char*name_key,char*name_attributes,char*result){  
  xmlDocPtr doc;
  xmlNodePtr cur;
  xmlChar *key, *uri;

  doc = xmlParseFile(filename);
  if (doc == NULL) printf(" error: could not parse file file.xml\n");

  cur = xmlDocGetRootElement(doc);
  if (cur == NULL) {
    fprintf(stderr," The document is empty \n");
    xmlFreeDoc(doc);
    return;
  }
  cur = cur->xmlChildrenNode;
  while (cur != NULL) {
    if ((!xmlStrcmp(cur->name, (const  xmlChar *) name_key))) {
      uri = xmlGetProp(cur, name_attributes);
      printf("uri: %s\n", uri);
      xmlFree(uri);
    }
    cur = cur->next;
  }
  xmlFreeDoc(doc);
}

#endif
