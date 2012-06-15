/*
 * This file is part of tiptop.
 *
 * Author: Antoine NAUDIN
 * Copyright (c) 2012 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

/*
 * This file is dedicated to parse the configuration file
 *
 */

#include "config.h"

#if HAVE_LIBXML2

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "options.h"
#include "target.h"
#include "xml-parser.h"
#include "screen.h"

static void option_update(xmlChar* name, xmlChar* val, struct option* opt);
static void parse_options(xmlDocPtr doc, xmlNodePtr cur, struct option* opt);
static void parse_screen(xmlDocPtr doc, xmlNodePtr cur);
static void parse_counters(screen_t* s, xmlDocPtr doc, xmlNodePtr cur);
static void parse_columns(screen_t* s, xmlDocPtr doc, xmlNodePtr cur);


/*
 *	Remplissage des structures
 */

static void option_update(xmlChar* name, xmlChar* val, struct option* opt)
{
  if(!xmlStrcmp(name, (const xmlChar *) "delay")) {
    opt->delay = atof((const char*)val);
  }
  if(!xmlStrcmp(name, (const xmlChar *) "cpu_threshold")) {
    opt->cpu_threshold = atof((char*)val);
  }
  if(!xmlStrcmp(name, (xmlChar *) "batch")) {
    opt->batch = (opt->batch || atoi((char*)val));
  }
  if(!xmlStrcmp(name, (xmlChar *) "show_cmdline"))
    opt->show_cmdline = (opt->show_cmdline || atoi((char*)val));

  if(!xmlStrcmp(name, (xmlChar *) "show_epoch"))
    opt->show_epoch = (opt->show_epoch || atoi((char*)val));

  if(!xmlStrcmp(name, (xmlChar *) "show_kernel"))
    opt->show_kernel = (opt->show_kernel || atoi((char*)val));

  if(!xmlStrcmp(name, (xmlChar *) "show_user"))
    opt->show_user = atoi((char*)val);

  if(!xmlStrcmp(name, (xmlChar *) "debug"))
    opt->debug = (opt->debug || atoi((char*)val));

  if(!xmlStrcmp(name, (xmlChar *) "watch_uid"))
    opt->watch_uid = (opt->watch_uid || atoi((char*)val));

  if(!xmlStrcmp(name, (xmlChar *) "watch_name"))
    opt->watch_name = strdup((char*)val);

  if(!xmlStrcmp(name, (xmlChar *) "max_iter"))
    opt->max_iter = (opt->max_iter || atoi((char*)val));

  if(!xmlStrcmp(name, (xmlChar *) "show_timestamp"))
    opt->show_timestamp = (opt->show_timestamp || atoi((char*)val));

  if(!xmlStrcmp(name, (xmlChar *) "show_threads")) {
    opt->show_threads = atoi((char*)val);
  }
  if(!xmlStrcmp(name, (const xmlChar *) "idle"))
    opt->idle = atoi((const char*)val);

  if(!xmlStrcmp(name, (const xmlChar *) "sticky"))
    opt->sticky = atoi((const char*)val);
}


/*
*	Parser
*/
static void parse_screen( xmlDocPtr doc, xmlNodePtr cur)
{
  xmlChar *desc=NULL, *name=NULL;
  screen_t* s;
  name = xmlGetProp(cur,(xmlChar*) "name");
  desc = xmlGetProp(cur,(xmlChar*) "desc");

  if (name == NULL)
    s = new_screen( "(Unknown)", (char*)desc);
  else
    s = new_screen((char*)name, (char*)desc);

  

  cur = cur->xmlChildrenNode;

  while (cur != NULL) {

    if ((!xmlStrcmp(cur->name, (const xmlChar *)"column")))
      parse_columns(s, doc, cur);
    else if ((!xmlStrcmp(cur->name, (const xmlChar *)"counter")))
      parse_counters(s, doc, cur);

    cur = cur->next;
  }
  xmlFree(desc);
  xmlFree(name);
}


static void parse_columns(screen_t* s, xmlDocPtr doc, xmlNodePtr cur)
{
  xmlChar *header=NULL, *format=NULL, *desc=NULL, *expr=NULL;
  header = xmlGetProp(cur, (xmlChar*) "header");
  format = xmlGetProp(cur, (xmlChar*) "format");
  desc = xmlGetProp(cur, (xmlChar*) "desc");
  expr = xmlGetProp(cur, (xmlChar*) "expr");

  if (header != NULL
      && format != NULL
      && desc != NULL
      && expr != NULL
      && strlen((char*)expr) > 0
      && strlen((char*)header) > 0
      && strlen((char*)format) > 0
      )
    /* Save column in tiptop struct "screen_t" */
    add_column(s, (char*) header, (char*) format, (char*) desc, (char*)expr);

  xmlFree(header);
  xmlFree(format);
  xmlFree(desc);
  xmlFree(expr);
}


static void parse_counters(screen_t* s, xmlDocPtr doc, xmlNodePtr cur)
{
  char *alias = NULL, *config = NULL, *type = NULL;
  char* arch = NULL;
  char* model = NULL;

  alias = (char*)xmlGetProp(cur,(xmlChar*) "alias");
  if (!alias)  /* no alias, cannot be referenced, hence useless */
    goto end;

  config = (char*)xmlGetProp(cur,(xmlChar*) "config");
  if (!config)  /* cannot be a valid counter without 'config' */
    goto end;

  arch = (char*)xmlGetProp(cur,(xmlChar*) "arch");
  if (arch && !match_target((char*)arch))
    goto end;

  model = (char*)xmlGetProp(cur,(xmlChar*) "model");
  if (model && !match_model((char*)model))
    goto end;

  type = (char*)xmlGetProp(cur, (xmlChar*) "type");


  add_counter(s, alias, config,  type);


 end:
  if (alias)
    xmlFree(alias);
  if (config)
    xmlFree(config);
  if (type)
  xmlFree(type);
  if (model)
    xmlFree(model);
  if (arch)
    xmlFree(arch);
}


static void parse_options(xmlDocPtr doc, xmlNodePtr cur, struct option* opt)
{
  xmlChar *name, *val;
  cur = cur->xmlChildrenNode;
  while (cur != NULL) {
    name = xmlGetProp(cur, (xmlChar*)"name");
    val = xmlGetProp(cur, (xmlChar*)"value");

    if (name != NULL && val != NULL)
      option_update(name, val, opt);

    xmlFree(val);
    xmlFree(name);

    cur = cur->next;
  }

}


/* Main function*/
int parse_doc(char* file, struct option* opt)
{
  xmlDocPtr doc;
  xmlNodePtr cur;

  doc = xmlParseFile(file);

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
      parse_options (doc, cur, opt);
    if ((!xmlStrcmp(cur->name, (const xmlChar *)"screen")))
      parse_screen (doc, cur);

    cur = cur->next;
  }
  xmlFreeDoc(doc);
  return 0;
}

#endif  /* HAVE_LIBXML2 */
