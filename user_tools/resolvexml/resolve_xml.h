/*-
 * Copyright (C), 1988-2012, mymtom
 *
 * vi:set ts=4 sw=4:
 *
 * $Id$
 */

/**
 * @file	resolve_xml.h
 * @brief	
 */
/*serarch <parent_node></parent_node>*/
int list_sub_node(char *parent_node);

int check_attr_value(char *nodename);

int get_sub_value(char *node,char *attr_name);
