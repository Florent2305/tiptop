#include "options.h"

#include "config-parser.h"


struct option* opt=NULL;

conf_screen listS[128];
int nbS=0;


void option_update(xmlChar* name, xmlChar* val){

  if(!xmlStrcmp(name, (xmlChar *) "delay")){
    opt->delay =  atof((char*)val);
  }
  if(!xmlStrcmp(name, (xmlChar *) "cpu_threshold")){
    opt->cpu_threshold = atof((char*)val);
  }
  if(!xmlStrcmp(name, (xmlChar *) "batch")){
    opt->batch = atoi((char*)val);
  }
  if(!xmlStrcmp(name, (xmlChar *) "show_threads")){
    opt->show_threads = atoi((char*)val);
  }
  if(!xmlStrcmp(name, (const xmlChar *) "idle"))
    opt->idle = atoi((const char*)val);
  
  if(!xmlStrcmp(name, (const xmlChar *) "sticky"))
    opt->sticky = atoi((const char*)val);
  
  if(!xmlStrcmp(name, (const xmlChar *) "spown_pos"))
    opt->spawn_pos = atoi((const char*)val);
}

void parse_options(xmlDocPtr doc, xmlNodePtr cur)
{
  xmlChar *name, *val;

  cur = cur->xmlChildrenNode;
  while (cur != NULL) {
    name = xmlGetProp(cur, (xmlChar*)"name");
    val = xmlGetProp(cur, (xmlChar*)"value");    

    if(name != NULL && val != NULL)
      option_update(name, val); 
    
    xmlFree(val);
    xmlFree(name);
    
    cur = cur->next;
  }
 
}

int parse_doc(char* path, struct option* op){

  opt=op;

  xmlDocPtr doc;
  xmlNodePtr cur;

  doc = xmlParseFile(path);
  if (doc == NULL ) {
    fprintf(stderr,"Document not parsed successfully. \n");
    return -1;
  }

  cur = xmlDocGetRootElement(doc);

  if (cur == NULL) {
    fprintf(stderr,"empty document\n");
    xmlFreeDoc(doc);
    return -1;
  }
  if (xmlStrcmp(cur->name, (const xmlChar *) "tiptop")) {
    fprintf(stderr,"document of the wrong type, root node != screens");
    xmlFreeDoc(doc);
    return -1;
  }
  cur = cur->xmlChildrenNode;

  while (cur != NULL) {
    if ((!xmlStrcmp(cur->name, (const xmlChar *)"options")))
      parse_options (doc, cur);
    cur = cur->next;
  }

  xmlFreeDoc(doc);
  return 0;
}


