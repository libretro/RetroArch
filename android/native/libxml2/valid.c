/*
 * valid.c : part of the code use to do the DTD handling and the validity
 *           checking
 *
 * See Copyright for the status of this software.
 *
 * daniel@veillard.com
 */

#define IN_LIBXML
#include "libxml.h"

#include <string.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <libxml/xmlmemory.h>
#include <libxml/hash.h>
#include <libxml/uri.h>
#include <libxml/valid.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/xmlerror.h>
#include <libxml/list.h>
#include <libxml/globals.h>

static xmlElementPtr xmlGetDtdElementDesc2(xmlDtdPtr dtd, const xmlChar *name,
	                           int create);
/* #define DEBUG_VALID_ALGO */
/* #define DEBUG_REGEXP_ALGO */

#define TODO 								\
    xmlGenericError(xmlGenericErrorContext,				\
	    "Unimplemented block at %s:%d\n",				\
            __FILE__, __LINE__);

/************************************************************************
 *									*
 *			Error handling routines				*
 *									*
 ************************************************************************/

/**
 * xmlVErrMemory:
 * @ctxt:  an XML validation parser context
 * @extra:  extra informations
 *
 * Handle an out of memory error
 */
static void
xmlVErrMemory(xmlValidCtxtPtr ctxt, const char *extra)
{
    xmlGenericErrorFunc channel = NULL;
    xmlParserCtxtPtr pctxt = NULL;
    void *data = NULL;

    if (ctxt != NULL) {
        channel = ctxt->error;
        data = ctxt->userData;
	/* Use the special values to detect if it is part of a parsing
	   context */
	if ((ctxt->finishDtd == XML_CTXT_FINISH_DTD_0) ||
	    (ctxt->finishDtd == XML_CTXT_FINISH_DTD_1)) {
	    long delta = (char *) ctxt - (char *) ctxt->userData;
	    if ((delta > 0) && (delta < 250))
		pctxt = ctxt->userData;
	}
    }
    if (extra)
        __xmlRaiseError(NULL, channel, data,
                        pctxt, NULL, XML_FROM_VALID, XML_ERR_NO_MEMORY,
                        XML_ERR_FATAL, NULL, 0, extra, NULL, NULL, 0, 0,
                        "Memory allocation failed : %s\n", extra);
    else
        __xmlRaiseError(NULL, channel, data,
                        pctxt, NULL, XML_FROM_VALID, XML_ERR_NO_MEMORY,
                        XML_ERR_FATAL, NULL, 0, NULL, NULL, NULL, 0, 0,
                        "Memory allocation failed\n");
}

/**
 * xmlErrValid:
 * @ctxt:  an XML validation parser context
 * @error:  the error number
 * @extra:  extra informations
 *
 * Handle a validation error
 */
static void
xmlErrValid(xmlValidCtxtPtr ctxt, xmlParserErrors error,
            const char *msg, const char *extra)
{
    xmlGenericErrorFunc channel = NULL;
    xmlParserCtxtPtr pctxt = NULL;
    void *data = NULL;

    if (ctxt != NULL) {
        channel = ctxt->error;
        data = ctxt->userData;
	/* Use the special values to detect if it is part of a parsing
	   context */
	if ((ctxt->finishDtd == XML_CTXT_FINISH_DTD_0) ||
	    (ctxt->finishDtd == XML_CTXT_FINISH_DTD_1)) {
	    long delta = (char *) ctxt - (char *) ctxt->userData;
	    if ((delta > 0) && (delta < 250))
		pctxt = ctxt->userData;
	}
    }
    if (extra)
        __xmlRaiseError(NULL, channel, data,
                        pctxt, NULL, XML_FROM_VALID, error,
                        XML_ERR_ERROR, NULL, 0, extra, NULL, NULL, 0, 0,
                        msg, extra);
    else
        __xmlRaiseError(NULL, channel, data,
                        pctxt, NULL, XML_FROM_VALID, error,
                        XML_ERR_ERROR, NULL, 0, NULL, NULL, NULL, 0, 0,
                        "%s", msg);
}

/**
 * xmlNewDocElementContent:
 * @doc:  the document
 * @name:  the subelement name or NULL
 * @type:  the type of element content decl
 *
 * Allocate an element content structure for the document.
 *
 * Returns NULL if not, otherwise the new element content structure
 */
xmlElementContentPtr
xmlNewDocElementContent(xmlDocPtr doc, const xmlChar *name,
                        xmlElementContentType type) {
    xmlElementContentPtr ret;
    xmlDictPtr dict = NULL;

    if (doc != NULL)
        dict = doc->dict;

    switch(type) {
	case XML_ELEMENT_CONTENT_ELEMENT:
	    if (name == NULL) {
	        xmlErrValid(NULL, XML_ERR_INTERNAL_ERROR,
			"xmlNewElementContent : name == NULL !\n",
			NULL);
	    }
	    break;
        case XML_ELEMENT_CONTENT_PCDATA:
	case XML_ELEMENT_CONTENT_SEQ:
	case XML_ELEMENT_CONTENT_OR:
	    if (name != NULL) {
	        xmlErrValid(NULL, XML_ERR_INTERNAL_ERROR,
			"xmlNewElementContent : name != NULL !\n",
			NULL);
	    }
	    break;
	default:
	    xmlErrValid(NULL, XML_ERR_INTERNAL_ERROR, 
		    "Internal: ELEMENT content corrupted invalid type\n",
		    NULL);
	    return(NULL);
    }
    ret = (xmlElementContentPtr) xmlMalloc(sizeof(xmlElementContent));
    if (ret == NULL) {
	xmlVErrMemory(NULL, "malloc failed");
	return(NULL);
    }
    memset(ret, 0, sizeof(xmlElementContent));
    ret->type = type;
    ret->ocur = XML_ELEMENT_CONTENT_ONCE;
    if (name != NULL) {
        int l;
	const xmlChar *tmp;

	tmp = xmlSplitQName3(name, &l);
	if (tmp == NULL) {
	    if (dict == NULL)
		ret->name = xmlStrdup(name);
	    else
	        ret->name = xmlDictLookup(dict, name, -1);
	} else {
	    if (dict == NULL) {
		ret->prefix = xmlStrndup(name, l);
		ret->name = xmlStrdup(tmp);
	    } else {
	        ret->prefix = xmlDictLookup(dict, name, l);
		ret->name = xmlDictLookup(dict, tmp, -1);
	    }
	}
    }
    return(ret);
}

/**
 * xmlNewElementContent:
 * @name:  the subelement name or NULL
 * @type:  the type of element content decl
 *
 * Allocate an element content structure.
 * Deprecated in favor of xmlNewDocElementContent
 *
 * Returns NULL if not, otherwise the new element content structure
 */
xmlElementContentPtr
xmlNewElementContent(const xmlChar *name, xmlElementContentType type) {
    return(xmlNewDocElementContent(NULL, name, type));
}

/**
 * xmlCopyDocElementContent:
 * @doc:  the document owning the element declaration
 * @cur:  An element content pointer.
 *
 * Build a copy of an element content description.
 * 
 * Returns the new xmlElementContentPtr or NULL in case of error.
 */
xmlElementContentPtr
xmlCopyDocElementContent(xmlDocPtr doc, xmlElementContentPtr cur) {
    xmlElementContentPtr ret = NULL, prev = NULL, tmp;
    xmlDictPtr dict = NULL;

    if (cur == NULL) return(NULL);

    if (doc != NULL)
        dict = doc->dict;

    ret = (xmlElementContentPtr) xmlMalloc(sizeof(xmlElementContent));
    if (ret == NULL) {
	xmlVErrMemory(NULL, "malloc failed");
	return(NULL);
    }
    memset(ret, 0, sizeof(xmlElementContent));
    ret->type = cur->type;
    ret->ocur = cur->ocur;
    if (cur->name != NULL) {
	if (dict)
	    ret->name = xmlDictLookup(dict, cur->name, -1);
	else
	    ret->name = xmlStrdup(cur->name);
    }
    
    if (cur->prefix != NULL) {
	if (dict)
	    ret->prefix = xmlDictLookup(dict, cur->prefix, -1);
	else
	    ret->prefix = xmlStrdup(cur->prefix);
    }
    if (cur->c1 != NULL)
        ret->c1 = xmlCopyDocElementContent(doc, cur->c1);
    if (ret->c1 != NULL)
	ret->c1->parent = ret;
    if (cur->c2 != NULL) {
        prev = ret;
	cur = cur->c2;
	while (cur != NULL) {
	    tmp = (xmlElementContentPtr) xmlMalloc(sizeof(xmlElementContent));
	    if (tmp == NULL) {
		xmlVErrMemory(NULL, "malloc failed");
		return(ret);
	    }
	    memset(tmp, 0, sizeof(xmlElementContent));
	    tmp->type = cur->type;
	    tmp->ocur = cur->ocur;
	    prev->c2 = tmp;
	    if (cur->name != NULL) {
		if (dict)
		    tmp->name = xmlDictLookup(dict, cur->name, -1);
		else
		    tmp->name = xmlStrdup(cur->name);
	    }
	    
	    if (cur->prefix != NULL) {
		if (dict)
		    tmp->prefix = xmlDictLookup(dict, cur->prefix, -1);
		else
		    tmp->prefix = xmlStrdup(cur->prefix);
	    }
	    if (cur->c1 != NULL)
	        tmp->c1 = xmlCopyDocElementContent(doc,cur->c1);
	    if (tmp->c1 != NULL)
		tmp->c1->parent = ret;
	    prev = tmp;
	    cur = cur->c2;
	}
    }
    return(ret);
}

/**
 * xmlCopyElementContent:
 * @cur:  An element content pointer.
 *
 * Build a copy of an element content description.
 * Deprecated, use xmlCopyDocElementContent instead
 * 
 * Returns the new xmlElementContentPtr or NULL in case of error.
 */
xmlElementContentPtr
xmlCopyElementContent(xmlElementContentPtr cur) {
    return(xmlCopyDocElementContent(NULL, cur));
}

/**
 * xmlFreeDocElementContent:
 * @doc: the document owning the element declaration
 * @cur:  the element content tree to free
 *
 * Free an element content structure. The whole subtree is removed.
 */
void
xmlFreeDocElementContent(xmlDocPtr doc, xmlElementContentPtr cur) {
    xmlElementContentPtr next;
    xmlDictPtr dict = NULL;

    if (doc != NULL)
        dict = doc->dict;

    while (cur != NULL) {
        next = cur->c2;
	switch (cur->type) {
	    case XML_ELEMENT_CONTENT_PCDATA:
	    case XML_ELEMENT_CONTENT_ELEMENT:
	    case XML_ELEMENT_CONTENT_SEQ:
	    case XML_ELEMENT_CONTENT_OR:
		break;
	    default:
		xmlErrValid(NULL, XML_ERR_INTERNAL_ERROR, 
			"Internal: ELEMENT content corrupted invalid type\n",
			NULL);
		return;
	}
	if (cur->c1 != NULL) xmlFreeDocElementContent(doc, cur->c1);
	if (dict) {
	    if ((cur->name != NULL) && (!xmlDictOwns(dict, cur->name)))
	        xmlFree((xmlChar *) cur->name);
	    if ((cur->prefix != NULL) && (!xmlDictOwns(dict, cur->prefix)))
	        xmlFree((xmlChar *) cur->prefix);
	} else {
	    if (cur->name != NULL) xmlFree((xmlChar *) cur->name);
	    if (cur->prefix != NULL) xmlFree((xmlChar *) cur->prefix);
	}
	xmlFree(cur);
	cur = next;
    }
}

/**
 * xmlFreeElementContent:
 * @cur:  the element content tree to free
 *
 * Free an element content structure. The whole subtree is removed.
 * Deprecated, use xmlFreeDocElementContent instead
 */
void
xmlFreeElementContent(xmlElementContentPtr cur) {
    xmlFreeDocElementContent(NULL, cur);
}

/**
 * xmlSnprintfElementContent:
 * @buf:  an output buffer
 * @size:  the buffer size
 * @content:  An element table
 * @englob: 1 if one must print the englobing parenthesis, 0 otherwise
 *
 * This will dump the content of the element content definition
 * Intended just for the debug routine
 */
void
xmlSnprintfElementContent(char *buf, int size, xmlElementContentPtr content, int englob) {
    int len;

    if (content == NULL) return;
    len = strlen(buf);
    if (size - len < 50) {
	if ((size - len > 4) && (buf[len - 1] != '.'))
	    strcat(buf, " ...");
	return;
    }
    if (englob) strcat(buf, "(");
    switch (content->type) {
        case XML_ELEMENT_CONTENT_PCDATA:
            strcat(buf, "#PCDATA");
	    break;
	case XML_ELEMENT_CONTENT_ELEMENT:
	    if (content->prefix != NULL) {
		if (size - len < xmlStrlen(content->prefix) + 10) {
		    strcat(buf, " ...");
		    return;
		}
		strcat(buf, (char *) content->prefix);
		strcat(buf, ":");
	    }
	    if (size - len < xmlStrlen(content->name) + 10) {
		strcat(buf, " ...");
		return;
	    }
	    if (content->name != NULL)
		strcat(buf, (char *) content->name);
	    break;
	case XML_ELEMENT_CONTENT_SEQ:
	    if ((content->c1->type == XML_ELEMENT_CONTENT_OR) ||
	        (content->c1->type == XML_ELEMENT_CONTENT_SEQ))
		xmlSnprintfElementContent(buf, size, content->c1, 1);
	    else
		xmlSnprintfElementContent(buf, size, content->c1, 0);
	    len = strlen(buf);
	    if (size - len < 50) {
		if ((size - len > 4) && (buf[len - 1] != '.'))
		    strcat(buf, " ...");
		return;
	    }
            strcat(buf, " , ");
	    if (((content->c2->type == XML_ELEMENT_CONTENT_OR) ||
		 (content->c2->ocur != XML_ELEMENT_CONTENT_ONCE)) &&
		(content->c2->type != XML_ELEMENT_CONTENT_ELEMENT))
		xmlSnprintfElementContent(buf, size, content->c2, 1);
	    else
		xmlSnprintfElementContent(buf, size, content->c2, 0);
	    break;
	case XML_ELEMENT_CONTENT_OR:
	    if ((content->c1->type == XML_ELEMENT_CONTENT_OR) ||
	        (content->c1->type == XML_ELEMENT_CONTENT_SEQ))
		xmlSnprintfElementContent(buf, size, content->c1, 1);
	    else
		xmlSnprintfElementContent(buf, size, content->c1, 0);
	    len = strlen(buf);
	    if (size - len < 50) {
		if ((size - len > 4) && (buf[len - 1] != '.'))
		    strcat(buf, " ...");
		return;
	    }
            strcat(buf, " | ");
	    if (((content->c2->type == XML_ELEMENT_CONTENT_SEQ) ||
		 (content->c2->ocur != XML_ELEMENT_CONTENT_ONCE)) &&
		(content->c2->type != XML_ELEMENT_CONTENT_ELEMENT))
		xmlSnprintfElementContent(buf, size, content->c2, 1);
	    else
		xmlSnprintfElementContent(buf, size, content->c2, 0);
	    break;
    }
    if (englob)
        strcat(buf, ")");
    switch (content->ocur) {
        case XML_ELEMENT_CONTENT_ONCE:
	    break;
        case XML_ELEMENT_CONTENT_OPT:
	    strcat(buf, "?");
	    break;
        case XML_ELEMENT_CONTENT_MULT:
	    strcat(buf, "*");
	    break;
        case XML_ELEMENT_CONTENT_PLUS:
	    strcat(buf, "+");
	    break;
    }
}

/****************************************************************
 *								*
 *	Registration of DTD declarations			*
 *								*
 ****************************************************************/

/**
 * xmlFreeElement:
 * @elem:  An element
 *
 * Deallocate the memory used by an element definition
 */
static void
xmlFreeElement(xmlElementPtr elem) {
    if (elem == NULL) return;
    xmlUnlinkNode((xmlNodePtr) elem);
    xmlFreeDocElementContent(elem->doc, elem->content);
    if (elem->name != NULL)
	xmlFree((xmlChar *) elem->name);
    if (elem->prefix != NULL)
	xmlFree((xmlChar *) elem->prefix);
    xmlFree(elem);
}


/**
 * xmlAddElementDecl:
 * @ctxt:  the validation context
 * @dtd:  pointer to the DTD
 * @name:  the entity name
 * @type:  the element type
 * @content:  the element content tree or NULL
 *
 * Register a new element declaration
 *
 * Returns NULL if not, otherwise the entity
 */
xmlElementPtr
xmlAddElementDecl(xmlValidCtxtPtr ctxt,
                  xmlDtdPtr dtd, const xmlChar *name,
                  xmlElementTypeVal type,
		  xmlElementContentPtr content) {
    xmlElementPtr ret;
    xmlElementTablePtr table;
    xmlAttributePtr oldAttributes = NULL;
    xmlChar *ns, *uqname;

    if (dtd == NULL) {
	return(NULL);
    }
    if (name == NULL) {
	return(NULL);
    }

    switch (type) {
        case XML_ELEMENT_TYPE_EMPTY:
	    if (content != NULL) {
		xmlErrValid(ctxt, XML_ERR_INTERNAL_ERROR, 
		        "xmlAddElementDecl: content != NULL for EMPTY\n",
			NULL);
		return(NULL);
	    }
	    break;
	case XML_ELEMENT_TYPE_ANY:
	    if (content != NULL) {
		xmlErrValid(ctxt, XML_ERR_INTERNAL_ERROR, 
		        "xmlAddElementDecl: content != NULL for ANY\n",
			NULL);
		return(NULL);
	    }
	    break;
	case XML_ELEMENT_TYPE_MIXED:
	    if (content == NULL) {
		xmlErrValid(ctxt, XML_ERR_INTERNAL_ERROR, 
		        "xmlAddElementDecl: content == NULL for MIXED\n",
			NULL);
		return(NULL);
	    }
	    break;
	case XML_ELEMENT_TYPE_ELEMENT:
	    if (content == NULL) {
		xmlErrValid(ctxt, XML_ERR_INTERNAL_ERROR, 
		        "xmlAddElementDecl: content == NULL for ELEMENT\n",
			NULL);
		return(NULL);
	    }
	    break;
	default:
	    xmlErrValid(ctxt, XML_ERR_INTERNAL_ERROR, 
		    "Internal: ELEMENT decl corrupted invalid type\n",
		    NULL);
	    return(NULL);
    }

    /*
     * check if name is a QName
     */
    uqname = xmlSplitQName2(name, &ns);
    if (uqname != NULL)
	name = uqname;

    /*
     * Create the Element table if needed.
     */
    table = (xmlElementTablePtr) dtd->elements;
    if (table == NULL) {
	xmlDictPtr dict = NULL;

	if (dtd->doc != NULL)
	    dict = dtd->doc->dict;
        table = xmlHashCreateDict(0, dict);
	dtd->elements = (void *) table;
    }
    if (table == NULL) {
	xmlVErrMemory(ctxt,
            "xmlAddElementDecl: Table creation failed!\n");
	if (uqname != NULL)
	    xmlFree(uqname);
	if (ns != NULL)
	    xmlFree(ns);
        return(NULL);
    }

    /*
     * lookup old attributes inserted on an undefined element in the
     * internal subset.
     */
    if ((dtd->doc != NULL) && (dtd->doc->intSubset != NULL)) {
	ret = xmlHashLookup2(dtd->doc->intSubset->elements, name, ns);
	if ((ret != NULL) && (ret->etype == XML_ELEMENT_TYPE_UNDEFINED)) {
	    oldAttributes = ret->attributes;
	    ret->attributes = NULL;
	    xmlHashRemoveEntry2(dtd->doc->intSubset->elements, name, ns, NULL);
	    xmlFreeElement(ret);
	}
    }

    /*
     * The element may already be present if one of its attribute
     * was registered first
     */
    ret = xmlHashLookup2(table, name, ns);
    if (ret != NULL) {
	if (ret->etype != XML_ELEMENT_TYPE_UNDEFINED) {
	    if (uqname != NULL)
		xmlFree(uqname);
            if (ns != NULL)
	        xmlFree(ns);
	    return(NULL);
	}
	if (ns != NULL) {
	    xmlFree(ns);
	    ns = NULL;
	}
    } else {
	ret = (xmlElementPtr) xmlMalloc(sizeof(xmlElement));
	if (ret == NULL) {
	    xmlVErrMemory(ctxt, "malloc failed");
	    if (uqname != NULL)
		xmlFree(uqname);
            if (ns != NULL)
	        xmlFree(ns);
	    return(NULL);
	}
	memset(ret, 0, sizeof(xmlElement));
	ret->type = XML_ELEMENT_DECL;

	/*
	 * fill the structure.
	 */
	ret->name = xmlStrdup(name);
	if (ret->name == NULL) {
	    xmlVErrMemory(ctxt, "malloc failed");
	    if (uqname != NULL)
		xmlFree(uqname);
            if (ns != NULL)
	        xmlFree(ns);
	    xmlFree(ret);
	    return(NULL);
	}
	ret->prefix = ns;

	/*
	 * Validity Check:
	 * Insertion must not fail
	 */
	if (xmlHashAddEntry2(table, name, ns, ret)) {
	    xmlFreeElement(ret);
	    if (uqname != NULL)
		xmlFree(uqname);
	    return(NULL);
	}
	/*
	 * For new element, may have attributes from earlier
	 * definition in internal subset
	 */
	ret->attributes = oldAttributes;
    }

    /*
     * Finish to fill the structure.
     */
    ret->etype = type;
    /*
     * Avoid a stupid copy when called by the parser
     * and flag it by setting a special parent value
     * so the parser doesn't unallocate it.
     */
    if ((ctxt != NULL) &&
        ((ctxt->finishDtd == XML_CTXT_FINISH_DTD_0) ||
         (ctxt->finishDtd == XML_CTXT_FINISH_DTD_1))) {
	ret->content = content;
	if (content != NULL)
	    content->parent = (xmlElementContentPtr) 1;
    } else {
	ret->content = xmlCopyDocElementContent(dtd->doc, content);
    }

    /*
     * Link it to the DTD
     */
    ret->parent = dtd;
    ret->doc = dtd->doc;
    if (dtd->last == NULL) {
	dtd->children = dtd->last = (xmlNodePtr) ret;
    } else {
        dtd->last->next = (xmlNodePtr) ret;
	ret->prev = dtd->last;
	dtd->last = (xmlNodePtr) ret;
    }
    if (uqname != NULL)
	xmlFree(uqname);
    return(ret);
}

/**
 * xmlFreeElementTable:
 * @table:  An element table
 *
 * Deallocate the memory used by an element hash table.
 */
void
xmlFreeElementTable(xmlElementTablePtr table) {
    xmlHashFree(table, (xmlHashDeallocator) xmlFreeElement);
}

#ifdef LIBXML_TREE_ENABLED
/**
 * xmlCopyElement:
 * @elem:  An element
 *
 * Build a copy of an element.
 * 
 * Returns the new xmlElementPtr or NULL in case of error.
 */
static xmlElementPtr
xmlCopyElement(xmlElementPtr elem) {
    xmlElementPtr cur;

    cur = (xmlElementPtr) xmlMalloc(sizeof(xmlElement));
    if (cur == NULL) {
	xmlVErrMemory(NULL, "malloc failed");
	return(NULL);
    }
    memset(cur, 0, sizeof(xmlElement));
    cur->type = XML_ELEMENT_DECL;
    cur->etype = elem->etype;
    if (elem->name != NULL)
	cur->name = xmlStrdup(elem->name);
    else
	cur->name = NULL;
    if (elem->prefix != NULL)
	cur->prefix = xmlStrdup(elem->prefix);
    else
	cur->prefix = NULL;
    cur->content = xmlCopyElementContent(elem->content);
    /* TODO : rebuild the attribute list on the copy */
    cur->attributes = NULL;
    return(cur);
}

/**
 * xmlCopyElementTable:
 * @table:  An element table
 *
 * Build a copy of an element table.
 * 
 * Returns the new xmlElementTablePtr or NULL in case of error.
 */
xmlElementTablePtr
xmlCopyElementTable(xmlElementTablePtr table) {
    return((xmlElementTablePtr) xmlHashCopy(table,
		                            (xmlHashCopier) xmlCopyElement));
}
#endif /* LIBXML_TREE_ENABLED */

/**
 * xmlCreateEnumeration:
 * @name:  the enumeration name or NULL
 *
 * create and initialize an enumeration attribute node.
 *
 * Returns the xmlEnumerationPtr just created or NULL in case
 *                of error.
 */
xmlEnumerationPtr
xmlCreateEnumeration(const xmlChar *name) {
    xmlEnumerationPtr ret;

    ret = (xmlEnumerationPtr) xmlMalloc(sizeof(xmlEnumeration));
    if (ret == NULL) {
	xmlVErrMemory(NULL, "malloc failed");
        return(NULL);
    }
    memset(ret, 0, sizeof(xmlEnumeration));

    if (name != NULL)
        ret->name = xmlStrdup(name);
    return(ret);
}

/**
 * xmlFreeEnumeration:
 * @cur:  the tree to free.
 *
 * free an enumeration attribute node (recursive).
 */
void
xmlFreeEnumeration(xmlEnumerationPtr cur) {
    if (cur == NULL) return;

    if (cur->next != NULL) xmlFreeEnumeration(cur->next);

    if (cur->name != NULL) xmlFree((xmlChar *) cur->name);
    xmlFree(cur);
}

#ifdef LIBXML_TREE_ENABLED
/**
 * xmlCopyEnumeration:
 * @cur:  the tree to copy.
 *
 * Copy an enumeration attribute node (recursive).
 *
 * Returns the xmlEnumerationPtr just created or NULL in case
 *                of error.
 */
xmlEnumerationPtr
xmlCopyEnumeration(xmlEnumerationPtr cur) {
    xmlEnumerationPtr ret;

    if (cur == NULL) return(NULL);
    ret = xmlCreateEnumeration((xmlChar *) cur->name);

    if (cur->next != NULL) ret->next = xmlCopyEnumeration(cur->next);
    else ret->next = NULL;

    return(ret);
}
#endif /* LIBXML_TREE_ENABLED */

/**
 * xmlFreeAttribute:
 * @elem:  An attribute
 *
 * Deallocate the memory used by an attribute definition
 */
static void
xmlFreeAttribute(xmlAttributePtr attr) {
    xmlDictPtr dict;

    if (attr == NULL) return;
    if (attr->doc != NULL)
	dict = attr->doc->dict;
    else
	dict = NULL;
    xmlUnlinkNode((xmlNodePtr) attr);
    if (attr->tree != NULL)
        xmlFreeEnumeration(attr->tree);
    if (dict) {
        if ((attr->elem != NULL) && (!xmlDictOwns(dict, attr->elem)))
	    xmlFree((xmlChar *) attr->elem);
        if ((attr->name != NULL) && (!xmlDictOwns(dict, attr->name)))
	    xmlFree((xmlChar *) attr->name);
        if ((attr->prefix != NULL) && (!xmlDictOwns(dict, attr->prefix)))
	    xmlFree((xmlChar *) attr->prefix);
        if ((attr->defaultValue != NULL) &&
	    (!xmlDictOwns(dict, attr->defaultValue)))
	    xmlFree((xmlChar *) attr->defaultValue);
    } else {
	if (attr->elem != NULL)
	    xmlFree((xmlChar *) attr->elem);
	if (attr->name != NULL)
	    xmlFree((xmlChar *) attr->name);
	if (attr->defaultValue != NULL)
	    xmlFree((xmlChar *) attr->defaultValue);
	if (attr->prefix != NULL)
	    xmlFree((xmlChar *) attr->prefix);
    }
    xmlFree(attr);
}


/**
 * xmlAddAttributeDecl:
 * @ctxt:  the validation context
 * @dtd:  pointer to the DTD
 * @elem:  the element name
 * @name:  the attribute name
 * @ns:  the attribute namespace prefix
 * @type:  the attribute type
 * @def:  the attribute default type
 * @defaultValue:  the attribute default value
 * @tree:  if it's an enumeration, the associated list
 *
 * Register a new attribute declaration
 * Note that @tree becomes the ownership of the DTD
 *
 * Returns NULL if not new, otherwise the attribute decl
 */
xmlAttributePtr
xmlAddAttributeDecl(xmlValidCtxtPtr ctxt,
                    xmlDtdPtr dtd, const xmlChar *elem,
                    const xmlChar *name, const xmlChar *ns,
		    xmlAttributeType type, xmlAttributeDefault def,
		    const xmlChar *defaultValue, xmlEnumerationPtr tree) {
    xmlAttributePtr ret;
    xmlAttributeTablePtr table;
    xmlElementPtr elemDef;
    xmlDictPtr dict = NULL;

    if (dtd == NULL) {
	xmlFreeEnumeration(tree);
	return(NULL);
    }
    if (name == NULL) {
	xmlFreeEnumeration(tree);
	return(NULL);
    }
    if (elem == NULL) {
	xmlFreeEnumeration(tree);
	return(NULL);
    }
    if (dtd->doc != NULL)
	dict = dtd->doc->dict;

    /*
     * Check first that an attribute defined in the external subset wasn't
     * already defined in the internal subset
     */
    if ((dtd->doc != NULL) && (dtd->doc->extSubset == dtd) &&
	(dtd->doc->intSubset != NULL) &&
	(dtd->doc->intSubset->attributes != NULL)) {
        ret = xmlHashLookup3(dtd->doc->intSubset->attributes, name, ns, elem);
	if (ret != NULL) {
	    xmlFreeEnumeration(tree);
	    return(NULL);
	}
    }

    /*
     * Create the Attribute table if needed.
     */
    table = (xmlAttributeTablePtr) dtd->attributes;
    if (table == NULL) {
        table = xmlHashCreateDict(0, dict);
	dtd->attributes = (void *) table;
    }
    if (table == NULL) {
	xmlVErrMemory(ctxt,
            "xmlAddAttributeDecl: Table creation failed!\n");
	xmlFreeEnumeration(tree);
        return(NULL);
    }


    ret = (xmlAttributePtr) xmlMalloc(sizeof(xmlAttribute));
    if (ret == NULL) {
	xmlVErrMemory(ctxt, "malloc failed");
	xmlFreeEnumeration(tree);
	return(NULL);
    }
    memset(ret, 0, sizeof(xmlAttribute));
    ret->type = XML_ATTRIBUTE_DECL;

    /*
     * fill the structure.
     */
    ret->atype = type;
    /*
     * doc must be set before possible error causes call
     * to xmlFreeAttribute (because it's used to check on
     * dict use)
     */
    ret->doc = dtd->doc;
    if (dict) {
	ret->name = xmlDictLookup(dict, name, -1);
	ret->prefix = xmlDictLookup(dict, ns, -1);
	ret->elem = xmlDictLookup(dict, elem, -1);
    } else {
	ret->name = xmlStrdup(name);
	ret->prefix = xmlStrdup(ns);
	ret->elem = xmlStrdup(elem);
    }
    ret->def = def;
    ret->tree = tree;
    if (defaultValue != NULL) {
        if (dict)
	    ret->defaultValue = xmlDictLookup(dict, defaultValue, -1);
	else
	    ret->defaultValue = xmlStrdup(defaultValue);
    }

    /*
     * Validity Check:
     * Search the DTD for previous declarations of the ATTLIST
     */
    if (xmlHashAddEntry3(table, ret->name, ret->prefix, ret->elem, ret) < 0) {
	xmlFreeAttribute(ret);
	return(NULL);
    }

    /*
     * Validity Check:
     * Multiple ID per element
     */
    elemDef = xmlGetDtdElementDesc2(dtd, elem, 1);
    if (elemDef != NULL) {


	/*
	 * Insert namespace default def first they need to be
	 * processed first.
	 */
	if ((xmlStrEqual(ret->name, BAD_CAST "xmlns")) ||
	    ((ret->prefix != NULL &&
	     (xmlStrEqual(ret->prefix, BAD_CAST "xmlns"))))) {
	    ret->nexth = elemDef->attributes;
	    elemDef->attributes = ret;
	} else {
	    xmlAttributePtr tmp = elemDef->attributes;

	    while ((tmp != NULL) &&
		   ((xmlStrEqual(tmp->name, BAD_CAST "xmlns")) ||
		    ((ret->prefix != NULL &&
		     (xmlStrEqual(ret->prefix, BAD_CAST "xmlns")))))) {
		if (tmp->nexth == NULL)
		    break;
		tmp = tmp->nexth;
	    }
	    if (tmp != NULL) {
		ret->nexth = tmp->nexth;
	        tmp->nexth = ret;
	    } else {
		ret->nexth = elemDef->attributes;
		elemDef->attributes = ret;
	    }
	}
    }

    /*
     * Link it to the DTD
     */
    ret->parent = dtd;
    if (dtd->last == NULL) {
	dtd->children = dtd->last = (xmlNodePtr) ret;
    } else {
        dtd->last->next = (xmlNodePtr) ret;
	ret->prev = dtd->last;
	dtd->last = (xmlNodePtr) ret;
    }
    return(ret);
}

/**
 * xmlFreeAttributeTable:
 * @table:  An attribute table
 *
 * Deallocate the memory used by an entities hash table.
 */
void
xmlFreeAttributeTable(xmlAttributeTablePtr table) {
    xmlHashFree(table, (xmlHashDeallocator) xmlFreeAttribute);
}

#ifdef LIBXML_TREE_ENABLED
/**
 * xmlCopyAttribute:
 * @attr:  An attribute
 *
 * Build a copy of an attribute.
 * 
 * Returns the new xmlAttributePtr or NULL in case of error.
 */
static xmlAttributePtr
xmlCopyAttribute(xmlAttributePtr attr) {
    xmlAttributePtr cur;

    cur = (xmlAttributePtr) xmlMalloc(sizeof(xmlAttribute));
    if (cur == NULL) {
	xmlVErrMemory(NULL, "malloc failed");
	return(NULL);
    }
    memset(cur, 0, sizeof(xmlAttribute));
    cur->type = XML_ATTRIBUTE_DECL;
    cur->atype = attr->atype;
    cur->def = attr->def;
    cur->tree = xmlCopyEnumeration(attr->tree);
    if (attr->elem != NULL)
	cur->elem = xmlStrdup(attr->elem);
    if (attr->name != NULL)
	cur->name = xmlStrdup(attr->name);
    if (attr->prefix != NULL)
	cur->prefix = xmlStrdup(attr->prefix);
    if (attr->defaultValue != NULL)
	cur->defaultValue = xmlStrdup(attr->defaultValue);
    return(cur);
}

/**
 * xmlCopyAttributeTable:
 * @table:  An attribute table
 *
 * Build a copy of an attribute table.
 * 
 * Returns the new xmlAttributeTablePtr or NULL in case of error.
 */
xmlAttributeTablePtr
xmlCopyAttributeTable(xmlAttributeTablePtr table) {
    return((xmlAttributeTablePtr) xmlHashCopy(table,
				    (xmlHashCopier) xmlCopyAttribute));
}
#endif /* LIBXML_TREE_ENABLED */

/************************************************************************
 *									*
 *				NOTATIONs				*
 *									*
 ************************************************************************/
/**
 * xmlFreeNotation:
 * @not:  A notation
 *
 * Deallocate the memory used by an notation definition
 */
static void
xmlFreeNotation(xmlNotationPtr nota) {
    if (nota == NULL) return;
    if (nota->name != NULL)
	xmlFree((xmlChar *) nota->name);
    if (nota->PublicID != NULL)
	xmlFree((xmlChar *) nota->PublicID);
    if (nota->SystemID != NULL)
	xmlFree((xmlChar *) nota->SystemID);
    xmlFree(nota);
}


/**
 * xmlAddNotationDecl:
 * @dtd:  pointer to the DTD
 * @ctxt:  the validation context
 * @name:  the entity name
 * @PublicID:  the public identifier or NULL
 * @SystemID:  the system identifier or NULL
 *
 * Register a new notation declaration
 *
 * Returns NULL if not, otherwise the entity
 */
xmlNotationPtr
xmlAddNotationDecl(xmlValidCtxtPtr ctxt, xmlDtdPtr dtd,
	           const xmlChar *name,
                   const xmlChar *PublicID, const xmlChar *SystemID) {
    xmlNotationPtr ret;
    xmlNotationTablePtr table;

    if (dtd == NULL) {
	return(NULL);
    }
    if (name == NULL) {
	return(NULL);
    }
    if ((PublicID == NULL) && (SystemID == NULL)) {
	return(NULL);
    }

    /*
     * Create the Notation table if needed.
     */
    table = (xmlNotationTablePtr) dtd->notations;
    if (table == NULL) {
	xmlDictPtr dict = NULL;
	if (dtd->doc != NULL)
	    dict = dtd->doc->dict;

        dtd->notations = table = xmlHashCreateDict(0, dict);
    }
    if (table == NULL) {
	xmlVErrMemory(ctxt,
		"xmlAddNotationDecl: Table creation failed!\n");
        return(NULL);
    }

    ret = (xmlNotationPtr) xmlMalloc(sizeof(xmlNotation));
    if (ret == NULL) {
	xmlVErrMemory(ctxt, "malloc failed");
	return(NULL);
    }
    memset(ret, 0, sizeof(xmlNotation));

    /*
     * fill the structure.
     */
    ret->name = xmlStrdup(name);
    if (SystemID != NULL)
        ret->SystemID = xmlStrdup(SystemID);
    if (PublicID != NULL)
        ret->PublicID = xmlStrdup(PublicID);

    /*
     * Validity Check:
     * Check the DTD for previous declarations of the ATTLIST
     */
    if (xmlHashAddEntry(table, name, ret)) {
	xmlFreeNotation(ret);
	return(NULL);
    }
    return(ret);
}

/**
 * xmlFreeNotationTable:
 * @table:  An notation table
 *
 * Deallocate the memory used by an entities hash table.
 */
void
xmlFreeNotationTable(xmlNotationTablePtr table) {
    xmlHashFree(table, (xmlHashDeallocator) xmlFreeNotation);
}

#ifdef LIBXML_TREE_ENABLED
/**
 * xmlCopyNotation:
 * @nota:  A notation
 *
 * Build a copy of a notation.
 * 
 * Returns the new xmlNotationPtr or NULL in case of error.
 */
static xmlNotationPtr
xmlCopyNotation(xmlNotationPtr nota) {
    xmlNotationPtr cur;

    cur = (xmlNotationPtr) xmlMalloc(sizeof(xmlNotation));
    if (cur == NULL) {
	xmlVErrMemory(NULL, "malloc failed");
	return(NULL);
    }
    if (nota->name != NULL)
	cur->name = xmlStrdup(nota->name);
    else
	cur->name = NULL;
    if (nota->PublicID != NULL)
	cur->PublicID = xmlStrdup(nota->PublicID);
    else
	cur->PublicID = NULL;
    if (nota->SystemID != NULL)
	cur->SystemID = xmlStrdup(nota->SystemID);
    else
	cur->SystemID = NULL;
    return(cur);
}

/**
 * xmlCopyNotationTable:
 * @table:  A notation table
 *
 * Build a copy of a notation table.
 * 
 * Returns the new xmlNotationTablePtr or NULL in case of error.
 */
xmlNotationTablePtr
xmlCopyNotationTable(xmlNotationTablePtr table) {
    return((xmlNotationTablePtr) xmlHashCopy(table,
				    (xmlHashCopier) xmlCopyNotation));
}
#endif /* LIBXML_TREE_ENABLED */

/************************************************************************
 *									*
 *				IDs					*
 *									*
 ************************************************************************/
/**
 * DICT_FREE:
 * @str:  a string
 *
 * Free a string if it is not owned by the "dict" dictionnary in the
 * current scope
 */
#define DICT_FREE(str)						\
	if ((str) && ((!dict) || 				\
	    (xmlDictOwns(dict, (const xmlChar *)(str)) == 0)))	\
	    xmlFree((char *)(str));

/**
 * xmlFreeID:
 * @not:  A id
 *
 * Deallocate the memory used by an id definition
 */
static void
xmlFreeID(xmlIDPtr id) {
    xmlDictPtr dict = NULL;

    if (id == NULL) return;

    if (id->doc != NULL)
        dict = id->doc->dict;

    if (id->value != NULL)
	DICT_FREE(id->value)
    if (id->name != NULL)
	DICT_FREE(id->name)
    xmlFree(id);
}


/**
 * xmlAddID:
 * @ctxt:  the validation context
 * @doc:  pointer to the document
 * @value:  the value name
 * @attr:  the attribute holding the ID
 *
 * Register a new id declaration
 *
 * Returns NULL if not, otherwise the new xmlIDPtr
 */
xmlIDPtr 
xmlAddID(xmlValidCtxtPtr ctxt, xmlDocPtr doc, const xmlChar *value,
         xmlAttrPtr attr) {
    xmlIDPtr ret;
    xmlIDTablePtr table;

    if (doc == NULL) {
	return(NULL);
    }
    if (value == NULL) {
	return(NULL);
    }
    if (attr == NULL) {
	return(NULL);
    }

    /*
     * Create the ID table if needed.
     */
    table = (xmlIDTablePtr) doc->ids;
    if (table == NULL)  {
        doc->ids = table = xmlHashCreateDict(0, doc->dict);
    }
    if (table == NULL) {
	xmlVErrMemory(ctxt,
		"xmlAddID: Table creation failed!\n");
        return(NULL);
    }

    ret = (xmlIDPtr) xmlMalloc(sizeof(xmlID));
    if (ret == NULL) {
	xmlVErrMemory(ctxt, "malloc failed");
	return(NULL);
    }

    /*
     * fill the structure.
     */
    ret->value = xmlStrdup(value);
    ret->doc = doc;
    if ((ctxt != NULL) && (ctxt->vstateNr != 0)) {
	/*
	 * Operating in streaming mode, attr is gonna disapear
	 */
	if (doc->dict != NULL)
	    ret->name = xmlDictLookup(doc->dict, attr->name, -1);
	else
	    ret->name = xmlStrdup(attr->name);
	ret->attr = NULL;
    } else {
	ret->attr = attr;
	ret->name = NULL;
    }
    ret->lineno = xmlGetLineNo(attr->parent);

    if (xmlHashAddEntry(table, value, ret) < 0) {
	xmlFreeID(ret);
	return(NULL);
    }
    if (attr != NULL)
	attr->atype = XML_ATTRIBUTE_ID;
    return(ret);
}

/**
 * xmlFreeIDTable:
 * @table:  An id table
 *
 * Deallocate the memory used by an ID hash table.
 */
void
xmlFreeIDTable(xmlIDTablePtr table) {
    xmlHashFree(table, (xmlHashDeallocator) xmlFreeID);
}

/**
 * xmlIsID:
 * @doc:  the document
 * @elem:  the element carrying the attribute
 * @attr:  the attribute
 *
 * Determine whether an attribute is of type ID. In case we have DTD(s)
 * then this is done if DTD loading has been requested. In the case
 * of HTML documents parsed with the HTML parser, then ID detection is
 * done systematically.
 *
 * Returns 0 or 1 depending on the lookup result
 */
int
xmlIsID(xmlDocPtr doc, xmlNodePtr elem, xmlAttrPtr attr) {
    if ((attr == NULL) || (attr->name == NULL)) return(0);
    if ((attr->ns != NULL) && (attr->ns->prefix != NULL) &&
        (!strcmp((char *) attr->name, "id")) &&
        (!strcmp((char *) attr->ns->prefix, "xml")))
	return(1);
    if (doc == NULL) return(0);
    if ((doc->intSubset == NULL) && (doc->extSubset == NULL) &&
        (doc->type != XML_HTML_DOCUMENT_NODE)) {
	return(0);
    } else if (doc->type == XML_HTML_DOCUMENT_NODE) {
        if ((xmlStrEqual(BAD_CAST "id", attr->name)) ||
	    ((xmlStrEqual(BAD_CAST "name", attr->name)) &&
	    ((elem == NULL) || (xmlStrEqual(elem->name, BAD_CAST "a")))))
	    return(1);
	return(0);    
    } else if (elem == NULL) {
	return(0);
    } else {
	xmlAttributePtr attrDecl = NULL;

	xmlChar felem[50], fattr[50];
	xmlChar *fullelemname, *fullattrname;

	fullelemname = (elem->ns != NULL && elem->ns->prefix != NULL) ?
	    xmlBuildQName(elem->name, elem->ns->prefix, felem, 50) :
	    (xmlChar *)elem->name;

	fullattrname = (attr->ns != NULL && attr->ns->prefix != NULL) ?
	    xmlBuildQName(attr->name, attr->ns->prefix, fattr, 50) :
	    (xmlChar *)attr->name;

	if (fullelemname != NULL && fullattrname != NULL) {
	    attrDecl = xmlGetDtdAttrDesc(doc->intSubset, fullelemname,
		                         fullattrname);
	    if ((attrDecl == NULL) && (doc->extSubset != NULL))
		attrDecl = xmlGetDtdAttrDesc(doc->extSubset, fullelemname,
					     fullattrname);
	}

	if ((fullattrname != fattr) && (fullattrname != attr->name))
	    xmlFree(fullattrname);
	if ((fullelemname != felem) && (fullelemname != elem->name))
	    xmlFree(fullelemname);

        if ((attrDecl != NULL) && (attrDecl->atype == XML_ATTRIBUTE_ID))
	    return(1);
    }
    return(0);
}

/**
 * xmlRemoveID:
 * @doc:  the document
 * @attr:  the attribute
 *
 * Remove the given attribute from the ID table maintained internally.
 *
 * Returns -1 if the lookup failed and 0 otherwise
 */
int
xmlRemoveID(xmlDocPtr doc, xmlAttrPtr attr) {
    xmlIDTablePtr table;
    xmlIDPtr id;
    xmlChar *ID;

    if (doc == NULL) return(-1);
    if (attr == NULL) return(-1);
    table = (xmlIDTablePtr) doc->ids;
    if (table == NULL) 
        return(-1);

    if (attr == NULL)
	return(-1);
    ID = xmlNodeListGetString(doc, attr->children, 1);
    if (ID == NULL)
	return(-1);
    id = xmlHashLookup(table, ID);
    if (id == NULL || id->attr != attr) {
	xmlFree(ID);
	return(-1);
    }
    xmlHashRemoveEntry(table, ID, (xmlHashDeallocator) xmlFreeID);
    xmlFree(ID);
	attr->atype = 0;
    return(0);
}

/**
 * xmlGetID:
 * @doc:  pointer to the document
 * @ID:  the ID value
 *
 * Search the attribute declaring the given ID
 *
 * Returns NULL if not found, otherwise the xmlAttrPtr defining the ID
 */
xmlAttrPtr 
xmlGetID(xmlDocPtr doc, const xmlChar *ID) {
    xmlIDTablePtr table;
    xmlIDPtr id;

    if (doc == NULL) {
	return(NULL);
    }

    if (ID == NULL) {
	return(NULL);
    }

    table = (xmlIDTablePtr) doc->ids;
    if (table == NULL) 
        return(NULL);

    id = xmlHashLookup(table, ID);
    if (id == NULL)
	return(NULL);
    if (id->attr == NULL) {
	/*
	 * We are operating on a stream, return a well known reference
	 * since the attribute node doesn't exist anymore
	 */
	return((xmlAttrPtr) doc);
    }
    return(id->attr);
}

/************************************************************************
 *									*
 *				Refs					*
 *									*
 ************************************************************************/
typedef struct xmlRemoveMemo_t 
{
	xmlListPtr l;
	xmlAttrPtr ap;
} xmlRemoveMemo;

typedef xmlRemoveMemo *xmlRemoveMemoPtr;

typedef struct xmlValidateMemo_t 
{
    xmlValidCtxtPtr ctxt;
    const xmlChar *name;
} xmlValidateMemo;

typedef xmlValidateMemo *xmlValidateMemoPtr;

/**
 * xmlFreeRef:
 * @lk:  A list link
 *
 * Deallocate the memory used by a ref definition
 */
static void
xmlFreeRef(xmlLinkPtr lk) {
    xmlRefPtr ref = (xmlRefPtr)xmlLinkGetData(lk);
    if (ref == NULL) return;
    if (ref->value != NULL)
        xmlFree((xmlChar *)ref->value);
    if (ref->name != NULL)
        xmlFree((xmlChar *)ref->name);
    xmlFree(ref);
}

/**
 * xmlFreeRefList:
 * @list_ref:  A list of references.
 *
 * Deallocate the memory used by a list of references
 */
static void
xmlFreeRefList(xmlListPtr list_ref) {
    if (list_ref == NULL) return;
    xmlListDelete(list_ref);
}

/**
 * xmlWalkRemoveRef:
 * @data:  Contents of current link
 * @user:  Value supplied by the user
 *
 * Returns 0 to abort the walk or 1 to continue
 */
static int
xmlWalkRemoveRef(const void *data, const void *user)
{
    xmlAttrPtr attr0 = ((xmlRefPtr)data)->attr;
    xmlAttrPtr attr1 = ((xmlRemoveMemoPtr)user)->ap;
    xmlListPtr ref_list = ((xmlRemoveMemoPtr)user)->l;

    if (attr0 == attr1) { /* Matched: remove and terminate walk */
        xmlListRemoveFirst(ref_list, (void *)data);
        return 0;
    }
    return 1;
}

/**
 * xmlDummyCompare
 * @data0:  Value supplied by the user
 * @data1:  Value supplied by the user
 *
 * Do nothing, return 0. Used to create unordered lists.
 */
static int
xmlDummyCompare(const void *data0 ATTRIBUTE_UNUSED,
                const void *data1 ATTRIBUTE_UNUSED)
{
    return (0);
}

/**
 * xmlAddRef:
 * @ctxt:  the validation context
 * @doc:  pointer to the document
 * @value:  the value name
 * @attr:  the attribute holding the Ref
 *
 * Register a new ref declaration
 *
 * Returns NULL if not, otherwise the new xmlRefPtr
 */
xmlRefPtr 
xmlAddRef(xmlValidCtxtPtr ctxt, xmlDocPtr doc, const xmlChar *value,
    xmlAttrPtr attr) {
    xmlRefPtr ret;
    xmlRefTablePtr table;
    xmlListPtr ref_list;

    if (doc == NULL) {
        return(NULL);
    }
    if (value == NULL) {
        return(NULL);
    }
    if (attr == NULL) {
        return(NULL);
    }

    /*
     * Create the Ref table if needed.
     */
    table = (xmlRefTablePtr) doc->refs;
    if (table == NULL) {
        doc->refs = table = xmlHashCreateDict(0, doc->dict);
    }
    if (table == NULL) {
	xmlVErrMemory(ctxt,
            "xmlAddRef: Table creation failed!\n");
        return(NULL);
    }

    ret = (xmlRefPtr) xmlMalloc(sizeof(xmlRef));
    if (ret == NULL) {
	xmlVErrMemory(ctxt, "malloc failed");
        return(NULL);
    }

    /*
     * fill the structure.
     */
    ret->value = xmlStrdup(value);
    if ((ctxt != NULL) && (ctxt->vstateNr != 0)) {
	/*
	 * Operating in streaming mode, attr is gonna disapear
	 */
	ret->name = xmlStrdup(attr->name);
	ret->attr = NULL;
    } else {
	ret->name = NULL;
	ret->attr = attr;
    }
    ret->lineno = xmlGetLineNo(attr->parent);

    /* To add a reference :-
     * References are maintained as a list of references,
     * Lookup the entry, if no entry create new nodelist
     * Add the owning node to the NodeList
     * Return the ref
     */

    if (NULL == (ref_list = xmlHashLookup(table, value))) {
        if (NULL == (ref_list = xmlListCreate(xmlFreeRef, xmlDummyCompare))) {
	    xmlErrValid(NULL, XML_ERR_INTERNAL_ERROR,
		    "xmlAddRef: Reference list creation failed!\n",
		    NULL);
	    goto failed;
        }
        if (xmlHashAddEntry(table, value, ref_list) < 0) {
            xmlListDelete(ref_list);
	    xmlErrValid(NULL, XML_ERR_INTERNAL_ERROR,
		    "xmlAddRef: Reference list insertion failed!\n",
		    NULL);
	    goto failed;
        }
    }
    if (xmlListAppend(ref_list, ret) != 0) {
	xmlErrValid(NULL, XML_ERR_INTERNAL_ERROR,
		    "xmlAddRef: Reference list insertion failed!\n",
		    NULL);
        goto failed;
    }
    return(ret);
failed:
    if (ret != NULL) {
        if (ret->value != NULL)
	    xmlFree((char *)ret->value);
        if (ret->name != NULL)
	    xmlFree((char *)ret->name);
        xmlFree(ret);
    }
    return(NULL);
}

/**
 * xmlFreeRefTable:
 * @table:  An ref table
 *
 * Deallocate the memory used by an Ref hash table.
 */
void
xmlFreeRefTable(xmlRefTablePtr table) {
    xmlHashFree(table, (xmlHashDeallocator) xmlFreeRefList);
}

/**
 * xmlIsRef:
 * @doc:  the document
 * @elem:  the element carrying the attribute
 * @attr:  the attribute
 *
 * Determine whether an attribute is of type Ref. In case we have DTD(s)
 * then this is simple, otherwise we use an heuristic: name Ref (upper
 * or lowercase).
 *
 * Returns 0 or 1 depending on the lookup result
 */
int
xmlIsRef(xmlDocPtr doc, xmlNodePtr elem, xmlAttrPtr attr) {
    if (attr == NULL)
        return(0);
    if (doc == NULL) {
        doc = attr->doc;
	if (doc == NULL) return(0);
    }

    if ((doc->intSubset == NULL) && (doc->extSubset == NULL)) {
        return(0);
    } else if (doc->type == XML_HTML_DOCUMENT_NODE) {
        /* TODO @@@ */
        return(0);    
    } else {
        xmlAttributePtr attrDecl;

        if (elem == NULL) return(0);
        attrDecl = xmlGetDtdAttrDesc(doc->intSubset, elem->name, attr->name);
        if ((attrDecl == NULL) && (doc->extSubset != NULL))
            attrDecl = xmlGetDtdAttrDesc(doc->extSubset,
		                         elem->name, attr->name);

	if ((attrDecl != NULL) &&
	    (attrDecl->atype == XML_ATTRIBUTE_IDREF ||
	     attrDecl->atype == XML_ATTRIBUTE_IDREFS))
	return(1);
    }
    return(0);
}

/**
 * xmlRemoveRef:
 * @doc:  the document
 * @attr:  the attribute
 *
 * Remove the given attribute from the Ref table maintained internally.
 *
 * Returns -1 if the lookup failed and 0 otherwise
 */
int
xmlRemoveRef(xmlDocPtr doc, xmlAttrPtr attr) {
    xmlListPtr ref_list;
    xmlRefTablePtr table;
    xmlChar *ID;
    xmlRemoveMemo target;

    if (doc == NULL) return(-1);
    if (attr == NULL) return(-1);
    table = (xmlRefTablePtr) doc->refs;
    if (table == NULL) 
        return(-1);

    if (attr == NULL)
        return(-1);
    ID = xmlNodeListGetString(doc, attr->children, 1);
    if (ID == NULL)
        return(-1);
    ref_list = xmlHashLookup(table, ID);

    if(ref_list == NULL) {
        xmlFree(ID);
        return (-1);
    }
    /* At this point, ref_list refers to a list of references which
     * have the same key as the supplied attr. Our list of references
     * is ordered by reference address and we don't have that information
     * here to use when removing. We'll have to walk the list and
     * check for a matching attribute, when we find one stop the walk
     * and remove the entry.
     * The list is ordered by reference, so that means we don't have the
     * key. Passing the list and the reference to the walker means we
     * will have enough data to be able to remove the entry.
     */
    target.l = ref_list;
    target.ap = attr;
    
    /* Remove the supplied attr from our list */
    xmlListWalk(ref_list, xmlWalkRemoveRef, &target);

    /*If the list is empty then remove the list entry in the hash */
    if (xmlListEmpty(ref_list))
        xmlHashUpdateEntry(table, ID, NULL, (xmlHashDeallocator)
        xmlFreeRefList);
    xmlFree(ID);
    return(0);
}

/**
 * xmlGetRefs:
 * @doc:  pointer to the document
 * @ID:  the ID value
 *
 * Find the set of references for the supplied ID. 
 *
 * Returns NULL if not found, otherwise node set for the ID.
 */
xmlListPtr 
xmlGetRefs(xmlDocPtr doc, const xmlChar *ID) {
    xmlRefTablePtr table;

    if (doc == NULL) {
        return(NULL);
    }

    if (ID == NULL) {
        return(NULL);
    }

    table = (xmlRefTablePtr) doc->refs;
    if (table == NULL) 
        return(NULL);

    return (xmlHashLookup(table, ID));
}

/************************************************************************
 *									*
 *		Routines for validity checking				*
 *									*
 ************************************************************************/

/**
 * xmlGetDtdElementDesc:
 * @dtd:  a pointer to the DtD to search
 * @name:  the element name
 *
 * Search the DTD for the description of this element
 *
 * returns the xmlElementPtr if found or NULL
 */

xmlElementPtr
xmlGetDtdElementDesc(xmlDtdPtr dtd, const xmlChar *name) {
    xmlElementTablePtr table;
    xmlElementPtr cur;
    xmlChar *uqname = NULL, *prefix = NULL;

    if ((dtd == NULL) || (name == NULL)) return(NULL);
    if (dtd->elements == NULL)
	return(NULL);
    table = (xmlElementTablePtr) dtd->elements;

    uqname = xmlSplitQName2(name, &prefix);
    if (uqname != NULL)
        name = uqname;
    cur = xmlHashLookup2(table, name, prefix);
    if (prefix != NULL) xmlFree(prefix);
    if (uqname != NULL) xmlFree(uqname);
    return(cur);
}
/**
 * xmlGetDtdElementDesc2:
 * @dtd:  a pointer to the DtD to search
 * @name:  the element name
 * @create:  create an empty description if not found
 *
 * Search the DTD for the description of this element
 *
 * returns the xmlElementPtr if found or NULL
 */

static xmlElementPtr
xmlGetDtdElementDesc2(xmlDtdPtr dtd, const xmlChar *name, int create) {
    xmlElementTablePtr table;
    xmlElementPtr cur;
    xmlChar *uqname = NULL, *prefix = NULL;

    if (dtd == NULL) return(NULL);
    if (dtd->elements == NULL) {
	xmlDictPtr dict = NULL;

	if (dtd->doc != NULL)
	    dict = dtd->doc->dict;

	if (!create) 
	    return(NULL);
	/*
	 * Create the Element table if needed.
	 */
	table = (xmlElementTablePtr) dtd->elements;
	if (table == NULL) {
	    table = xmlHashCreateDict(0, dict);
	    dtd->elements = (void *) table;
	}
	if (table == NULL) {
	    xmlVErrMemory(NULL, "element table allocation failed");
	    return(NULL);
	}
    }
    table = (xmlElementTablePtr) dtd->elements;

    uqname = xmlSplitQName2(name, &prefix);
    if (uqname != NULL)
        name = uqname;
    cur = xmlHashLookup2(table, name, prefix);
    if ((cur == NULL) && (create)) {
	cur = (xmlElementPtr) xmlMalloc(sizeof(xmlElement));
	if (cur == NULL) {
	    xmlVErrMemory(NULL, "malloc failed");
	    return(NULL);
	}
	memset(cur, 0, sizeof(xmlElement));
	cur->type = XML_ELEMENT_DECL;

	/*
	 * fill the structure.
	 */
	cur->name = xmlStrdup(name);
	cur->prefix = xmlStrdup(prefix);
	cur->etype = XML_ELEMENT_TYPE_UNDEFINED;

	xmlHashAddEntry2(table, name, prefix, cur);
    }
    if (prefix != NULL) xmlFree(prefix);
    if (uqname != NULL) xmlFree(uqname);
    return(cur);
}

/**
 * xmlGetDtdQElementDesc:
 * @dtd:  a pointer to the DtD to search
 * @name:  the element name
 * @prefix:  the element namespace prefix
 *
 * Search the DTD for the description of this element
 *
 * returns the xmlElementPtr if found or NULL
 */

xmlElementPtr
xmlGetDtdQElementDesc(xmlDtdPtr dtd, const xmlChar *name,
	              const xmlChar *prefix) {
    xmlElementTablePtr table;

    if (dtd == NULL) return(NULL);
    if (dtd->elements == NULL) return(NULL);
    table = (xmlElementTablePtr) dtd->elements;

    return(xmlHashLookup2(table, name, prefix));
}

/**
 * xmlGetDtdAttrDesc:
 * @dtd:  a pointer to the DtD to search
 * @elem:  the element name
 * @name:  the attribute name
 *
 * Search the DTD for the description of this attribute on
 * this element.
 *
 * returns the xmlAttributePtr if found or NULL
 */

xmlAttributePtr
xmlGetDtdAttrDesc(xmlDtdPtr dtd, const xmlChar *elem, const xmlChar *name) {
    xmlAttributeTablePtr table;
    xmlAttributePtr cur;
    xmlChar *uqname = NULL, *prefix = NULL;

    if (dtd == NULL) return(NULL);
    if (dtd->attributes == NULL) return(NULL);

    table = (xmlAttributeTablePtr) dtd->attributes;
    if (table == NULL)
	return(NULL);

    uqname = xmlSplitQName2(name, &prefix);

    if (uqname != NULL) {
	cur = xmlHashLookup3(table, uqname, prefix, elem);
	if (prefix != NULL) xmlFree(prefix);
	if (uqname != NULL) xmlFree(uqname);
    } else
	cur = xmlHashLookup3(table, name, NULL, elem);
    return(cur);
}

/**
 * xmlGetDtdQAttrDesc:
 * @dtd:  a pointer to the DtD to search
 * @elem:  the element name
 * @name:  the attribute name
 * @prefix:  the attribute namespace prefix
 *
 * Search the DTD for the description of this qualified attribute on
 * this element.
 *
 * returns the xmlAttributePtr if found or NULL
 */

xmlAttributePtr
xmlGetDtdQAttrDesc(xmlDtdPtr dtd, const xmlChar *elem, const xmlChar *name,
	          const xmlChar *prefix) {
    xmlAttributeTablePtr table;

    if (dtd == NULL) return(NULL);
    if (dtd->attributes == NULL) return(NULL);
    table = (xmlAttributeTablePtr) dtd->attributes;

    return(xmlHashLookup3(table, name, prefix, elem));
}

/**
 * xmlGetDtdNotationDesc:
 * @dtd:  a pointer to the DtD to search
 * @name:  the notation name
 *
 * Search the DTD for the description of this notation
 *
 * returns the xmlNotationPtr if found or NULL
 */

xmlNotationPtr
xmlGetDtdNotationDesc(xmlDtdPtr dtd, const xmlChar *name) {
    xmlNotationTablePtr table;

    if (dtd == NULL) return(NULL);
    if (dtd->notations == NULL) return(NULL);
    table = (xmlNotationTablePtr) dtd->notations;

    return(xmlHashLookup(table, name));
}

/**
 * xmlIsMixedElement:
 * @doc:  the document
 * @name:  the element name
 *
 * Search in the DtDs whether an element accept Mixed content (or ANY)
 * basically if it is supposed to accept text childs
 *
 * returns 0 if no, 1 if yes, and -1 if no element description is available
 */

int
xmlIsMixedElement(xmlDocPtr doc, const xmlChar *name) {
    xmlElementPtr elemDecl;

    if ((doc == NULL) || (doc->intSubset == NULL)) return(-1);

    elemDecl = xmlGetDtdElementDesc(doc->intSubset, name);
    if ((elemDecl == NULL) && (doc->extSubset != NULL))
	elemDecl = xmlGetDtdElementDesc(doc->extSubset, name);
    if (elemDecl == NULL) return(-1);
    switch (elemDecl->etype) {
	case XML_ELEMENT_TYPE_UNDEFINED:
	    return(-1);
	case XML_ELEMENT_TYPE_ELEMENT:
	    return(0);
        case XML_ELEMENT_TYPE_EMPTY:
	    /*
	     * return 1 for EMPTY since we want VC error to pop up
	     * on <empty>     </empty> for example
	     */
	case XML_ELEMENT_TYPE_ANY:
	case XML_ELEMENT_TYPE_MIXED:
	    return(1);
    }
    return(1);
}

#define bottom_valid
#include "elfgcchack.h"
