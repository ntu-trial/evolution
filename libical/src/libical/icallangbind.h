/* -*- Mode: C -*-
  ======================================================================
  FILE: icallangbind.h
  CREATOR: eric 25 jan 2001
  
  DESCRIPTION:
  
  $Id$
  $Locker$

  (C) COPYRIGHT 1999 Eric Busboom 
  http://www.softwarestudio.org
  
  This package is free software and is provided "as is" without
  express or implied warranty.  It may be used, redistributed and/or
  modified under the same terms as perl itself. ( Either the Artistic
  License or the GPL. )

  ======================================================================*/

#ifndef __ICALLANGBIND_H__
#define __ICALLANGBIND_H__

int* icallangbind_new_array(int size);
void icallangbind_free_array(int* array);
int icallangbind_access_array(int* array, int index);
icalproperty* icallangbind_get_property(icalcomponent *c, int n, const char* prop);
const char* icallangbind_get_property_val(icalproperty* p);
const char* icallangbind_get_parameter(icalproperty *p, const char* parameter);
icalcomponent* icallangbind_get_component(icalcomponent *c, const char* comp);

#endif /*__ICALLANGBIND_H__*/
